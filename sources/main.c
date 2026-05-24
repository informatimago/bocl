#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <sysexits.h>
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
#include "version.h"

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
static bool do_quiet=false;
static bool do_verbose=false;
static bool do_batch=false;
static bool do_print=false;
static bool do_dump=false;
static bool do_sigtrap=true;
static char* on_error_action=NULL;
typedef enum { on_error_debug, on_error_exit, on_error_abort, on_error_appease } on_error_action_t;
static on_error_action_t on_error=on_error_exit;
static string_list* initfiles=NULL;
static string_list* expressions_and_loads=NULL;
static char* package=NULL;


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
    {{"-Q","-q","--quiet","--silent",NULL}, &do_quiet,   none, {.name=NULL},                              "if --batch, also suppress printing of heralds, prompts."},
    {{"-b","--batch",NULL},                 &do_batch,   none, {.name=NULL},                              "exit when EOF on *STANDARD-INPUT*"},
    {{"--verbose",NULL},                    &do_verbose, none, {.name=NULL},                              "verbosity level: affects banner, *LOAD-VERBOSE*, *COMPILE-VERBOSE*"},
    {{"--print",NULL},                      &do_print,   none, {.name=NULL},                              "sets *LOAD-PRINT*, *COMPILE-PRINT*"},
    {{"--dump",NULL},                       &do_dump,    none, {.name=NULL},                              "dumps the heap"},/* TODO: put that in expressions_and_loads */
    {{"--no-sigtrap",NULL},                 &do_sigtrap, none, {.name=NULL},                              "obscure option for running under GDB"},
    {{"-on-error",NULL},                    NULL,        strg, {"action",{.string=&on_error_action}},     "action can be one of debug, exit, abort, appease"},
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
    initfile=strdup("~/.boclrc");
    on_error_action=strdup("exit");
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
                  else{
                      (*options[k].argument.variable.string)=argv[i];}
                  break;
              case elst:
                  string_list_push(argv[i],options[k].argument.variable.list);
                  break;
              case load:
                  string_list_push(generate_load_form(argv[i]),options[k].argument.variable.list);
                  break;
              case none: /* fall thru */
              default:
                  break;}}
        i++;}
    /* Some post processing: */
    if(0==strcmp(on_error_action,"debug")){
        on_error=on_error_debug;}
    else if(0==strcmp(on_error_action,"exit")){
        on_error=on_error_exit;}
    else if(0==strcmp(on_error_action,"abort")){
        on_error=on_error_abort;}
    else if(0==strcmp(on_error_action,"appease")){
        on_error=on_error_appease;}
    all_encoding=validate_encoding("all",all_encoding);
    terminal_encoding=validate_encoding("terminal",terminal_encoding);
    initfiles=string_list_nreverse(initfiles);
    expressions_and_loads=string_list_nreverse(expressions_and_loads);
    if(do_verbose && do_print){
        print_options();}}


static void print_banner(void){
    printf("\nWelcome to ");
    printf("BOCL Version %s\n",BOCL_VERSION);
    printf("Copyright 2021 Pascal J. Bourguignon\n");
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

int main(int argc,char** argv){
    pname=argv[0];
    int status=0;
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

    if(do_batch){
        /* TODO: update once packages are implemented: */
        bocl_read_eval_cstring("(DEFPARAMETER *BATCH*  T)");}
    else{
        /* TODO: update once packages are implemented: */
        bocl_read_eval_cstring("(DEFPARAMETER *BATCH*  NIL)");}

    define_parameter(S(*LOAD-VERBOSE*),S(NIL));
    define_parameter(S(*COMPILE-VERBOSE*),S(NIL));
    define_parameter(S(*LOAD-PRINT*),S(NIL));
    define_parameter(S(*COMPILE-PRINT*),S(NIL));

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

    if(!do_norc){
        initfiles=string_list_cons(initfile,initfiles);}
    {do_string_list(ifile,initfiles){
            bocl_load_cstring(ifile);}}
    string_list_free(initfiles);
    initfiles=NULL;

    {do_string_list(expression,expressions_and_loads){
            bocl_read_eval_cstring(expression);}}
    string_list_free(expressions_and_loads);
    expressions_and_loads=NULL;

    if(package){
        bocl_in_package(bocl_read_cstring(package));}

    if(do_repl){
        /* TODO: update once packages are implemented: */
        /* Ref result=bocl_read_eval_cstring("(BOCL:REPL)"); */
        Ref result=bocl_repl(variable_value(S(*STANDARD-INPUT*)),
                             variable_value(S(*STANDARD-OUTPUT*)));
        if(integerp(result)){
            status=(int)integer_value(result);}}

    if(do_wait){
        wait_for_termination(status);}

    return status;}
