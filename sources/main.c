#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/stat.h>
#include "string_list.h"
#include "kernel.h"
#include "bocl.h"
#include "macros.h"
#include "memory.h"
#include "boerror.h"
#include "strdup.h"
#include "stream.h"
#include "printer.h"
#include "reader.h"
#include "variable.h"
#include "eval.h"
#include "version.h"

/* Globals defined elsewhere, declared at file scope (rather than as nested
   externs inside the functions that use them). */
extern int    bocl_main_argc;     /* set from main()'s argc/argv; read by bocl.c */
extern char** bocl_main_argv;
extern bool   bocl_load_recover;  /* recover-and-continue load mode (bocl.c)      */
extern Ref    COMMON_LISP_Package, COMMON_LISP_USER_Package;   /* packages.c       */

static const char* source="http://gitlab.com/bocl/bocl/";
static const char* bocl_lisp="(defun repl () (catch 'quit (do () () (print (eval (read))))))";
static char* initfile=NULL;
static char* pname=NULL;

static bool do_help=false;
static bool do_version=false;
static bool do_repl=true;
static bool do_wait=false;
static bool do_quit=false;
static char* all_encoding;
static char* terminal_encoding;
static bool do_norc=false;
static bool do_noprelude=false;
static bool do_quiet=false;
static bool do_verbose=false;
static bool do_batch=false;
static bool do_print=false;
static bool do_dump=false;
static bool do_sigtrap=true;
static char* on_error_action=NULL;
typedef enum { on_error_debug, on_error_exit, on_error_abort, on_error_appease } on_error_action_t;
static on_error_action_t on_error=on_error_exit;
/* Whether --on-error was given explicitly on the command line.  When it was NOT,
   option/rc/file processing uses EXIT (fail hard, good for batch) but the
   interactive REPL uses DEBUG (issue on-error-default-setting). */
static bool on_error_explicit=false;
static string_list* initfiles=NULL;
static string_list* expressions_and_loads=NULL;
static char* package=NULL;


/* Candidate RC (init) file paths searched in order; the first existing file
   wins, EXCEPT that when a .fasl candidate is found, a more-recent .lisp sibling
   is preferred (see find_rc_file).  A leading ~/ is expanded to $HOME/.  This is
   the union of bocl's historical candidates (~/.bocl, ~/.config/bocl/init,
   ~/.common-lisp, ~/.config/common-lisp/init, with .lisp) and the documented
   per-stem {bare, .fasl, .lisp} set. */
static const char* rc_candidates[]={
    "~/.bocl",
    "~/.bocl.fasl",
    "~/.bocl.lisp",
    "~/.boclrc",
    "~/.boclrc.fasl",
    "~/.boclrc.lisp",
    "~/.config/bocl/init",
    "~/.config/bocl/init.fasl",
    "~/.config/bocl/init.lisp",
    "~/.common-lisp",
    "~/.common-lisp.fasl",
    "~/.common-lisp.lisp",
    "~/.config/common-lisp/init",
    "~/.config/common-lisp/init.fasl",
    "~/.config/common-lisp/init.lisp",
    NULL};

/* Return a malloc'd copy of path with a leading ~/ replaced by $HOME/. */
static char* expand_tilde(const char* path){
    if(path[0]!='~' || path[1]!='/'){
        return strdup(path);}
    const char* home=getenv("HOME");
    if(!home){ home=""; }
    size_t len=strlen(home)+strlen(path); /* path has '~', replaced by home */
    char* result=CHECK_POINTER(malloc(len+1));
    snprintf(result,len+1,"%s%s",home,path+1); /* skip the '~' */
    return result;}

/* mtime of PATH in seconds, or -1 if it cannot be stat'd. */
static long file_mtime(const char* path){
    struct stat st;
    if(stat(path,&st)!=0){ return -1; }
    return (long)st.st_mtime;}

/* Return the first readable RC file from rc_candidates, malloc'd, or NULL.
   When the match is a .fasl whose .lisp sibling exists and is more recent, the
   .lisp is returned instead (the fasl is stale). */
static char* find_rc_file(void){
    for(int i=0; rc_candidates[i]; i++){
        char* path=expand_tilde(rc_candidates[i]);
        if(access(path,R_OK)==0){
            size_t n=strlen(path);
            if(n>5 && strcmp(path+n-5,".fasl")==0){
                char* lisp=CHECK_POINTER(malloc(n+1));
                snprintf(lisp,n+1,"%.*s.lisp",(int)(n-5),path);
                if(access(lisp,R_OK)==0 && file_mtime(lisp)>file_mtime(path)){
                    free(path); return lisp; }
                free(lisp);}
            return path;}
        free(path);}
    return NULL;}

/* The argv[0] path of this process, captured in main(). */
static const char* program_path=NULL;

/* Locate bootstrap/prelude.lisp.  Search order:
   1. $BOCL_PRELUDE environment variable (explicit override).
   2. Paths relative to the executable directory (handles in-tree runs and
      typical installs: <exe-dir>/../bootstrap/prelude.lisp, etc.).
   3. Standard system install paths.
   Returns a malloc'd path or NULL if none is readable. */
static char* find_prelude(void){
    /* 1. Environment override. */
    const char* env=getenv("BOCL_PRELUDE");
    if(env && access(env,R_OK)==0){ return strdup(env); }

    /* 2. Relative to the executable. */
    if(program_path){
        /* Copy program_path and strip the trailing component to get the dir. */
        char dir[4096];
        snprintf(dir,sizeof(dir),"%s",program_path);
        char* slash=strrchr(dir,'/');
        if(slash){ *slash='\0'; } else { snprintf(dir,sizeof(dir),"."); }
        /* Candidate suffixes relative to the executable directory. */
        static const char* suffixes[]={
            "/../bootstrap/prelude.lisp",     /* in-tree:  sources/../bootstrap */
            "/bootstrap/prelude.lisp",        /* exe alongside bootstrap/      */
            "/../libexec/bocl/prelude.lisp",  /* install:  bin/../libexec/bocl */
            "/../share/bocl/prelude.lisp",    /* install:  bin/../share/bocl   */
            "/../lib/bocl/prelude.lisp",
            NULL};
        for(int i=0; suffixes[i]; i++){
            char path[4096];
            snprintf(path,sizeof(path),"%s%s",dir,suffixes[i]);
            if(access(path,R_OK)==0){ return strdup(path); }}
    }

    /* 3. Standard install paths. BOCL_PRELUDE_PATH is the location configured at
          build time ($(bocllibexecdir)/prelude.lisp); it is tried first so a custom
          $PREFIX install is found even when it is none of the hard-coded paths. */
    static const char* sys_paths[]={
#ifdef BOCL_PRELUDE_PATH
        BOCL_PRELUDE_PATH,
#endif
        "/opt/local/libexec/bocl/prelude.lisp",
        "/usr/local/libexec/bocl/prelude.lisp",
        "/usr/libexec/bocl/prelude.lisp",
        "/usr/local/share/bocl/prelude.lisp",
        "/usr/share/bocl/prelude.lisp",
        NULL};
    for(int i=0; sys_paths[i]; i++){
        if(access(sys_paths[i],R_OK)==0){ return strdup(sys_paths[i]); }}

    return NULL;}


#define PSOP(name) printf("%-30s = %s\n",#name,(name)?(name):"NULL")
#define PBOP(name) printf("%-30s = %s\n",#name,(name)?"true":"false")
#define PLOP(name) {int i=0;do_string_list(element,(name)){ \
            i++;printf("%-26s[%02d] = %s\n",#name,i,element);}}

static void print_options(void){
    PSOP(pname);
    PSOP(source);
    printf("bocl_lisp length               = %lu\n",strlen(bocl_lisp));
    PSOP(initfile);
    PBOP(do_help);
    PBOP(do_version);
    PBOP(do_repl);
    PBOP(do_wait);
    PBOP(do_quit);
    PSOP(all_encoding);
    PSOP(terminal_encoding);
    PBOP(do_norc);
    PBOP(do_quiet);
    PBOP(do_verbose);
    PBOP(do_batch);
    PBOP(do_print);
    PBOP(do_dump);
    PBOP(do_sigtrap);
    PSOP(on_error_action);
    PLOP(initfiles);
    PLOP(expressions_and_loads);
    PSOP(package);}

static struct {
    const char* variants[8]; /* we must set a maximum number of variants + 1 for NULL */
    bool* do_variable;       /* NULL <=> needs an argument: */
    enum { none, strg, elst, load } argument_type;
    struct {const char* name; union { char** string; string_list** list; } variable;} argument;
    const char* description;
}  options[]={
    /* Note: we've mixed and merged options from ccl and clisp; other variants may be suggested. */
    {{"-h","--help",NULL},                  &do_help,    none, {.name=NULL},                              "print this text"},
    {{"-V","--version",NULL},               &do_version, none, {.name=NULL},                              "print (LISP-IMPLEMENTATION-VERSION) and exit"},
    {{"-E",NULL},                           NULL,        strg, {"encoding",{.string=&all_encoding}},      "set encodings"},
    {{"-Eterminal",NULL},                   NULL,        strg, {"encoding",{.string=&terminal_encoding}}, "specify character encoding to use for *TERMINAL-IO*"},
    {{"--terminal-encoding",NULL},          NULL,        strg, {"encoding",{.string=&terminal_encoding}}, "specify character encoding to use for *TERMINAL-IO*"},
    {{"-n","-norc","--no-init",NULL},       &do_norc,    none, {.name=NULL},                              "suppress loading of default init file ~/.boclrc"},
    {{"--no-prelude",NULL},                 &do_noprelude, none, {.name=NULL},                            "suppress loading of bootstrap/prelude.lisp (bare kernel only)"},
    {{"-Q","-q","--quiet","--silent",NULL}, &do_quiet,   none, {.name=NULL},                              "if --batch, also suppress printing of heralds, prompts."},
    {{"-b","--batch",NULL},                 &do_batch,   none, {.name=NULL},                              "exit when EOF on *STANDARD-INPUT*"},
    {{"--verbose",NULL},                    &do_verbose, none, {.name=NULL},                              "verbosity level: affects banner, *LOAD-VERBOSE*, *COMPILE-VERBOSE*"},
    {{"--print",NULL},                      &do_print,   none, {.name=NULL},                              "sets *LOAD-PRINT*, *COMPILE-PRINT*"},
    {{"--dump",NULL},                       &do_dump,    none, {.name=NULL},                              "dumps the heap"},/* TODO: put that in expressions_and_loads */
    {{"--no-sigtrap",NULL},                 &do_sigtrap, none, {.name=NULL},                              "obscure option for running under GDB"},
    {{"--on-error","-on-error",NULL},       NULL,        strg, {"action",{.string=&on_error_action}},     "action can be one of debug, exit, abort, appease"},
    {{"-i","--init",NULL},                  NULL,        elst, {"file",{.list=&initfiles}},               "load initfile (can be repeated)"},
    {{"-x","-e","--eval",NULL},             NULL,        elst, {"form",{.list=&expressions_and_loads}},   "evaluate form (may need to quote form in shell)"},
    {{"-l","--load",NULL},                  NULL,        load, {"file",{.list=&expressions_and_loads}},   "load file"},
    {{"-p","--package",NULL},               NULL,        strg, {"package",{.string=&package}},            "start in the package"},
    {{"-repl","--repl",NULL},               &do_repl,    none, {.name=NULL},                              "enter the interactive read-eval-print loop when done (default)"},
    {{"-quit","--quit",NULL},               &do_quit,    none, {.name=NULL},                              "quit when argument processing is done"},
    {{"-w","--wait",NULL},                  &do_wait,    none, {.name=NULL},                              "wait for a keypress after program termination"},
    {{NULL},NULL,none,{.name=NULL},NULL}};

static void print_spaces(size_t nspaces){
    const char* buffer="                                        ";
    size_t buflen=strlen(buffer);
    while(buflen<nspaces){
        nspaces-=buflen;
        puts(buffer);}
    if(0<nspaces){
        printf("%s",(buffer+buflen-nspaces));}}

static void print_help(void){
    printf("%s usage:\n",pname);
    /* Compute max syntax length for nice alignment: */
    size_t max_syntax_length=0;
    for(int i=0;options[i].description;i++){
        int nvariants=0;
        size_t syntax_length=0;
        while(options[i].variants[nvariants]){
            nvariants++;}
        assert(0<nvariants);
        syntax_length+=strlen(options[i].variants[0]);
        for(int j=1;j<nvariants;j++){
            syntax_length+=1+strlen(options[i].variants[j]);}
        if(options[i].argument.name){
            syntax_length+=1+strlen(options[i].argument.name);}
        if(max_syntax_length<syntax_length){
            max_syntax_length=syntax_length;}}
    /* Compute display the syntax and description: */
    for(int i=0;options[i].description;i++){
        int nvariants=0;
        size_t syntax_length=0;
        while(options[i].variants[nvariants]){
            nvariants++;}
        printf("  %s",options[i].variants[0]);
        syntax_length+=strlen(options[i].variants[0]);
        for(int j=1;j<nvariants;j++){
            printf("|%s",options[i].variants[j]);
            syntax_length+=1+strlen(options[i].variants[j]);}
        if(options[i].argument.name){
            printf(" %s",options[i].argument.name);
            syntax_length+=1+strlen(options[i].argument.name);}
        print_spaces(2+max_syntax_length-syntax_length);
        printf("%s\n",options[i].description);}}

enum { no_such_option=-1 };
static int find_option(const char* variant){
    for(int i=0;options[i].description;i++){
        for(int j=0;options[i].variants[j];j++){
            if(strcmp(options[i].variants[j],variant)==0){
                return i;}}}
    return no_such_option;}

static char* generate_load_form(char* path){
    /* Wraps the path in a (load "") form, escapeing \\ and \" in path */
    size_t size=(1+strlen(path)*2+strlen("(load \"\")"));
    char* buffer=CHECK_POINTER(malloc(size));
    strcpy(buffer,"(load \"");
    size_t p=strlen(buffer);
    while(*path){
        switch(*path){
          case '\\': buffer[p++]='\\'; buffer[p++]='\\'; break;
          case '"':  buffer[p++]='\\'; buffer[p++]='"';  break;
          default:   buffer[p++]=*path; break; }
        path++;}
    strcpy(buffer+p,"\")");
    return buffer;}

static char* validate_encoding(const char* name,char* encoding){
    if((encoding==NULL)
       ||(0==strcasecmp(encoding,"ISO-8859-1"))
       ||(0==strcasecmp(encoding,"ISO_8859-1"))
       ||(0==strcasecmp(encoding,"LATIN1"))
       ||(0==strcasecmp(encoding,"ISO-LATIN-1"))
       ||(0==strcasecmp(encoding,"IBM819"))
       ||(0==strcasecmp(encoding,"CP819"))
       ||(0==strcasecmp(encoding,"CSISOLATIN1"))){
        return strdup("ISO-8859-1");}
    if((0==strcasecmp(encoding,"US-ASCII"))
       ||(0==strcasecmp(encoding,"ASCII"))){
        return strdup("US-ASCII");}
    fprintf(stderr,"ERROR: Invalid encoding name for %s: %s\n",name,encoding);
    fprintf(stderr,"Valid encoding names are US-ASCII and ISO-8859-1\n");
    exit(EX_CONFIG);}


static void process_options(int argc,char** argv){
    /* defaults: */
    all_encoding=NULL;
    terminal_encoding=NULL;
    initfile=NULL;
    on_error_action=NULL;   /* NULL = not given: EXIT for processing, DEBUG for REPL */
    package=strdup("COMMON-LISP-USER");
    pname=argv[0];
    int i=1;
    while(i<argc){
        int k=find_option(argv[i]);
        if(k==no_such_option){
            fprintf(stderr,"ERROR: Unknown option: %s\n",argv[i]);
            print_help();
            exit(EX_USAGE);}
        if(options[k].do_variable){
            *(options[k].do_variable)=true;
        }else if(options[k].argument_type!=none){
            /* we need an argument: */
            i++;
            if(argc<=i){
                fprintf(stderr,"ERROR: Missing a %s argument after %s\n",
                        options[k].argument.name,
                        argv[i-1]);
                print_help();
                exit(EX_USAGE);}
            switch(options[k].argument_type){
              case strg:
                  if(*options[k].argument.variable.string){
                      fprintf(stderr,"WARNING: argument for %s has already been given: %s ; changing it to %s.\n",
                              argv[i-1],
                              (*options[k].argument.variable.string),
                              argv[i]);}
                  /* Always apply the supplied value — including when a default
                     was pre-set (on_error_action defaults to "exit", package to
                     "COMMON-LISP-USER").  Previously the assignment lived only in
                     the else branch, so -on-error/-p were silently ignored
                     whenever a default existed. */
                  (*options[k].argument.variable.string)=argv[i];
                  break;
              case elst:
                  string_list_push(strdup(argv[i]),options[k].argument.variable.list);
                  break;
              case load:
                  string_list_push(generate_load_form(argv[i]),options[k].argument.variable.list);
                  break;
              case none: /* fall thru */
              default:
                  break;}}
        i++;}
    /* Some post processing: */
    if(on_error_action==NULL){
        /* Not given: EXIT for option/rc/file processing; the REPL default (DEBUG)
           is applied when BOCL:*ON-ERROR* is published, below. */
        on_error=on_error_exit;
        on_error_explicit=false;}
    else{
        on_error_explicit=true;
        if(0==strcmp(on_error_action,"debug")){
            on_error=on_error_debug;}
        else if(0==strcmp(on_error_action,"exit")){
            on_error=on_error_exit;}
        else if(0==strcmp(on_error_action,"abort")){
            on_error=on_error_abort;}
        else if(0==strcmp(on_error_action,"appease")){
            on_error=on_error_appease;}}
    all_encoding=validate_encoding("all",all_encoding);
    terminal_encoding=validate_encoding("terminal",terminal_encoding);
    initfiles=string_list_nreverse(initfiles);
    expressions_and_loads=string_list_nreverse(expressions_and_loads);
    if(do_verbose && do_print){
        print_options();}}


static void print_banner(void){
    printf("\nWelcome to ");
    printf("BOCL Version %s\n",BOCL_VERSION);
    printf("Copyright 2021-2026 Pascal J. Bourguignon\n");
    printf("Licensed under AGPL; source codes available at:\n");
    printf("%s\n\n",source);}

static void print_version(void){
    printf("%s\n",BOCL_VERSION);}

static void wait_for_termination(int status){
    printf("Enter RET to terminate with status %d.\n",status);
    char buffer[80];
    fgets(buffer,sizeof(buffer)-1,stdin);}

_Noreturn void invoke_debugger(Ref error);
bool error_handler(Ref error);
_Noreturn void invoke_debugger(unused Ref error){
    abort();}

bool error_handler(Ref error){
    report_error(error);
    switch(on_error){
      case on_error_debug:
          invoke_debugger(error);
      case on_error_exit:
          exit(EX_DATAERR);
      case on_error_appease:
          return true;
      case on_error_abort: /* fall thru */
      default:
          abort();}}

/* The session body, run under the BOCL:QUIT top-level catch (see main()):
   init/rc files, BOCL:*INIT-HOOK*, then the -x/-l forms, IN-PACKAGE and the
   REPL.  Returns the exit status as an integer Ref. */
static Ref run_session(void* ctx){
    (void)ctx;
    int status=0;
    if(!do_norc){
        char* rcfile=find_rc_file();
        if(rcfile){
            /* prepend so it loads before any -i files */
            initfiles=string_list_cons(rcfile,initfiles);}}
    {do_string_list(ifile,initfiles){
            bocl_load_cstring(ifile);}}
    string_list_free(initfiles);
    initfiles=NULL;

    /* BOCL:*INIT-HOOK* — after the init/rc files, before the main task. */
    bocl_read_eval_cstring("(BOCL::%RUN-INIT-HOOKS)");

    {do_string_list(expression,expressions_and_loads){
            bocl_read_eval_cstring(expression);}}
    string_list_free(expressions_and_loads);
    expressions_and_loads=NULL;

    if(package){
        bocl_in_package(bocl_read_cstring(package));}

    if(do_repl){
        /* When a Lisp-level REPL (e.g. BOCL:REPL) is available, call it
           instead.  For now, use the C bocl_repl() entry point directly. */
        Ref result=bocl_repl(variable_value(S(*STANDARD-INPUT*)),
                             variable_value(S(*STANDARD-OUTPUT*)));
        if(integerp(result)){
            status=(int)integer_value(result);}}

    return integer_from_word(status);}

int main(int argc,char** argv){
    /* Line-buffer stdout (and unbuffer stderr) so output is visible promptly
       when redirected to a file or pipe (CI logs, tee), not just on a tty. */
    setvbuf(stdout,NULL,_IOLBF,0);
    setvbuf(stderr,NULL,_IONBF,0);
    pname=argv[0];
    program_path=argv[0];
    /* Capture argv for BOCL's %COMMAND-LINE-ARGUMENTS (UIOP raw-command-line-args). */
    {   bocl_main_argc=argc; bocl_main_argv=argv; }
    int status=0;
    /* Capture a high address near the base of the C stack for the eval
       stack-overflow guard (main's frame is close to the stack's start). */
    {   char stack_anchor;
        eval_init_stack_guard(&stack_anchor); }
    kernel_initialize();
    process_options(argc,argv);

    if(do_version){
        print_version();
        exit(0);}

    if(do_help){
        print_help();
        exit(0);}

    if(!do_quiet && !do_batch){
        print_banner();}

    if(do_dump){
        memory_dump(stdout);}

    define_parameter(S(*BATCH*), do_batch ? T_Symbol : NIL_Symbol);

    define_parameter(S(*LOAD-VERBOSE*),S(NIL));
    define_parameter(S(*COMPILE-VERBOSE*),S(NIL));
    define_parameter(S(*LOAD-PRINT*),S(NIL));
    define_parameter(S(*COMPILE-PRINT*),S(NIL));

    /* Load the Lisp prelude (bootstrap/prelude.lisp) so the full Common Lisp
       surface (macros like OR, AND, LOOP, CLOS, etc.) is available before any
       rc file, init file, user expression, or loaded file runs.  --no-prelude
       suppresses this for tests that exercise only the bare kernel.

       This MUST happen BEFORE enabling LOAD-PRINT and LOAD-VERBOSE below: the
       prelude defines the printer's special variables (PRINT-LENGTH,
       PRINT-LEVEL, ...) partway through, so printing the result of each
       loaded form during the bootstrap would reference them before they are
       bound.  --print and --verbose therefore apply only to user files loaded
       afterwards, never to the bootstrap itself. */
    if(!do_noprelude){
        /* The prelude defines the standard CL symbols, so load it with *PACKAGE*
           bound to COMMON-LISP: every symbol it interns then lands in CL (where
           the reader's bare symbols already go), instead of in CL-USER.  After
           the bootstrap, user code defaults to COMMON-LISP-USER per the standard. */
        set_symbol_value(S(*PACKAGE*), COMMON_LISP_Package);
        char* prelude=find_prelude();
        if(prelude){
            /* FIRST make the standard CL symbols external (generated
               cl-exports.lisp, sibling of prelude.lisp): it uses only QUOTE and
               the %EXPORT-STRINGS primitive, so it runs before the prelude and
               the prelude's cl:FOO references then resolve to external symbols. */
            char* p=strstr(prelude,"prelude.lisp");
            if(p){
                size_t base=(size_t)(p-prelude);
                char* exports=malloc(base+sizeof("cl-exports.lisp"));
                memcpy(exports,prelude,base);
                strcpy(exports+base,"cl-exports.lisp");
                bocl_load_cstring(exports);
                free(exports);}
            bocl_load_cstring(prelude);
            free(prelude);
        } else if(!do_quiet){
            fprintf(stderr,
                    "bocl: warning: could not find bootstrap/prelude.lisp; "
                    "running with the bare kernel only.\n"
                    "      Set BOCL_PRELUDE or use --no-prelude to silence.\n");}
        set_symbol_value(S(*PACKAGE*), COMMON_LISP_USER_Package);}

    if(do_verbose){
        define_parameter(S(*LOAD-VERBOSE*),S(T));
        define_parameter(S(*COMPILE-VERBOSE*),S(T));
        bocl_read_eval_cstring("(SETQ *LOAD-VERBOSE*    T)");
        bocl_read_eval_cstring("(SETQ *COMPILE-VERBOSE* T)");}

    if(do_print){
        define_parameter(S(*LOAD-PRINT*),S(T));
        define_parameter(S(*COMPILE-PRINT*),S(T));
        bocl_read_eval_cstring("(SETQ *LOAD-PRINT*      T)");
        bocl_read_eval_cstring("(SETQ *COMPILE-PRINT*   T)");}

    /* The prelude has now loaded.  Under -on-error appease, enable per-form
       recovery so user files (rc / init / -l, and everything they LOAD) skip
       individual bad forms instead of aborting — this lets large suites such as
       the ANSI tests load as much as possible and still reach their report. */
    {   if(on_error==on_error_appease){ bocl_load_recover=true; }}

    /* Publish the --on-error action to Lisp as BOCL:*ON-ERROR* so the REPL's
       %REPL-EVAL wrapper can decide whether an unhandled error enters the
       debugger (:debug), is reported (:appease/:abort) or exits (:exit). */
    if(!do_noprelude){
        /* When --on-error was NOT given, the interactive REPL defaults to DEBUG
           (while the option/rc/file processing above ran under EXIT).  When it WAS
           given, the REPL uses that action too. */
        const char* k = (!on_error_explicit)            ? ":DEBUG"
                      : (on_error==on_error_debug)      ? ":DEBUG"
                      : (on_error==on_error_exit)       ? ":EXIT"
                      : (on_error==on_error_abort)      ? ":ABORT"
                      :                                   ":APPEASE";
        char buf[64];
        snprintf(buf,sizeof(buf),"(SETQ BOCL:*ON-ERROR* %s)",k);
        bocl_read_eval_cstring(buf);}

    /* The session — init/rc files, BOCL:*INIT-HOOK*, then -x/-l forms, IN-PACKAGE
       and the REPL — runs inside a top-level CATCH for BOCL:QUIT, so QUIT (from
       an init hook, a loaded form, or the REPL) performs a proper non-local exit
       that runs pending UNWIND-PROTECT clean-ups.  *QUIT-HOOK* runs afterwards. */
    {   Ref r=bocl_run_with_toplevel_catch(run_session,NULL);
        if(integerp(r)){ status=(int)integer_value(r); }}

    /* BOCL:*QUIT-HOOK* — best-effort clean-up, then flush, then exit. */
    {   char buf[64];
        snprintf(buf,sizeof(buf),"(BOCL::%%RUN-QUIT-HOOKS %d)",status);
        bocl_read_eval_cstring(buf);}
    fflush(stdout);
    fflush(stderr);

    if(do_wait){
        wait_for_termination(status);}

    return status;}
