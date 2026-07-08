/* realpath() (used below for *LOAD-TRUENAME*) is gated behind _XOPEN_SOURCE on
   musl — not _POSIX_C_SOURCE — so without this it is only implicitly declared
   there, and an implicit int return truncates its char* on LP64. glibc under
   -std=c17 hides it the same way (__STRICT_ANSI__), and gcc 14 promotes the
   implicit declaration to a hard error. Must precede every include. */
#ifndef _XOPEN_SOURCE
#  define _XOPEN_SOURCE 700
#endif
#include "bocl.h"
#include "memory.h"
#include "boerror.h"
#include "kernel.h"
#include "stream.h"
#include "reader.h"
#include "printer.h"
#include "variable.h"
#include "macros.h"
#include "eval.h"
#include "packages.h"
#include <time.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>   /* AF_UNSPEC for LONG-SITE-NAME getaddrinfo */
#include <netdb.h>        /* getaddrinfo / AI_CANONNAME (FQDN) */
#include <dirent.h>       /* opendir/readdir for %LIST-DIRECTORY */
#include <sys/stat.h>     /* stat/S_ISDIR for %DIRECTORY-P */


Ref bocl_eval(Ref sexp){
    MultipleValues results={0};
    return evale(sexp,eval_global_environment(),&results);
}

Ref bocl_read_cstring(const char* text){
    Ref stream=stream_input_string(string_new_cstring(text),I(0),NIL_Symbol);
    Ref result=kernel_read(stream,NIL_Symbol,NIL_Symbol,NIL_Symbol);
    stream_close(stream);
    return result;}

Ref bocl_read_eval_cstring(const char* text){
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    Ref sexp=bocl_read_cstring(text);
    Ref result=bocl_eval(sexp);
    stream_finish_output(out);
    return result;}

/* Evaluate one already-parsed top-level form through the Lisp %REPL-EVAL
   wrapper, so BOCL:*ON-ERROR* governs an unhandled SERIOUS-CONDITION
   (debug / appease / abort / exit) exactly as in the interactive REPL —
   instead of the process being killed.  On the bare kernel (prelude not
   loaded, %REPL-EVAL unavailable) it falls back to a direct EVAL.  This is
   the pre-REPL counterpart of what bocl_repl() does per form, used by the
   launcher for rc/init files and -l/-x forms (issue on-error-default-setting). */
Ref bocl_toplevel_eval(Ref sexp){
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    MultipleValues results={0};
    {   Ref re=symbol_intern_cstring("%REPL-EVAL");
        if(symbol_fboundp(re)){
            kernel_apply(symbol_function(re),cons(sexp,NIL_Symbol),&results);
        } else {
            evale(sexp,eval_global_environment(),&results);}}
    stream_finish_output(out);
    return (results.count>0)?results.values[0]:NIL_Symbol;}

/* Read TEXT and evaluate it through the %REPL-EVAL wrapper (see above). */
Ref bocl_toplevel_eval_cstring(const char* text){
    return bocl_toplevel_eval(bocl_read_cstring(text));}

/* Emit the REPL prompt.  If BOCL:*PROMPT-FUNCTION* is a non-NIL function
   designator, call (funcall *prompt-function* out) to produce the prompt; else
   write the default implementation-defined prompt. */
static void bocl_emit_prompt(Ref out){
    if(BOCL_Package!=NULL && typep(BOCL_Package,PACKAGE_Class)){
        Ref present=BOCL_Package->slots->package.present;
        Ref sym=find_symbol(string_new_cstring("*PROMPT-FUNCTION*"),present);
        if(sym){
            Ref pf=symbol_value(sym);
            Ref fn=NIL_Symbol;
            if(symbolp(pf)){ if(symbol_fboundp(pf)){ fn=symbol_function(pf); } }
            else if(!nullp(pf)){ fn=pf; }   /* a function / closure object */
            if(!nullp(fn)){
                MultipleValues mv;
                kernel_apply(fn,cons(out,NIL_Symbol),&mv);
                return;}}}
    princ(T("bocl> "),out);}

Ref bocl_repl(Ref inp,Ref out){
    terpri(out);
    /* REPL history variables (CLHS 25.1.1).  (Interned here with explicit names
       because S(//) / S(///) would be read as C line comments.)
       -        : the form currently being evaluated
       + ++ +++   : the last three forms evaluated
       * ** ***   : the primary values of the last three evaluations
       / // ///   : the value LISTS of the last three evaluations                  */
    Ref vd=S(-);
    Ref vp1=S(+),  vp2=S(++),  vp3=S(+++);
    Ref vs1=S(*),  vs2=S(**),  vs3=S(***);
    Ref vl1=S(/),  vl2=symbol_intern_cstring("//"), vl3=symbol_intern_cstring("///");
    set_symbol_value(vd,NIL_Symbol);
    set_symbol_value(vp1,NIL_Symbol); set_symbol_value(vp2,NIL_Symbol); set_symbol_value(vp3,NIL_Symbol);
    set_symbol_value(vs1,NIL_Symbol); set_symbol_value(vs2,NIL_Symbol); set_symbol_value(vs3,NIL_Symbol);
    set_symbol_value(vl1,NIL_Symbol); set_symbol_value(vl2,NIL_Symbol); set_symbol_value(vl3,NIL_Symbol);
    while(true){
        /* The prompt is interaction chrome — keep it OFF the dribble file.  When
           a DRIBBLE is active, *STANDARD-OUTPUT* (and the captured OUT) is the
           teeing broadcast, so emit the prompt to the saved real output instead. */
        Ref prompt_out=out;
        {   Ref so=find_symbol(string_new_cstring("*DRIBBLE-SAVED-OUTPUT*"),
                               BOCL_Package->slots->package.present);
            if(so && symbol_boundp(so) && !nullp(symbol_value(so))){
                prompt_out=symbol_value(so);}}
        bocl_emit_prompt(prompt_out);
        stream_finish_output(prompt_out);
        Ref sexp=kernel_read(inp,NIL_Symbol,inp,NIL_Symbol);
        if((sexp==inp)||equal(sexp,list(S(QUIT),NULL))){
            return I(0);}
        /* When dribbling, record the input form verbatim (no prefix) to the
           dribble file before evaluating it (issue dribble).  %DRIBBLE-NOTE-INPUT
           no-ops when not dribbling. */
        {   Ref ni=find_symbol(string_new_cstring("%DRIBBLE-NOTE-INPUT"),
                               BOCL_Package->slots->package.present);
            if(ni && symbol_fboundp(ni)){
                MultipleValues nmv={0};
                kernel_apply(symbol_function(ni),
                             cons(prin1_to_string(sexp),NIL_Symbol),&nmv);}}
        /* Shift the form history: + gets the form evaluated last time (old -). */
        set_symbol_value(vp3,symbol_value(vp2));
        set_symbol_value(vp2,symbol_value(vp1));
        set_symbol_value(vp1,symbol_value(vd));
        set_symbol_value(vd,sexp);
        /* Evaluate, capturing ALL values (for / // ///).  When the prelude is
           loaded, route through the Lisp %REPL-EVAL wrapper so an unhandled error
           enters the debugger / is reported per BOCL:*ON-ERROR* instead of killing
           the whole process (issue bocl-or-bash-debugger).  Fall back to a direct
           EVAL on the bare kernel (no prelude). */
        MultipleValues results={0};
        {   Ref re=symbol_intern_cstring("%REPL-EVAL");
            if(symbol_fboundp(re)){
                kernel_apply(symbol_function(re),cons(sexp,NIL_Symbol),&results);
            } else {
                evale(sexp,eval_global_environment(),&results);}}
        Ref primary=(results.count>0)?results.values[0]:NIL_Symbol;
        Ref vals=NIL_Symbol;
        for(uword i=results.count;i-->0;){ vals=cons(results.values[i],vals); }
        /* Shift the value history. */
        set_symbol_value(vs3,symbol_value(vs2));
        set_symbol_value(vs2,symbol_value(vs1));
        set_symbol_value(vs1,primary);
        set_symbol_value(vl3,symbol_value(vl2));
        set_symbol_value(vl2,symbol_value(vl1));
        set_symbol_value(vl1,vals);
        /* Print the result to the CURRENT *STANDARD-OUTPUT* (not the captured
           OUT) so a DRIBBLE in effect tees the result echo to the dribble file;
           the prompt stays on OUT so it is not recorded. */
        {   Ref cur_out=variable_value(S(*STANDARD-OUTPUT*));
            print(primary,cur_out);
            terpri(cur_out);}}}

Ref bocl_read_eval_loop_cstring(const char* text){
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    Ref inp=stream_input_string(string_new_cstring(text),I(0),NIL_Symbol);
    Ref result=bocl_repl(inp,out);
    stream_close(inp);
    return result;}

Ref bocl_in_package(Ref package_designator){
    Ref pkg = package_find(package_designator);
    if(nullp(pkg)){
        PACKAGE_ERROR("IN-PACKAGE: no such package %s",
              string_cstring(prin1_to_string(package_designator)));}
    set_symbol_value(S(*PACKAGE*), pkg);
    return pkg;}

/* When true, each top-level form is evaluated through the Lisp wrapper
   %LOAD-EVAL-TOPLEVEL-FORM, which catches errors so a single bad form does not
   abort the whole file.  main.c enables this only after the prelude has loaded
   and only under -on-error appease — never during the bootstrap. */
bool bocl_load_recover = false;

Ref bocl_load_cstring(const char* path){
    Ref out = variable_value(S(*STANDARD-OUTPUT*));

    if(getenv("BOCL_DBG_FILE")){ fprintf(stderr,">>> loading %s\n",path); fflush(stderr); }
    bool do_verbose = is_true(variable_value(S(*LOAD-VERBOSE*)));
    bool do_print   = is_true(variable_value(S(*LOAD-PRINT*)));
    /* Build + root the pathname BEFORE stream_open: the K(...)/S(...) interns in
       the argument list allocate (first time a keyword is interned) and a GC
       there would free the just-built pathname waiting as a pending argument.
       Keep the pool open for the whole load so inp (the stream), its pathname and
       the saved LOAD-PATHNAME and LOAD-TRUENAME values, all reachable only via C
       locals, survive every GC the read/eval loop below triggers. */
    uword load_pool=open_live_pool();
    Ref pn_in=physical_pathname_new_cstring(path);
    add_to_live_pool(pn_in);
    Ref inp = stream_open(pn_in,
                             K(INPUT),
                             S(CHARACTER),
                             NIL_Symbol,        /* if-exists: irrelevant for :input */
                             K(ERROR),          /* if-does-not-exist */
                             K(DEFAULT));
    add_to_live_pool(inp);
    /* Bind *LOAD-PATHNAME* / *LOAD-TRUENAME* for the duration of this load.
       EVAL-WHEN :LOAD-TOPLEVEL checks these (in eval.c). */
    Ref pn_obj = stream_pathname(inp);
    add_to_live_pool(pn_obj);
    /* *LOAD-TRUENAME* must be the absolute truename (CLHS) — code that loads
       sibling files relative to its own directory (e.g. the ansi-test harness's
       *aux-dir*) breaks if it is left relative. */
    Ref tn_obj = pn_obj;
    {   char abs[4096];
        if(realpath(path, abs)){
            tn_obj = physical_pathname_new_cstring(abs);
            add_to_live_pool(tn_obj);}}
    Ref old_load_pn = variable_value(S(*LOAD-PATHNAME*));
    Ref old_load_tn = variable_value(S(*LOAD-TRUENAME*));
    add_to_live_pool(old_load_pn);
    add_to_live_pool(old_load_tn);
    set_symbol_value(S(*LOAD-PATHNAME*), pn_obj);
    set_symbol_value(S(*LOAD-TRUENAME*), tn_obj);

    if(do_verbose){
        princ(T(";; Loading "),out);
        princ(pn_obj,out);
        terpri(out);
        stream_finish_output(out);}
    /* kernel_read on EOF returns the stream itself (we pass `inp` as the
       eof-value), so the loop test `sexp != inp` terminates at EOF.

       Each iteration runs inside its own live-pool frame so that the form and
       everything it allocates transiently are reclaimed before the next form is
       read.  Without this, every top-level form of every (possibly deeply
       nested) LOAD piles up in one ever-growing live pool, making each GC scan
       O(total-forms) and turning a large load — e.g. the ANSI suite — into an
       O(n^2) crawl.  Definitions survive trimming because they are rooted via
       their symbols, not the live pool. */
    int dbgf=0;
    for(;;){
        uword form_pool=open_live_pool();
        Ref sexp=kernel_read(inp,NIL_Symbol,inp,NIL_Symbol);
        if(sexp==inp){ close_live_pool(form_pool); break; }
        add_to_live_pool(sexp);
        if(do_print){
            prin1(sexp,out);
            terpri(out);
            stream_finish_output(out);}
        if(getenv("BOCL_DBG_LOAD")){
            fprintf(stderr,"F%d:",dbgf++);
            if(consp(sexp)&&symbolp(car(sexp))){ fputs(string_cstring(symbol_name(car(sexp))),stderr);
                if(consp(cdr(sexp))&&symbolp(cadr(sexp))){ fputc(' ',stderr); fputs(string_cstring(symbol_name(cadr(sexp))),stderr);} }
            fputc('\n',stderr); fflush(stderr); }
        if(bocl_load_recover){
            /* Route through %LOAD-EVAL-TOPLEVEL-FORM so an error in this form is
               caught and reported rather than aborting the rest of the file. */
            Ref wrapper=symbol_function(symbol_intern_cstring("%LOAD-EVAL-TOPLEVEL-FORM"));
            if(functionp(wrapper) || built_in_function_p(wrapper) || closurep(wrapper)){
                MultipleValues r={0};
                kernel_apply(wrapper, cons(sexp,NIL_Symbol), &r);
            } else {
                bocl_eval(sexp);}
        } else {
            bocl_eval(sexp);}
        close_live_pool(form_pool);}
    stream_close(inp);
    if(do_verbose){
        princ(T(";; Loaded "),out);
        princ(pn_obj,out);
        terpri(out);
        stream_finish_output(out);}

    /* Restore *LOAD-PATHNAME* bindings. */
    set_symbol_value(S(*LOAD-PATHNAME*), old_load_pn);
    set_symbol_value(S(*LOAD-TRUENAME*), old_load_tn);

    close_live_pool(load_pool);
    return T_Symbol;}


/* Load cl-exports.lisp (sibling of prelude.lisp) followed by prelude.lisp,
   with *PACKAGE* bound to COMMON-LISP during loading (mirroring main.c).
   Tests call this instead of bocl_load_cstring() for the prelude so that
   (a) the COMMON-LISP external table is populated, and
   (b) S(DEFMACRO) etc. in eval.c match the symbols the reader interns. */
Ref bocl_load_prelude(const char* prelude_path){
    /* Set *PACKAGE* = COMMON-LISP for the duration of the bootstrap so the
       reader interns bare symbols into the CL package, matching eval.c's
       S(...) caches.  Restore to CL-USER afterwards (as main.c does). */
    set_symbol_value(S(*PACKAGE*), COMMON_LISP_Package);
    /* Derive the cl-exports path: replace "prelude.lisp" with
       "cl-exports.lisp" at the end of the path. */
    const char* needle = "prelude.lisp";
    const char* p = strstr(prelude_path, needle);
    if(p){
        size_t base=(size_t)(p-prelude_path);
        char* exports=(char*)malloc(base+sizeof("cl-exports.lisp"));
        memcpy(exports,prelude_path,base);
        strcpy(exports+base,"cl-exports.lisp");
        bocl_load_cstring(exports);
        free(exports);}
    Ref result = bocl_load_cstring(prelude_path);
    set_symbol_value(S(*PACKAGE*), COMMON_LISP_USER_Package);
    return result;}

/* Kernel-visible (LOAD filespec) — minimal: no &key :verbose / :print /
   :if-does-not-exist yet. Those go on the Lisp side. */
static void kernel_LOAD(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref filespec=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    /* Accept a STRING (treat as namestring) or a PHYSICAL-PATHNAME. */
    char* path;
    if(typep(filespec,STRING_Class)){
        path=string_cstring(filespec);
    } else if(typep(filespec,PHYSICAL_PATHNAME_Class)){
        path=string_cstring(namestring(filespec));
    } else {
        TYPE_ERROR(filespec,STRING_Class);}
    Ref result=bocl_load_cstring(path);
    free(path);
    results->count=1;
    results->values[0]=result;
    close_live_pool(pool);}

/* (%LIST-DIRECTORY path) → list of the entry NAMES (strings) directly in the
   directory PATH (a string / pathname namestring), excluding "." and "..".
   Returns NIL if PATH cannot be opened.  Names only — the caller joins them with
   PATH as needed.  Used by the module loader to scan libexec/bocl/modules/. */
static void kernel_LIST_DIRECTORY(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref dirspec=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    char* path;
    if(typep(dirspec,STRING_Class)){
        path=string_cstring(dirspec);
    } else if(typep(dirspec,PHYSICAL_PATHNAME_Class)){
        path=string_cstring(namestring(dirspec));
    } else {
        TYPE_ERROR(dirspec,STRING_Class);}
    Ref out=NIL_Symbol;
    DIR* d=opendir(path);
    if(d){
        struct dirent* e;
        while((e=readdir(d))!=NULL){
            if(strcmp(e->d_name,".")==0 || strcmp(e->d_name,"..")==0){ continue; }
            Ref name=string_new_cstring(e->d_name);
            add_to_live_pool(name);
            push(name,&out);
            add_to_live_pool(out);}
        closedir(d);}
    free(path);
    results->count=1;
    results->values[0]=out;
    close_live_pool(pool);}

/* Fast DIRECTORY: run the whole recursive glob walk in C.  The Lisp %GLOB did
   this with a CHAR-by-CHAR %WILD-MATCH, a %DIRECTORY-P stat() syscall per entry,
   a %LIST-DIRECTORY per level, and an O(n^2) APPEND — every step an interpreter
   funcall (~150us), so DIRECTORY of a real tree took tens of seconds.  Here the
   walk is pure C: opendir/readdir once per level, readdir's d_type instead of a
   per-entry stat (stat only for symlinks / DT_UNKNOWN), and a byte-wise glob
   matcher.  Matches are collected into a C string vector during the walk (NO
   Lisp allocation, hence no GC and no longjmp risk mid-walk); the Lisp result
   list is built once at the end. */

/* Glob match: '*' matches any (possibly empty) run, '?' one char — same as the
   old %WILD-MATCH (no char classes / escapes). */
static bool bocl_glob_match(const char* pat, const char* str){
    const char* star=NULL; const char* ss=NULL;
    while(*str){
        if(*pat=='?' || *pat==*str){ pat++; str++; }
        else if(*pat=='*'){ star=pat++; ss=str; }
        else if(star){ pat=star+1; str=++ss; }
        else{ return false; }}
    while(*pat=='*'){ pat++; }
    return *pat=='\0';}

/* dir + name (+ optional trailing '/'); caller frees. */
static char* bocl_path_join(const char* dir, const char* name, bool add_slash){
    size_t ld=strlen(dir), ln=strlen(name);
    bool need_sep = (ld>0 && dir[ld-1]!='/');
    char* p=(char*)malloc(ld + (need_sep?1:0) + ln + (add_slash?1:0) + 1);
    size_t k=0;
    memcpy(p+k,dir,ld); k+=ld;
    if(need_sep){ p[k++]='/'; }
    memcpy(p+k,name,ln); k+=ln;
    if(add_slash){ p[k++]='/'; }
    p[k]='\0';
    return p;}

typedef struct { char** items; size_t count, cap; } bocl_strvec;
static void bocl_sv_push(bocl_strvec* v, char* owned){
    if(v->count==v->cap){ v->cap = v->cap ? v->cap*2 : 64;
        v->items=(char**)realloc(v->items, v->cap*sizeof(char*)); }
    v->items[v->count++]=owned;}

static bool bocl_is_dir(const char* p){
    struct stat st; return stat(p,&st)==0 && S_ISDIR(st.st_mode);}

static bool bocl_entry_is_dir(unused struct dirent* e, const char* full){
#if defined(DT_DIR) && defined(DT_UNKNOWN)
    if(e->d_type==DT_DIR){ return true; }
    if(e->d_type!=DT_UNKNOWN && e->d_type!=DT_LNK){ return false; }
#endif
    struct stat st;
    return stat(full,&st)==0 && S_ISDIR(st.st_mode);}

/* DIR ends with '/'. COMPS[ci..nc) are the remaining glob components.  When
   DIRS_ONLY, a terminal match is kept only if it is a directory and is emitted in
   directory-pathname form (with a trailing '/') — this is CL's trailing-slash
   pattern semantics: a DIRECTORY pattern ending in slash-star-slash returns the
   sub-directories only, which also lets UIOP's SUBDIRECTORIES avoid a per-entry
   stat in Lisp. */
static void bocl_glob_walk(const char* dir, char** comps, int nc, int ci, bocl_strvec* out, bool dirs_only){
    if(ci>=nc){ return; }
    const char* comp=comps[ci];
    bool last=(ci==nc-1);
    if(strcmp(comp,"**")==0){
        bocl_glob_walk(dir,comps,nc,ci+1,out,dirs_only);   /* zero levels: match rest here */
        DIR* d=opendir(dir);
        if(d){ struct dirent* e;
            while((e=readdir(d))!=NULL){
                if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")){ continue; }
                char* sub=bocl_path_join(dir,e->d_name,true);
                if(bocl_entry_is_dir(e,sub)){ bocl_glob_walk(sub,comps,nc,ci,out,dirs_only); }  /* ** stays */
                free(sub);}
            closedir(d);}}
    else if(strchr(comp,'*')||strchr(comp,'?')){
        DIR* d=opendir(dir);
        if(d){ struct dirent* e;
            while((e=readdir(d))!=NULL){
                if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")){ continue; }
                if(bocl_glob_match(comp,e->d_name)){
                    if(last){
                        if(dirs_only){
                            char* sub=bocl_path_join(dir,e->d_name,true);
                            if(bocl_entry_is_dir(e,sub)){ bocl_sv_push(out,sub); } else { free(sub); }}
                        else{ bocl_sv_push(out, bocl_path_join(dir,e->d_name,false)); }}
                    else{ char* sub=bocl_path_join(dir,e->d_name,true);
                          if(bocl_entry_is_dir(e,sub)){ bocl_glob_walk(sub,comps,nc,ci+1,out,dirs_only); }
                          free(sub);}}}
            closedir(d);}}
    else{
        if(last){
            if(dirs_only){
                char* sub=bocl_path_join(dir,comp,true);
                if(bocl_is_dir(sub)){ bocl_sv_push(out,sub); } else { free(sub); }}
            else{
                char* full=bocl_path_join(dir,comp,false);
                struct stat st;
                if(stat(full,&st)==0){ bocl_sv_push(out, full); } else { free(full); }}}
        else{
            char* sub=bocl_path_join(dir,comp,true);
            if(bocl_is_dir(sub)){ bocl_glob_walk(sub,comps,nc,ci+1,out,dirs_only); }
            free(sub);}}}

/* (%DIRECTORY-GLOB pattern) -> list of matching path strings.  PATTERN is the
   whole glob namestring; ALL parsing is done here in C — splitting on '/', the
   absolute-vs-CWD-relative base (via getcwd), and the trailing-slash "directories
   only" flag — so DIRECTORY's per-call Lisp cost is just one MAPCAR over the
   results.  (Splitting in Lisp with %SPLIT-PATH was CHAR-by-CHAR and, called once
   per directory, dominated a recursive SUBDIRECTORIES scan.) */
static void kernel_DIRECTORY_GLOB(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref patspec=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    check_class(patspec,STRING_Class);
    char* pat=string_cstring(patspec);
    size_t plen=strlen(pat);
    bool dirs_only=(plen>0 && pat[plen-1]=='/');
    bool abs=(plen>0 && pat[0]=='/');
    char cwdbuf[4096];
    const char* base;
    if(abs){ base="/"; }
    else if(getcwd(cwdbuf,sizeof(cwdbuf))){ base=cwdbuf; }
    else{ base="."; }
    /* Split PAT on '/' into non-empty components (in place). */
    int cap=8, nc=0;
    char** comps=(char**)malloc((size_t)cap*sizeof(char*));
    { size_t s=0;
      for(size_t i=0;i<=plen;i++){
          if(i==plen || pat[i]=='/'){
              if(i>s){
                  if(nc==cap){ cap*=2; comps=(char**)realloc(comps,(size_t)cap*sizeof(char*)); }
                  comps[nc]=(char*)malloc(i-s+1);
                  memcpy(comps[nc],pat+s,i-s); comps[nc][i-s]='\0'; nc++; }
              s=i+1; } } }
    bocl_strvec out={0,0,0};
    if(nc>0){ bocl_glob_walk(base, comps, nc, 0, &out, dirs_only); }
    /* Build the Lisp result list (reverse iteration -> forward order). */
    Ref lst=NIL_Symbol;
    for(size_t i=out.count; i-->0; ){
        Ref s=string_new_cstring(out.items[i]);
        add_to_live_pool(s);
        push(s,&lst);
        add_to_live_pool(lst);
        free(out.items[i]);}
    free(out.items);
    free(pat);
    for(int i=0;i<nc;i++){ free(comps[i]); }
    free(comps);
    results->count=1; results->values[0]=lst;
    close_live_pool(pool);}

/* ---- OS environment / cwd / argv (UIOP/ASDF portability primitives) ------- */

/* Process command-line argv, captured by main() for %COMMAND-LINE-ARGUMENTS. */
int    bocl_main_argc = 0;
char** bocl_main_argv = NULL;

/* (%GETENV name) -> value string, or NIL if the variable is unset. */
static void kernel_GETENV(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref name=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    if(!typep(name,STRING_Class)){ TYPE_ERROR(name,STRING_Class); }
    char* n=string_cstring(name);
    const char* v=getenv(n);
    free(n);
    results->count=1;
    results->values[0]=v ? string_new_cstring(v) : NIL_Symbol;
    close_live_pool(pool);}

/* (%SETENV name value) -> value ; sets the environment variable (overwriting). */
static void kernel_SETENV(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref name=pop_mandatory_argument(function,&current);
    Ref value=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    if(!typep(name,STRING_Class)){ TYPE_ERROR(name,STRING_Class); }
    if(!typep(value,STRING_Class)){ TYPE_ERROR(value,STRING_Class); }
    char* n=string_cstring(name); char* v=string_cstring(value);
    setenv(n,v,1);
    free(n); free(v);
    results->count=1; results->values[0]=value;
    close_live_pool(pool);}

/* (%UNSETENV name) -> NIL ; removes the environment variable. */
static void kernel_UNSETENV(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref name=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    if(!typep(name,STRING_Class)){ TYPE_ERROR(name,STRING_Class); }
    char* n=string_cstring(name);
    unsetenv(n);
    free(n);
    results->count=1; results->values[0]=NIL_Symbol;
    close_live_pool(pool);}

/* (%GETCWD) -> the current working directory as a string (with a trailing "/"). */
static void kernel_GETCWD(unused Ref function, unused Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    char buf[4096];
    Ref out=NIL_Symbol;
    if(getcwd(buf,sizeof(buf)-2)!=NULL){
        size_t l=strlen(buf);
        if(l==0 || buf[l-1]!='/'){ buf[l]='/'; buf[l+1]='\0'; }
        out=string_new_cstring(buf);}
    results->count=1; results->values[0]=out;
    close_live_pool(pool);}

/* (%CHDIR path) -> T on success, NIL on failure. */
static void kernel_CHDIR(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref pathspec=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    char* path;
    if(typep(pathspec,STRING_Class)){ path=string_cstring(pathspec); }
    else if(typep(pathspec,PHYSICAL_PATHNAME_Class)){ path=string_cstring(namestring(pathspec)); }
    else { TYPE_ERROR(pathspec,STRING_Class); }
    int rc=chdir(path);
    free(path);
    results->count=1; results->values[0]=as_boolean(rc==0);
    close_live_pool(pool);}

/* (%COMMAND-LINE-ARGUMENTS) -> list of the raw argv strings (argv[0] first). */
static void kernel_COMMAND_LINE_ARGUMENTS(unused Ref function, unused Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    Ref out=NIL_Symbol;
    for(int i=bocl_main_argc-1; i>=0; i--){
        if(bocl_main_argv && bocl_main_argv[i]){
            Ref s=string_new_cstring(bocl_main_argv[i]);
            add_to_live_pool(s);
            push(s,&out);
            add_to_live_pool(out);}}
    results->count=1; results->values[0]=out;
    close_live_pool(pool);}

/* (%DIRECTORY-P path) -> T if PATH names an existing directory, else NIL. */
static void kernel_DIRECTORY_P(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref pathspec=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    char* path;
    if(typep(pathspec,STRING_Class)){ path=string_cstring(pathspec); }
    else if(typep(pathspec,PHYSICAL_PATHNAME_Class)){ path=string_cstring(namestring(pathspec)); }
    else { TYPE_ERROR(pathspec,STRING_Class); }
    struct stat st;
    bool isdir = (stat(path,&st)==0) && S_ISDIR(st.st_mode);
    free(path);
    results->count=1; results->values[0]=as_boolean(isdir);
    close_live_pool(pool);}

/* (%MKDIR path) -> T if PATH now names a directory (just created, or it already
   existed), NIL on failure.  Creates a single level; intermediate directories
   must already exist (ENSURE-DIRECTORIES-EXIST walks the chain). */
static void kernel_MKDIR(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref pathspec=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    char* path;
    if(typep(pathspec,STRING_Class)){ path=string_cstring(pathspec); }
    else if(typep(pathspec,PHYSICAL_PATHNAME_Class)){ path=string_cstring(namestring(pathspec)); }
    else { TYPE_ERROR(pathspec,STRING_Class); }
    int rc=mkdir(path,0777);   /* umask applies; 0777 -> typically 0755 */
    bool ok;
    if(rc==0){ ok=true; }
    else { struct stat st; ok=(stat(path,&st)==0)&&S_ISDIR(st.st_mode); }  /* EEXIST dir = ok */
    free(path);
    results->count=1; results->values[0]=as_boolean(ok);
    close_live_pool(pool);}

/* APPEND is needed by backquote-expanded code; the kernel previously only
   shipped NCONC.  (bocl_append is declared in cons.h, bocl_write_char in printer.h.) */

static void kernel_WRITE_CHAR(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref ch=cons_car(current); current=cons_cdr(current);
    Ref stream=consp(current)?cons_car(current):variable_value(S(*STANDARD-OUTPUT*));
    results->count=1;
    results->values[0]=bocl_write_char(ch,stream);
    close_live_pool(pool);}

static void kernel_APPEND(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(arguments);
    results->count=1;
    results->values[0]=bocl_append(arguments);
    close_live_pool(pool);}

/* (%clos-null-p x) — returns T if X is C NULL (not a valid Lisp object),
   NIL otherwise.  Used to distinguish kernel find-class returning C NULL
   (class not found) from returning a real class object. */
static void kernel_CLOS_NULL_P(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(arguments);
    Ref x=cons_car(arguments);
    results->count=1;
    results->values[0]=(x==NULL)?T_Symbol:NIL_Symbol;
    close_live_pool(pool);}

/* (%clos-obj-p x) — T if X is a CLOS instance (%obj struct), bypassing bridge. */
static void kernel_CLOS_OBJ_P(unused Ref function, Ref arguments, MultipleValues* results){
    Ref x=cons_car(arguments);
    results->count=1;
    results->values[0]=clos_instance_p(x)?T_Symbol:NIL_Symbol;}

/* (%clos-cls-p x) — T if X is a CLOS class object (%cls struct). */
static void kernel_CLOS_CLS_P(unused Ref function, Ref arguments, MultipleValues* results){
    Ref x=cons_car(arguments);
    results->count=1;
    results->values[0]=clos_class_p(x)?T_Symbol:NIL_Symbol;}

/* (%clos-register obj-struct-class cls-struct-class) — wires up the CLOS
   type bridge so class_of() and subclassp() understand CLOS instances. */
static void kernel_CLOS_REGISTER(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref obj_sc=cons_car(current); current=cons_cdr(current);
    Ref cls_sc=cons_car(current);
    clos_register_bridge(obj_sc, cls_sc);
    results->count=1;
    results->values[0]=T_Symbol;
    close_live_pool(pool);}

/* ---- Slice 22 kernel built-ins ------------------------------------------ */

#include <sys/stat.h>
#include <unistd.h>

/* (CLOSE stream &key abort) — close a stream; returns T. */
static void kernel_CLOSE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref stream=cons_car(arguments);
    /* :abort keyword is accepted but ignored — we always close */
    results->count=1;
    results->values[0]=stream_close_any(stream);
    close_live_pool(pool);}

/* (READ-BYTE stream &optional eof-error-p eof-value) */
static void kernel_READ_BYTE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref stream      = cons_car(current); current=cons_cdr(current);
    Ref eof_errorp  = consp(current)?cons_car(current):T_Symbol; if(consp(current)) current=cons_cdr(current);
    Ref eof_value   = consp(current)?cons_car(current):NIL_Symbol;
    results->count=1;
    results->values[0]=stream_read_byte(stream,eof_errorp,eof_value);
    close_live_pool(pool);}

/* (WRITE-BYTE byte stream) */
static void kernel_WRITE_BYTE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref byte_val = cons_car(current); current=cons_cdr(current);
    Ref stream   = cons_car(current);
    results->count=1;
    results->values[0]=stream_write_byte(byte_val, stream);
    close_live_pool(pool);}

/* (PROBE-FILE filespec) — returns the truename or NIL if not found. */
static void kernel_PROBE_FILE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref filespec=cons_car(arguments);
    Ref name_ref;
    if(typep(filespec,STRING_Class)){
        name_ref=filespec;
    } else {
        name_ref=namestring(filespec);}
    char* path=string_cstring(name_ref);
    struct stat st;
    bool exists=(stat(path,&st)==0);
    free(path);
    results->count=1;
    results->values[0]=exists?name_ref:NIL_Symbol;
    close_live_pool(pool);}

/* (FILE-WRITE-DATE filespec) — return the file's modification time as a
   Common-Lisp universal time (seconds since 1900-01-01 00:00:00 GMT), or
   signal an error if the file does not exist. */
#define BOCL_UNIVERSAL_TIME_OFFSET 2208988800L  /* 1900..1970 in seconds */
static void kernel_FILE_WRITE_DATE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref filespec=cons_car(arguments);
    Ref name_ref=typep(filespec,STRING_Class)?filespec:namestring(filespec);
    char* path=string_cstring(name_ref);
    struct stat st;
    int rc=stat(path,&st);
    free(path);
    if(rc!=0){ ERROR("FILE-WRITE-DATE: file does not exist%s",""); }
    results->count=1;
    results->values[0]=integer_from_word((word)st.st_mtime + BOCL_UNIVERSAL_TIME_OFFSET);
    close_live_pool(pool);}

/* (FILE-LENGTH stream) — length in bytes of the file the (open) file-stream is
   connected to, via fstat (does not disturb the stream position). */
static void kernel_FILE_LENGTH(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref stream=cons_car(arguments);
    check_type(stream,FILE_STREAM_Class);
    FILE* f=stream->slots->file_stream.stream;
    struct stat st;
    if(f==NULL || fstat(fileno(f),&st)!=0){ ERROR("FILE-LENGTH: not an open file stream%s",""); }
    results->count=1;
    results->values[0]=integer_from_word((word)st.st_size);
    close_live_pool(pool);}

/* (%COPY-FILE source destination) — byte-for-byte copy of SOURCE to
   DESTINATION.  Used by COMPILE-FILE, which on bocl (an interpreter) "compiles"
   a source file by copying it to the fasl pathname so that LOAD can read it
   back as ordinary source.  Returns the destination namestring. */
static void kernel_COPY_FILE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref from=cons_car(current); current=cons_cdr(current);
    Ref to=cons_car(current);
    Ref from_str=typep(from,STRING_Class)?from:namestring(from);
    Ref to_str  =typep(to,  STRING_Class)?to  :namestring(to);
    char* fp=string_cstring(from_str);
    char* tp=string_cstring(to_str);
    FILE* in=fopen(fp,"rb");
    if(!in){ free(fp); free(tp); ERROR("%%COPY-FILE: cannot open source%s",""); }
    FILE* out=fopen(tp,"wb");
    if(!out){ fclose(in); free(fp); free(tp); ERROR("%%COPY-FILE: cannot open destination%s",""); }
    char buf[8192]; size_t n;
    while((n=fread(buf,1,sizeof(buf),in))>0){ fwrite(buf,1,n,out); }
    fclose(in); fclose(out);
    free(fp); free(tp);
    results->count=1;
    results->values[0]=to_str;
    close_live_pool(pool);}

/* (DELETE-FILE filespec) — delete a file; errors if it can't. */
static void kernel_DELETE_FILE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref filespec=cons_car(arguments);
    Ref name_ref=typep(filespec,STRING_Class)?filespec:namestring(filespec);
    char* path=string_cstring(name_ref);
    int rc=remove(path);
    free(path);
    if(rc!=0){ ERROR("DELETE-FILE: failed to delete%s",""); }
    results->count=1;
    results->values[0]=T_Symbol;
    close_live_pool(pool);}

/* (RENAME-FILE filespec new-name) — rename a file; returns 3 values. */
static void kernel_RENAME_FILE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref from=cons_car(current); current=cons_cdr(current);
    Ref to=cons_car(current);
    Ref from_str=typep(from,STRING_Class)?from:namestring(from);
    Ref to_str  =typep(to,  STRING_Class)?to  :namestring(to);
    char* fp=string_cstring(from_str);
    char* tp=string_cstring(to_str);
    int rc=rename(fp,tp);
    free(fp); free(tp);
    if(rc!=0){ ERROR("RENAME-FILE: failed to rename%s",""); }
    results->count=3;
    results->values[0]=from_str;
    results->values[1]=to_str;
    results->values[2]=to_str;
    close_live_pool(pool);}

/* (WILD-PATHNAME-P pathname &optional field-key) — stub: returns NIL. */
static void kernel_WILD_PATHNAME_P(unused Ref function, Ref arguments, MultipleValues* results){
    add_to_live_pool(arguments);
    results->count=1;
    results->values[0]=NIL_Symbol;}

/* (TYPE-OF object) — return a type specifier for OBJECT. */
static void kernel_TYPE_OF(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref obj=cons_car(arguments);
    Ref type;
    if(nullp(obj)){                  type=S(NULL); }
    else if(obj==T_Symbol){          type=symbol_intern_cstring("BOOLEAN"); }
    else if(integerp(obj)){
        /* Return (integer lo hi) as a proper Lisp list — the most specific
           standard range bucket for this value. */
        word v=integer_value(obj);
        Ref lo,hi;
        if(v>=0 && v<=255){
            lo=I(0); hi=I(255);
        } else if(v>=-128 && v<=127){
            lo=I(-128); hi=I(127);
        } else {
            /* Full signed-word range */
            lo=integer_from_word(WORD_MIN); hi=integer_from_word(WORD_MAX);}
        type=list(S(INTEGER),lo,hi,NULL); }
    else if(floatp(obj)){            type=symbol_intern_cstring("DOUBLE-FLOAT"); }
    else if(symbolp(obj)){
        if(keywordp(obj))             type=S(KEYWORD);
        else                          type=S(SYMBOL); }
    else if(typep(obj,CONS_Class)){  type=S(CONS); }
    else if(typep(obj,STRING_Class)){ type=symbol_intern_cstring("SIMPLE-STRING"); }
    else if(typep(obj,BIT_VECTOR_Class)){ type=symbol_intern_cstring("SIMPLE-BIT-VECTOR"); }
    else if(typep(obj,VECTOR_Class)){ type=symbol_intern_cstring("SIMPLE-VECTOR"); }
    else if(typep(obj,CHARACTER_Class)){ type=S(CHARACTER); }
    else if(typep(obj,HASH_TABLE_Class)){ type=symbol_intern_cstring("HASH-TABLE"); }
    else if(typep(obj,PACKAGE_Class)){ type=S(PACKAGE); }
    else if(typep(obj,FUNCTION_Class)){ type=S(FUNCTION); }
    else if(closurep(obj)){          type=S(FUNCTION); }
    else if(typep(obj,PATHNAME_Class)){ type=S(PATHNAME); }
    else if(typep(obj,FILE_STREAM_Class)){ type=symbol_intern_cstring("FILE-STREAM"); }
    else if(typep(obj,STRING_STREAM_Class)){ type=symbol_intern_cstring("STRING-STREAM"); }
    else if(typep(obj,RANDOM_STATE_Class)){ type=symbol_intern_cstring("RANDOM-STATE"); }
    else if(clos_instance_p(obj)){
        /* CLOS instance — return the class name */
        Ref cls_of=class_of(obj);
        type=(clos_class_p(cls_of)&&structure_get(cls_of,symbol_intern_cstring("NAME"))!=NIL_Symbol)
             ? structure_get(cls_of,symbol_intern_cstring("NAME"))
             : symbol_intern_cstring("STANDARD-OBJECT");}
    else if(structurep(obj)){
        /* Structure instance — return the structure class name */
        type=obj->class->slots->structure_class.name;}
    else if(typep(obj,CONDITION_Class)){
        /* Condition — return its class name (e.g. SIMPLE-ERROR). */
        type=class_name(class_of(obj));}
    else{                            type=S(T); }
    results->count=1;
    results->values[0]=type;
    close_live_pool(pool);}

/* (INT-CHAR integer) — return the character with the given code. */
static void kernel_INT_CHAR(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref n=cons_car(arguments);
    check_type(n,INTEGER_Class);
    word code=integer_value(n);
    if(code<0||code>255){ ERROR("INT-CHAR: code out of range%s",""); }
    results->count=1;
    results->values[0]=octet_character((octet)code);
    close_live_pool(pool);}

/* (ARRAY-DIMENSIONS array) — return a list of the array's dimensions. */
static void kernel_ARRAY_DIMENSIONS(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref arr=cons_car(arguments);
    Ref dims;
    if(typep(arr,VECTOR_Class)||typep(arr,STRING_Class)||typep(arr,BIT_VECTOR_Class)){
        /* 1-D: single dimension */
        uword len;
        if(typep(arr,STRING_Class))         len=string_length(arr);
        else if(typep(arr,BIT_VECTOR_Class)) len=bit_vector_length(arr);
        else                                 len=arr->slots->array.dimensions[0];
        dims=list(integer_from_uword(len),NULL);
    } else {
        dims=NIL_Symbol;}
    results->count=1;
    results->values[0]=dims;
    close_live_pool(pool);}

/* (ARRAY-ELEMENT-TYPE array) — the element type the array specialises on.  A
   STRING holds CHARACTER and a BIT-VECTOR holds BIT; bocl's general arrays are
   not type-specialised, so they answer T (the upgraded type of "anything"). */
static void kernel_ARRAY_ELEMENT_TYPE(unused Ref function, Ref arguments, MultipleValues* results){
    add_to_live_pool(arguments);
    Ref arr = consp(arguments) ? cons_car(arguments) : NIL_Symbol;
    results->count=1;
    if(stringp(arr))            { results->values[0]=S(CHARACTER); }
    else if(bit_vector_p(arr))  { results->values[0]=S(BIT); }
    else                        { results->values[0]=S(T); }}

/* Note: INPUT-STREAM-P, OUTPUT-STREAM-P, OPEN-STREAM-P are already in
   built_in_functions.c — not duplicated here. */

/* ---- Slice 23: timing, write-string, sleep, clear-input, random ------- */

/* splitmix64 PRNG — fast, good quality, no external state needed. */
static uint64_t prng_state = 0x12345678ABCDEF01ULL;

static uint64_t splitmix64(void){
    uint64_t z = (prng_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);}

/* (RANDOM limit &optional random-state) — uniform in [0,limit). */
static void kernel_RANDOM_FN(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref limit=cons_car(arguments);
    uint64_t r=splitmix64();
    Ref val;
    if(integerp(limit)){
        word lv=integer_value(limit);
        if(lv<=0){ ERROR("RANDOM: limit must be positive%s",""); }
        val=integer_from_word((word)(r%(uint64_t)lv));
    } else if(floatp(limit)){
        floating lv=float_value(limit);
        if(lv<=0.0){ ERROR("RANDOM: limit must be positive%s",""); }
        val=float_from_floating(((floating)(r>>11)/(floating)(1ULL<<53))*lv);
    } else {
        TYPE_ERROR(limit,NUMBER_Class); val=NIL_Symbol;}
    results->count=1;
    results->values[0]=val;
    close_live_pool(pool);}

/* (MAKE-RANDOM-STATE &optional state) — return T (use global PRNG). */
static void kernel_MAKE_RANDOM_STATE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    results->count=1;
    results->values[0]=T_Symbol;  /* bocl uses a global PRNG; T = current state */
    close_live_pool(pool);}

/* CL universal time = seconds since 1900-01-01 00:00:00 UTC.
   Unix epoch is 1970-01-01 = 70 years after 1900. */
#define SECONDS_FROM_1900_TO_1970  2208988800UL

static void kernel_GET_UNIVERSAL_TIME(unused Ref function, Ref arguments, MultipleValues* results){
    add_to_live_pool(arguments);
    results->count=1;
    results->values[0]=integer_from_uword((uword)time(NULL)+(uword)SECONDS_FROM_1900_TO_1970);}

static void kernel_GET_INTERNAL_REAL_TIME(unused Ref function, Ref arguments, MultipleValues* results){
    add_to_live_pool(arguments);
    struct timeval tv; gettimeofday(&tv,NULL);
    uword us=(uword)tv.tv_sec*1000000UL+(uword)tv.tv_usec;
    results->count=1;
    results->values[0]=integer_from_uword(us);}

static void kernel_GET_INTERNAL_RUN_TIME(unused Ref function, Ref arguments, MultipleValues* results){
    add_to_live_pool(arguments);
    struct timespec ts; clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&ts);
    uword us=(uword)ts.tv_sec*1000000UL+(uword)ts.tv_nsec/1000UL;
    results->count=1;
    results->values[0]=integer_from_uword(us);}

/* (WRITE-STRING string &optional stream &key start end)
   Also used as (KERNEL-WRITE-STRING string stream start end) — positional. */
static void kernel_WRITE_STRING(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref str    = cons_car(current); current=cons_cdr(current);
    Ref stream = NIL_Symbol;
    Ref start  = NIL_Symbol;
    Ref end    = NIL_Symbol;
    if(consp(current)){
        stream=cons_car(current); current=cons_cdr(current);
        /* Accept both keyword (:start/:end) and positional (start end) forms. */
        if(consp(current)){
            Ref x=cons_car(current);
            if(keywordp(x)){
                /* Keyword args: skip :start/:end labels, take values. */
                while(consp(current)){
                    Ref k=cons_car(current); current=cons_cdr(current);
                    Ref v=consp(current)?cons_car(current):NIL_Symbol; if(consp(current)) current=cons_cdr(current);
                    if(eql(k,keyword_intern_cstring("START"))) start=v;
                    else if(eql(k,keyword_intern_cstring("END"))) end=v;}
            } else {
                /* Positional: start then end. */
                start=x; current=cons_cdr(current);
                if(consp(current)){ end=cons_car(current);}}}}
    if(nullp(stream)) stream=variable_value(S(*STANDARD-OUTPUT*));
    results->count=1;
    results->values[0]=bocl_write_string(str,stream,start,end);
    close_live_pool(pool);}

/* (SLEEP seconds) — non-blocking sleep using nanosleep. */
static void kernel_SLEEP(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref secs_ref=cons_car(current);
    double s=floatp(secs_ref)?float_value(secs_ref):integer_value_as_floating(secs_ref);
    if(s>0.0){
        struct timespec ts;
        ts.tv_sec=(time_t)s;
        ts.tv_nsec=(long)((s-(double)ts.tv_sec)*1e9);
        nanosleep(&ts,NULL);}
    results->count=1;
    results->values[0]=NIL_Symbol;
    close_live_pool(pool);}

/* (CLEAR-INPUT &optional stream) — discard buffered input. */
static void kernel_CLEAR_INPUT(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    results->count=1;
    results->values[0]=NIL_Symbol;
    close_live_pool(pool);}

/* ---- Slice 25: environment, PARSE-FLOAT, COMPILE stubs -------------------*/

/* Helper: get uname struct; return false on failure. */
static bool get_uname(struct utsname* u){ return uname(u)==0; }

/* (MACHINE-TYPE) → sysname from uname, e.g. "Darwin" */
static void kernel_MACHINE_TYPE(unused Ref function, unused Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    struct utsname u;
    Ref s = get_uname(&u) ? string_new_cstring(u.machine) : string_new_cstring("unknown");
    results->count=1; results->values[0]=s;
    close_live_pool(pool);}

/* (MACHINE-INSTANCE) → hostname */
static void kernel_MACHINE_INSTANCE(unused Ref function, unused Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    char buf[256]=""; gethostname(buf,sizeof(buf)-1);
    results->count=1; results->values[0]=string_new_cstring(buf);
    close_live_pool(pool);}

/* (MACHINE-VERSION) → uname version field */
static void kernel_MACHINE_VERSION(unused Ref function, unused Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    struct utsname u;
    Ref s = get_uname(&u) ? string_new_cstring(u.version) : string_new_cstring("0");
    results->count=1; results->values[0]=s;
    close_live_pool(pool);}

/* (SOFTWARE-TYPE) → OS name, e.g. "Darwin" or "Linux" */
static void kernel_SOFTWARE_TYPE(unused Ref function, unused Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    struct utsname u;
    Ref s = get_uname(&u) ? string_new_cstring(u.sysname) : string_new_cstring("unknown");
    results->count=1; results->values[0]=s;
    close_live_pool(pool);}

/* (SOFTWARE-VERSION) → OS release string */
static void kernel_SOFTWARE_VERSION(unused Ref function, unused Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    struct utsname u;
    Ref s = get_uname(&u) ? string_new_cstring(u.release) : string_new_cstring("0");
    results->count=1; results->values[0]=s;
    close_live_pool(pool);}

/* (SHORT-SITE-NAME) → short host name. bocl maps the site name to the host
   identity (see issues/closed/platform-identification.issue); the short form
   strips any trailing domain. */
static void kernel_SHORT_SITE_NAME(unused Ref function, unused Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    char buf[256]=""; gethostname(buf,sizeof(buf)-1);
    char* dot=strchr(buf,'.'); if(dot){ *dot='\0'; }
    results->count=1; results->values[0]=string_new_cstring(buf);
    close_live_pool(pool);}

/* (LONG-SITE-NAME) → fully-qualified domain name (getaddrinfo canonical name),
   falling back to the host name when it cannot be resolved. */
static void kernel_LONG_SITE_NAME(unused Ref function, unused Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    char buf[256]=""; gethostname(buf,sizeof(buf)-1);
    const char* name=buf;
    struct addrinfo hints; memset(&hints,0,sizeof(hints));
    hints.ai_family=AF_UNSPEC;
    hints.ai_flags=AI_CANONNAME;
    struct addrinfo* res=NULL;
    if(getaddrinfo(buf,NULL,&hints,&res)==0 && res && res->ai_canonname && res->ai_canonname[0]){
        name=res->ai_canonname;}
    Ref s=string_new_cstring(name);
    if(res){ freeaddrinfo(res); }
    results->count=1; results->values[0]=s;
    close_live_pool(pool);}

/* (PARSE-FLOAT string &key start end junk-allowed) → float
   Simplified: ignores :start/:end/:junk-allowed, uses strtod on the whole string. */
static void kernel_PARSE_FLOAT(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref str_ref=cons_car(arguments);
    if(!stringp(str_ref)){ ERROR("PARSE-FLOAT: argument is not a string%s",""); }
    char* cstr=string_cstring(str_ref);
    char* endp=NULL; errno=0;
    double val=strtod(cstr,&endp);
    if(errno!=0||endp==cstr){
        ERROR("PARSE-FLOAT: cannot parse ~S as a float",str_ref); }
    results->count=1; results->values[0]=float_from_floating(val);
    close_live_pool(pool);}

/* ---- Slice 25b: EVAL, MACROEXPAND-1, MACROEXPAND, proper COMPILE ---------*/

/* (EVAL form) — evaluate FORM in the global environment. */
static void kernel_EVAL(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref form=cons_car(arguments);
    evale(form, eval_global_environment(), results);
    (void)pool;  /* pool closed inside evale */
    close_live_pool(pool);}

/* (MACROEXPAND-1 form &optional env) → (values expanded-form expanded-p)
   Performs one step of macro expansion.  ENV is ignored (global only). */
static void kernel_MACROEXPAND_1(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref form=cons_car(arguments);
    bool expanded=false;
    if(consp(form)){
        Ref op=cons_car(form);
        if(symbolp(op)){
            Ref macro_fn=getf(symbol_plist(op),S($MACRO),NIL_Symbol);
            if(!nullp(macro_fn)){
                Ref macro_args=cons_cdr(form);
                MultipleValues mac={0};
                kernel_apply(macro_fn,macro_args,&mac);
                form=(mac.count>0)?mac.values[0]:NIL_Symbol;
                expanded=true;}}}
    results->count=2;
    results->values[0]=form;
    results->values[1]=expanded?T_Symbol:NIL_Symbol;
    close_live_pool(pool);}

/* (MACROEXPAND form &optional env) → (values fully-expanded-form expanded-p)
   Repeatedly applies MACROEXPAND-1 until no further expansion is possible. */
static void kernel_MACROEXPAND(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref form=cons_car(arguments);
    bool any=false;
    for(;;){
        bool this_step=false;
        if(consp(form)){
            Ref op=cons_car(form);
            if(symbolp(op)){
                Ref macro_fn=getf(symbol_plist(op),S($MACRO),NIL_Symbol);
                if(!nullp(macro_fn)){
                    Ref macro_args=cons_cdr(form);
                    MultipleValues mac={0};
                    kernel_apply(macro_fn,macro_args,&mac);
                    form=(mac.count>0)?mac.values[0]:NIL_Symbol;
                    this_step=true; any=true;}}}
        if(!this_step) break;}
    results->count=2;
    results->values[0]=form;
    results->values[1]=any?T_Symbol:NIL_Symbol;
    close_live_pool(pool);}

/* (COMPILE name &optional definition) → (values name/fn warnings-p failure-p)
   Minimal compilation per CLHS 3.2.2:
   – (compile nil lambda-form) : evaluate lambda-form → function
   – (compile nil fn)          : fn is already a function; return it
   – (compile 'name)           : already compiled; return name
   – (compile 'name lambda)    : evaluate lambda, install as name's function */
static void kernel_COMPILE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref name=cons_car(arguments);
    Ref rest=cons_cdr(arguments);
    Ref defn=consp(rest)?cons_car(rest):NIL_Symbol;

    Ref compiled;
    if(nullp(name)){
        /* (compile nil defn) — evaluate defn to produce a function */
        if(nullp(defn)){
            ERROR("COMPILE: a definition is required when name is NIL%s","");}
        if(functionp(defn)||closurep(defn)){
            compiled=defn;    /* already a function */
        } else {
            MultipleValues tmp={0};
            evale(defn,eval_global_environment(),&tmp);
            compiled=(tmp.count>0)?tmp.values[0]:NIL_Symbol;}
    } else if(symbolp(name)&&nullp(defn)){
        /* (compile 'name) — function already defined; return name */
        compiled=name;
    } else if(symbolp(name)){
        /* (compile 'name defn) — evaluate, install, return name */
        Ref fn;
        if(functionp(defn)||closurep(defn)){
            fn=defn;
        } else {
            MultipleValues tmp={0};
            evale(defn,eval_global_environment(),&tmp);
            fn=(tmp.count>0)?tmp.values[0]:NIL_Symbol;}
        if(functionp(fn)||closurep(fn))
            set_symbol_function(name,fn);
        compiled=name;
    } else {
        ERROR("COMPILE: name must be NIL or a symbol%s","");}

    results->count=3;
    results->values[0]=compiled;
    results->values[1]=NIL_Symbol;  /* warnings-p */
    results->values[2]=NIL_Symbol;  /* failure-p */
    close_live_pool(pool);}

/* (COMPILE-FILE input-file &rest keys) — minimal compilation.
   In bocl there is no FASL output format.  The Lisp-layer wrapper in
   prelude.lisp calls this after setting *COMPILE-FILE-PATHNAME* etc.;
   this C entry point processes the file with *LOADING-FASL-FILE* = T
   so EVAL-WHEN :COMPILE-TOPLEVEL and :LOAD-TOPLEVEL forms are active
   while :EXECUTE forms are suppressed. */
static void kernel_COMPILE_FILE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref filespec=cons_car(arguments);
    char* path;
    if(typep(filespec,STRING_Class)){
        path=string_cstring(filespec);
    } else if(typep(filespec,PHYSICAL_PATHNAME_Class)){
        path=string_cstring(namestring(filespec));
    } else {
        TYPE_ERROR(filespec,STRING_Class);}

    /* Bind *COMPILE-FILE-PATHNAME* and *LOADING-FASL-FILE* = T. */
    Ref old_cfp  = variable_value(S(*COMPILE-FILE-PATHNAME*));
    Ref old_cft  = variable_value(S(*COMPILE-FILE-TRUENAME*));
    Ref old_lff  = variable_value(S(*LOADING-FASL-FILE*));
    Ref pn_obj   = pathname(string_new_cstring(path));
    set_symbol_value(S(*COMPILE-FILE-PATHNAME*), pn_obj);
    set_symbol_value(S(*COMPILE-FILE-TRUENAME*), pn_obj);
    set_symbol_value(S(*LOADING-FASL-FILE*),     T_Symbol);

    bocl_load_cstring(path);   /* LOAD with compile-file variable bindings */

    set_symbol_value(S(*COMPILE-FILE-PATHNAME*), old_cfp);
    set_symbol_value(S(*COMPILE-FILE-TRUENAME*), old_cft);
    set_symbol_value(S(*LOADING-FASL-FILE*),     old_lff);
    free(path);

    results->count=3;
    results->values[0]=pn_obj;  /* "output file" = source file (no FASL) */
    results->values[1]=NIL_Symbol;
    results->values[2]=NIL_Symbol;
    close_live_pool(pool);}

/* (DISASSEMBLE fn) → NIL — bocl is an interpreter, nothing to disassemble. */
static void kernel_DISASSEMBLE(unused Ref function, unused Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    results->count=1; results->values[0]=NIL_Symbol;
    close_live_pool(pool);}

/* (ED &optional x) → NIL stub. */
static void kernel_ED(unused Ref function, unused Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    results->count=1; results->values[0]=NIL_Symbol;
    close_live_pool(pool);}

/* (%DEFAULT-MODULES-DIRECTORY) → the directory REQUIRE scans for installed
   modules, as an absolute namestring with a trailing slash.  Resolved WITHOUT
   reference to argv[0] / the current directory / where the prelude was found
   (all of which are unreliable — a prelude found relative to the CWD would scan
   the wrong place).  Order:
     1. $BOCL_MODULES_DIR                       (explicit runtime override)
     2. <dir of the compiled-in BOCL_PRELUDE_PATH>/modules/
        BOCL_PRELUDE_PATH is set by `make install` to $(bocllibexecdir)/
        prelude.lisp, so it already carries the install PREFIX (e.g.
        /opt/local/libexec/bocl/prelude.lisp -> /opt/local/libexec/bocl/modules/).
     3. a last-resort default for the conventional PREFIX=/opt/local install.
   (Later this can consult XDG paths and a generated install-paths module; kept
   deliberately small for now.) */
/* (%DEFAULT-MODULES-DIRECTORY) -> the modules directory namestring (trailing '/').
   Resolution, INDEPENDENT of argv[0]/CWD (never derive resources from the exe):
     1. $BOCL_MODULES_DIR                                   (explicit override).
     2. <dir of the compiled-in BOCL_PRELUDE_PATH>/modules/ (carries the install
        PREFIX from `make install`, e.g. a custom PREFIX=/foo).
     3. the standard install paths, the SAME three searched for the prelude
        (find_prelude in main.c): /opt/local, /usr/local, /usr under
        libexec/bocl/modules/.
   Candidates 2-3 are tried in order and the first that EXISTS (as a directory)
   wins; if none exists, the first candidate is returned as the nominal default
   so REQUIRE's error report names a sensible path. */
static void kernel_DEFAULT_MODULES_DIRECTORY(unused Ref function, unused Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    char buf[4096];
    const char* env=getenv("BOCL_MODULES_DIR");
    if(env && env[0]){
        size_t n=strlen(env);
        if(n && env[n-1]=='/'){ snprintf(buf,sizeof(buf),"%s",env); }
        else                  { snprintf(buf,sizeof(buf),"%s/",env); }
    }else{
        /* Candidate 2: derived from the build-time-configured prelude path. */
        char cfg[4096]; cfg[0]='\0';
#ifdef BOCL_PRELUDE_PATH
        {   const char* pp=BOCL_PRELUDE_PATH;
            const char* slash=strrchr(pp,'/');
            if(slash){ int base=(int)(slash-pp)+1;   /* keep the trailing slash */
                snprintf(cfg,sizeof(cfg),"%.*smodules/",base,pp); } }
#endif
        /* Candidates 3: the standard install paths (mirror find_prelude). */
        static const char* std_paths[]={
            "/opt/local/libexec/bocl/modules/",
            "/usr/local/libexec/bocl/modules/",
            "/usr/libexec/bocl/modules/",
            NULL};
        const char* chosen=NULL;
        const char* first=NULL;
        if(cfg[0]){ first=cfg; if(bocl_is_dir(cfg)){ chosen=cfg; } }
        if(!chosen){
            for(int i=0; std_paths[i]; i++){
                if(!first){ first=std_paths[i]; }
                if(bocl_is_dir(std_paths[i])){ chosen=std_paths[i]; break; }}}
        if(!chosen){ chosen = first ? first : "/opt/local/libexec/bocl/modules/"; }
        snprintf(buf,sizeof(buf),"%s",chosen);
    }
    results->count=1; results->values[0]=string_new_cstring(buf);
    close_live_pool(pool);}

/* (%STRING-CONTAINS-CI needle haystack) → T if NEEDLE occurs in HAYSTACK as a
   case-insensitive substring, else NIL.  Operates directly on the string octet
   data (no per-character CHAR calls, no allocation), so APROPOS over the whole
   image is fast — the Lisp CHAR accessor costs ~15 us/call, which made the naive
   substring scan take tens of seconds. */
static inline int bocl_ci_fold(int c){ return (c>='A'&&c<='Z') ? c+('a'-'A') : c; }
static void kernel_STRING_CONTAINS_CI(unused Ref function, Ref arguments, MultipleValues* results){
    Ref needle   = car(arguments);
    Ref haystack = cadr(arguments);
    check_class(needle,  STRING_Class);
    check_class(haystack,STRING_Class);
    const unsigned char* n = (const unsigned char*)needle->slots->octet_array.elements->elements;
    const unsigned char* h = (const unsigned char*)haystack->slots->octet_array.elements->elements;
    size_t nlen = needle->slots->octet_array.dimensions[0];
    size_t hlen = haystack->slots->octet_array.dimensions[0];
    bool found = (nlen==0);
    if(!found && nlen<=hlen){
        for(size_t i=0, last=hlen-nlen; i<=last && !found; i++){
            size_t j=0;
            while(j<nlen && bocl_ci_fold(h[i+j])==bocl_ci_fold(n[j])){ j++; }
            if(j==nlen){ found=true; }}}
    results->count=1; results->values[0]= found ? T_Symbol : NIL_Symbol;}

/* Fast bounded string comparison — the C engine behind %STRING-COMPARE,
   %STRING-COMPARE-BOUNDED and %STRING-CMP (and thus every STRING</STRING=/... and
   any SORT with a string comparator).  bocl strings are octet arrays with one
   octet per character (char_ref indexes bytes directly), and case folding is
   ASCII-only (see UPPER-CASE-P/LOWER-CASE-P), so a byte-wise scan reproduces the
   Lisp semantics exactly while skipping ~15us-per-CHAR interpreter dispatch.

   (%STRING-CMP2 a b ci start1 end1 start2 end2) compares A[start1,end1) with
   B[start2,end2) (END* may be NIL = string length; CI non-NIL = fold case) and
   returns two values: the absolute index in A of the first mismatch (or the end
   of A's compared region when A is a prefix of B), and a relation -1 / 0 / 1. */
static void kernel_STRING_CMP2(unused Ref function, Ref arguments, MultipleValues* results){
    Ref current=arguments;
    Ref a  = pop_mandatory_argument(function,&current);
    Ref b  = pop_mandatory_argument(function,&current);
    Ref ci = pop_mandatory_argument(function,&current);
    Ref rs1= pop_mandatory_argument(function,&current);
    Ref re1= pop_mandatory_argument(function,&current);
    Ref rs2= pop_mandatory_argument(function,&current);
    Ref re2= pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    check_class(a,STRING_Class);
    check_class(b,STRING_Class);
    const unsigned char* pa=(const unsigned char*)a->slots->octet_array.elements->elements;
    const unsigned char* pb=(const unsigned char*)b->slots->octet_array.elements->elements;
    word la=(word)a->slots->octet_array.dimensions[0];
    word lb=(word)b->slots->octet_array.dimensions[0];
    word i=integer_value(rs1);
    word j=integer_value(rs2);
    word e1=nullp(re1)?la:integer_value(re1);
    word e2=nullp(re2)?lb:integer_value(re2);
    bool fold=!nullp(ci);
    if(i<0){ i=0; } if(j<0){ j=0; }
    if(e1>la){ e1=la; } if(e2>lb){ e2=lb; }
    word rel=0;
    for(;;){
        if(i>=e1 && j>=e2){ rel=0;  break; }
        if(i>=e1){          rel=-1; break; }
        if(j>=e2){          rel=1;  break; }
        int ca=pa[i], cb=pb[j];
        if(fold){ ca=bocl_ci_fold(ca); cb=bocl_ci_fold(cb); }
        if(ca<cb){ rel=-1; break; }
        if(ca>cb){ rel=1;  break; }
        i++; j++;}
    results->count=2;
    results->values[0]=integer_from_word(i);
    results->values[1]=integer_from_word(rel);}

/* Fast substring search — the C engine behind SEARCH's string fast path.
   (%STRING-SEARCH pattern target ci start2 end2) returns the smallest index in
   TARGET[start2,end2) at which PATTERN occurs (whole of PATTERN, CI folds case),
   or NIL.  An empty PATTERN matches at START2 (per CLHS SEARCH). */
static void kernel_STRING_SEARCH(unused Ref function, Ref arguments, MultipleValues* results){
    Ref current=arguments;
    Ref pat = pop_mandatory_argument(function,&current);
    Ref tgt = pop_mandatory_argument(function,&current);
    Ref ci  = pop_mandatory_argument(function,&current);
    Ref rs2 = pop_mandatory_argument(function,&current);
    Ref re2 = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    check_class(pat,STRING_Class);
    check_class(tgt,STRING_Class);
    const unsigned char* p=(const unsigned char*)pat->slots->octet_array.elements->elements;
    const unsigned char* t=(const unsigned char*)tgt->slots->octet_array.elements->elements;
    word np=(word)pat->slots->octet_array.dimensions[0];
    word nt=(word)tgt->slots->octet_array.dimensions[0];
    word s=integer_value(rs2);
    word e=nullp(re2)?nt:integer_value(re2);
    bool fold=!nullp(ci);
    if(s<0){ s=0; } if(e>nt){ e=nt; }
    Ref answer=NIL_Symbol;
    for(word i=s; i+np<=e; i++){
        word k=0;
        while(k<np){
            int a=t[i+k], b=p[k];
            if(fold){ a=bocl_ci_fold(a); b=bocl_ci_fold(b); }
            if(a!=b){ break; }
            k++;}
        if(k==np){ answer=integer_from_word(i); break; }}
    results->count=1; results->values[0]=answer;}

/* Kernel SORT — a stable bottom-up merge sort whose loop runs in C, calling back
   into Lisp only for the comparator.  A Lisp-level SORT is bounded by the
   interpreter's ~150us-per-op cost (car/cdr, aref, funcall), so an in-C sort that
   pays that cost once per COMPARISON (not several times per element move) is the
   only way to make SORT fast; an array merge sort written in Lisp was measured
   ~5x SLOWER than the list one because bocl AREF/MAKE-ARRAY dwarf CONS car/cdr.

   GC discipline (this calls Lisp per comparison, so a GC — or a comparator error
   longjmp — can happen at every kernel_apply):
   - Scratch is two bocl simple-vectors (NOT malloc), pinned in the live pool, so
     a comparator error that longjmps past this frame leaks nothing (the GC
     reclaims them) and a skipped close_live_pool only briefly over-retains and
     self-heals at the enclosing frame — the same contract FUNCALL/MAPCAR rely on.
   - The element Refs held in the scratch vectors stay reachable from the pinned
     input SEQUENCE throughout, and the collector is non-moving, so the raw
     element pointers (a/b) stay valid across every kernel_apply (cf. the bucket
     vector in hash_table.c).  The scratch vectors stay young (pinned), so raw
     stores into them need no write barrier; only the final write-back into the
     possibly-old input goes through cons_rplaca / gc_write_barrier. */
extern Ref   Object_new(Ref class, Slots* slots);
extern Array* Array_slots(Ref element_type, halfword rank, halfword* dimensions);

/* (less src[j] < src[i]) ?  Reuses the pinned 2-cons ARGPAIR to avoid allocating
   an argument list per comparison. */
static inline bool sort_less(Ref less, Ref argpair, Ref* elems, word j, word i){
    cons_rplaca(argpair, elems[j]);
    cons_rplaca(cons_cdr(argpair), elems[i]);
    MultipleValues r; r.count=0;
    kernel_apply(less, argpair, &r);
    return (r.count>0) && !nullp(r.values[0]);}

/* Bottom-up stable merge sort of A[0,n) using scratch B[0,n); the sorted result
   is left in A. */
static void sort_merge_refs(Ref less, Ref argpair, Ref* a, Ref* b, word n){
    Ref* src=a; Ref* dst=b;
    for(word width=1; width<n; width*=2){
        for(word lo=0; lo<n; lo+=2*width){
            word mid=lo+width;      if(mid>n){ mid=n; }
            word hi =lo+2*width;    if(hi>n){  hi=n;  }
            word i=lo, j=mid, k=lo;
            while(i<mid && j<hi){
                /* stable: take the left run unless src[j] is strictly less */
                if(sort_less(less,argpair,src,j,i)){ dst[k++]=src[j++]; }
                else                               { dst[k++]=src[i++]; }}
            while(i<mid){ dst[k++]=src[i++]; }
            while(j<hi){  dst[k++]=src[j++]; }}
        Ref* t=src; src=dst; dst=t;}
    if(src!=a){ for(word i=0;i<n;i++){ a[i]=src[i]; } }}

/* (%KERNEL-SORT! sequence less) — destructively sort a list or vector by the
   strict 2-arg comparator LESS.  KEY handling stays in the Lisp SORT wrapper. */
static void kernel_KERNEL_SORT(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    Ref current=arguments;
    Ref seq  = pop_mandatory_argument(function,&current);
    Ref less = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    add_to_live_pool(seq);
    add_to_live_pool(less);

    bool is_list;
    word n;
    if(nullp(seq)){ results->count=1; results->values[0]=seq; close_live_pool(pool); return; }
    if(consp(seq)){       is_list=true;  n=(word)list_length(seq); }
    else if(vectorp(seq)){is_list=false; n=(word)vector_length(seq); }
    else { close_live_pool(pool); ERROR("SORT: %s","argument is not a proper list or vector"); return; }

    if(n<2){ results->count=1; results->values[0]=seq; close_live_pool(pool); return; }

    halfword dims[1]; dims[0]=(halfword)n;
    Ref buf = Object_new(VECTOR_Class,(Slots*)Array_slots(T_Symbol,1,dims));
    add_to_live_pool(buf);
    Ref tmp = Object_new(VECTOR_Class,(Slots*)Array_slots(T_Symbol,1,dims));
    add_to_live_pool(tmp);
    Ref* a = buf->slots->array.elements->elements;
    Ref* b = tmp->slots->array.elements->elements;

    if(is_list){
        Ref cell=seq;
        for(word idx=0; idx<n; idx++){ a[idx]=cons_car(cell); cell=cons_cdr(cell); }}
    else{
        Ref* s = seq->slots->array.elements->elements;
        for(word idx=0; idx<n; idx++){ a[idx]=s[idx]; }}

    Ref argpair = list(NIL_Symbol,NIL_Symbol,NULL);
    add_to_live_pool(argpair);

    sort_merge_refs(less, argpair, a, b, n);   /* sorted result left in a (buf) */

    if(is_list){
        Ref cell=seq;
        for(word idx=0; idx<n; idx++){ cons_rplaca(cell, a[idx]); cell=cons_cdr(cell); }}
    else{
        Ref* d = seq->slots->array.elements->elements;
        for(word idx=0; idx<n; idx++){ d[idx]=a[idx]; }
        gc_write_barrier(seq->slots->array.elements);}

    results->count=1; results->values[0]=seq;
    close_live_pool(pool);}

/* ---- end Slice 25 C functions --------------------------------------------*/

void bocl_register_load(void){
    Ref fname=S(LOAD);
    set_symbol_function(fname,built_in_function_new(fname,list(S(FILESPEC),NULL),kernel_LOAD));
    Ref ld=symbol_intern_cstring("%LIST-DIRECTORY");
    set_symbol_function(ld,built_in_function_new(ld,list(S(PATH),NULL),kernel_LIST_DIRECTORY));
    {   Ref md=symbol_intern_cstring("%DEFAULT-MODULES-DIRECTORY");
        set_symbol_function(md,built_in_function_new(md,list(NULL),kernel_DEFAULT_MODULES_DIRECTORY)); }
    {   Ref sc=symbol_intern_cstring("%STRING-CONTAINS-CI");
        set_symbol_function(sc,built_in_function_new(sc,list(S(NAME),S(VALUE),NULL),kernel_STRING_CONTAINS_CI)); }
    {   Ref sc=symbol_intern_cstring("%STRING-CMP2");
        set_symbol_function(sc,built_in_function_new(sc,
            list(S(A),S(B),S(CI),S(START1),S(END1),S(START2),S(END2),NULL),kernel_STRING_CMP2)); }
    {   Ref sc=symbol_intern_cstring("%STRING-SEARCH");
        set_symbol_function(sc,built_in_function_new(sc,
            list(S(PATTERN),S(TARGET),S(CI),S(START2),S(END2),NULL),kernel_STRING_SEARCH)); }
    {   Ref sc=symbol_intern_cstring("%KERNEL-SORT!");
        set_symbol_function(sc,built_in_function_new(sc,
            list(S(SEQUENCE),S(LESS),NULL),kernel_KERNEL_SORT)); }
    /* OS portability primitives (UIOP/ASDF): env, cwd, argv. */
    {   Ref s;
        s=symbol_intern_cstring("%GETENV");
        set_symbol_function(s,built_in_function_new(s,list(S(NAME),NULL),kernel_GETENV));
        s=symbol_intern_cstring("%SETENV");
        set_symbol_function(s,built_in_function_new(s,list(S(NAME),S(VALUE),NULL),kernel_SETENV));
        s=symbol_intern_cstring("%UNSETENV");
        set_symbol_function(s,built_in_function_new(s,list(S(NAME),NULL),kernel_UNSETENV));
        s=symbol_intern_cstring("%GETCWD");
        set_symbol_function(s,built_in_function_new(s,NIL_Symbol,kernel_GETCWD));
        s=symbol_intern_cstring("%CHDIR");
        set_symbol_function(s,built_in_function_new(s,list(S(PATH),NULL),kernel_CHDIR));
        s=symbol_intern_cstring("%COMMAND-LINE-ARGUMENTS");
        set_symbol_function(s,built_in_function_new(s,NIL_Symbol,kernel_COMMAND_LINE_ARGUMENTS));
        s=symbol_intern_cstring("%DIRECTORY-P");
        set_symbol_function(s,built_in_function_new(s,list(S(PATH),NULL),kernel_DIRECTORY_P));
        s=symbol_intern_cstring("%MKDIR");
        set_symbol_function(s,built_in_function_new(s,list(S(PATH),NULL),kernel_MKDIR));
        s=symbol_intern_cstring("%DIRECTORY-GLOB");
        set_symbol_function(s,built_in_function_new(s,list(S(PATTERN),NULL),kernel_DIRECTORY_GLOB));}
    Ref af=S(APPEND);
    set_symbol_function(af,built_in_function_new(af,list(S(&REST),S(LISTS),NULL),kernel_APPEND));
    /* BOCL::%MAKE-OUTPUT-CALLBACK-STREAM — kernel hook for CL compound streams
       and DRIBBLE (registered here: BOCL_Package exists only after packages init). */
    {   Ref mcs=package_intern_cstring_in("%MAKE-OUTPUT-CALLBACK-STREAM",BOCL_Package);
        set_symbol_function(mcs,built_in_function_new(mcs,list(S(WRITE-FUNCTION),NULL),
                            kernel_MAKE_OUTPUT_CALLBACK_STREAM));}
    /* Helper for CLOS: detect C NULL (returned by kernel find-class for unknown names). */
    Ref cn=symbol_intern_cstring("%CLOS-NULL-P");
    set_symbol_function(cn,built_in_function_new(cn,list(S(X),NULL),kernel_CLOS_NULL_P));
    /* Predicates that bypass the class_of bridge — needed after %clos-register. */
    Ref cop=symbol_intern_cstring("%CLOS-OBJ-P");
    set_symbol_function(cop,built_in_function_new(cop,list(S(X),NULL),kernel_CLOS_OBJ_P));
    Ref ccp=symbol_intern_cstring("%CLOS-CLS-P");
    set_symbol_function(ccp,built_in_function_new(ccp,list(S(X),NULL),kernel_CLOS_CLS_P));
    /* Wire the CLOS type bridge: call from prelude after defining %obj/%cls structs. */
    Ref cr=symbol_intern_cstring("%CLOS-REGISTER");
    set_symbol_function(cr,built_in_function_new(cr,
        list(symbol_intern_cstring("OBJ-SC"),symbol_intern_cstring("CLS-SC"),NULL),
        kernel_CLOS_REGISTER));
    /* WRITE-CHAR — expose bocl_write_char as a Lisp built-in. */
    {   Ref wc_fn = symbol_intern_cstring("WRITE-CHAR");
        Ref wc_ll = list(symbol_intern_cstring("CHARACTER"),
                         S(&OPTIONAL), symbol_intern_cstring("STREAM"), NULL);
        set_symbol_function(wc_fn,built_in_function_new(wc_fn,wc_ll,kernel_WRITE_CHAR));}
    /* Slice 22 — I/O, pathnames, type-of, array, stream predicates. */
    {   Ref ll_s=list(S(STREAM),NULL);
        Ref ll_s_opt=list(S(STREAM),S(&REST),symbol_intern_cstring("KEYS"),NULL);
        Ref ll_b_s=list(symbol_intern_cstring("BYTE"),S(STREAM),NULL);
        Ref ll_s_o2=list(S(STREAM),S(&OPTIONAL),symbol_intern_cstring("EOF-ERROR-P"),symbol_intern_cstring("EOF-VALUE"),NULL);
        Ref ll_x=list(S(X),NULL);
        Ref ll_obj=list(symbol_intern_cstring("OBJECT"),NULL);
        Ref ll_arr=list(symbol_intern_cstring("ARRAY"),NULL);
        Ref ll_f=list(symbol_intern_cstring("FILESPEC"),NULL);
        Ref ll_f2=list(symbol_intern_cstring("FILESPEC"),symbol_intern_cstring("NEW-NAME"),NULL);
        Ref ll_f_o=list(symbol_intern_cstring("PATHNAME"),S(&OPTIONAL),symbol_intern_cstring("FIELD"),NULL);
        set_symbol_function(symbol_intern_cstring("CLOSE"),
            built_in_function_new(symbol_intern_cstring("CLOSE"),ll_s_opt,kernel_CLOSE));
        set_symbol_function(symbol_intern_cstring("READ-BYTE"),
            built_in_function_new(symbol_intern_cstring("READ-BYTE"),ll_s_o2,kernel_READ_BYTE));
        set_symbol_function(symbol_intern_cstring("WRITE-BYTE"),
            built_in_function_new(symbol_intern_cstring("WRITE-BYTE"),ll_b_s,kernel_WRITE_BYTE));
        set_symbol_function(symbol_intern_cstring("PROBE-FILE"),
            built_in_function_new(symbol_intern_cstring("PROBE-FILE"),ll_f,kernel_PROBE_FILE));
        set_symbol_function(symbol_intern_cstring("DELETE-FILE"),
            built_in_function_new(symbol_intern_cstring("DELETE-FILE"),ll_f,kernel_DELETE_FILE));
        set_symbol_function(symbol_intern_cstring("RENAME-FILE"),
            built_in_function_new(symbol_intern_cstring("RENAME-FILE"),ll_f2,kernel_RENAME_FILE));
        set_symbol_function(symbol_intern_cstring("FILE-WRITE-DATE"),
            built_in_function_new(symbol_intern_cstring("FILE-WRITE-DATE"),ll_f,kernel_FILE_WRITE_DATE));
        set_symbol_function(symbol_intern_cstring("FILE-LENGTH"),
            built_in_function_new(symbol_intern_cstring("FILE-LENGTH"),ll_s,kernel_FILE_LENGTH));
        set_symbol_function(symbol_intern_cstring("%COPY-FILE"),
            built_in_function_new(symbol_intern_cstring("%COPY-FILE"),ll_f2,kernel_COPY_FILE));
        set_symbol_function(symbol_intern_cstring("WILD-PATHNAME-P"),
            built_in_function_new(symbol_intern_cstring("WILD-PATHNAME-P"),ll_f_o,kernel_WILD_PATHNAME_P));
        set_symbol_function(symbol_intern_cstring("TYPE-OF"),
            built_in_function_new(symbol_intern_cstring("TYPE-OF"),ll_obj,kernel_TYPE_OF));
        set_symbol_function(symbol_intern_cstring("INT-CHAR"),
            built_in_function_new(symbol_intern_cstring("INT-CHAR"),ll_x,kernel_INT_CHAR));
        set_symbol_function(symbol_intern_cstring("ARRAY-DIMENSIONS"),
            built_in_function_new(symbol_intern_cstring("ARRAY-DIMENSIONS"),ll_arr,kernel_ARRAY_DIMENSIONS));
        set_symbol_function(symbol_intern_cstring("ARRAY-ELEMENT-TYPE"),
            built_in_function_new(symbol_intern_cstring("ARRAY-ELEMENT-TYPE"),ll_arr,kernel_ARRAY_ELEMENT_TYPE));
        /* INPUT-STREAM-P, OUTPUT-STREAM-P, OPEN-STREAM-P already registered
           in kernel_initialize_built_in_functions() — skip here. */
    }
    /* Slice 23 — timing, write-string, sleep, clear-input. */
    {   Ref ll_e   = list(S(&REST),symbol_intern_cstring("ARGS"),NULL);
        Ref ll_str = list(symbol_intern_cstring("STRING"),S(&REST),symbol_intern_cstring("OPTS"),NULL);
        Ref ll_s_o = list(S(SECONDS),NULL);
        Ref ll_si  = list(S(STREAM),S(&OPTIONAL),NULL);
        set_symbol_function(symbol_intern_cstring("GET-UNIVERSAL-TIME"),
            built_in_function_new(symbol_intern_cstring("GET-UNIVERSAL-TIME"),ll_e,kernel_GET_UNIVERSAL_TIME));
        set_symbol_function(symbol_intern_cstring("GET-INTERNAL-REAL-TIME"),
            built_in_function_new(symbol_intern_cstring("GET-INTERNAL-REAL-TIME"),ll_e,kernel_GET_INTERNAL_REAL_TIME));
        set_symbol_function(symbol_intern_cstring("GET-INTERNAL-RUN-TIME"),
            built_in_function_new(symbol_intern_cstring("GET-INTERNAL-RUN-TIME"),ll_e,kernel_GET_INTERNAL_RUN_TIME));
        set_symbol_function(symbol_intern_cstring("WRITE-STRING"),
            built_in_function_new(symbol_intern_cstring("WRITE-STRING"),ll_str,kernel_WRITE_STRING));
        /* The prelude's write-string calls kernel-write-string internally.
           Register it with positional (string stream start end) args. */
        Ref ll_ws4=list(symbol_intern_cstring("STRING"),S(STREAM),
                        symbol_intern_cstring("START"),symbol_intern_cstring("END"),NULL);
        set_symbol_function(symbol_intern_cstring("KERNEL-WRITE-STRING"),
            built_in_function_new(symbol_intern_cstring("KERNEL-WRITE-STRING"),ll_ws4,kernel_WRITE_STRING));
        set_symbol_function(symbol_intern_cstring("SLEEP"),
            built_in_function_new(symbol_intern_cstring("SLEEP"),ll_s_o,kernel_SLEEP));
        set_symbol_function(symbol_intern_cstring("CLEAR-INPUT"),
            built_in_function_new(symbol_intern_cstring("CLEAR-INPUT"),ll_si,kernel_CLEAR_INPUT));
        /* RANDOM / MAKE-RANDOM-STATE */
        Ref ll_rand=list(symbol_intern_cstring("LIMIT"),S(&OPTIONAL),symbol_intern_cstring("STATE"),NULL);
        Ref ll_rand2=list(S(&OPTIONAL),symbol_intern_cstring("STATE"),NULL);
        set_symbol_function(symbol_intern_cstring("RANDOM"),
            built_in_function_new(symbol_intern_cstring("RANDOM"),ll_rand,kernel_RANDOM_FN));
        set_symbol_function(symbol_intern_cstring("MAKE-RANDOM-STATE"),
            built_in_function_new(symbol_intern_cstring("MAKE-RANDOM-STATE"),ll_rand2,kernel_MAKE_RANDOM_STATE));
        /* Slice 24 — KERNEL-FORMAT-RUN (stream ctrl-string start end args) */
        Ref ll_fmt=list(S(STREAM),symbol_intern_cstring("CTRL"),
                        symbol_intern_cstring("START"),symbol_intern_cstring("END"),
                        symbol_intern_cstring("ARGS"),NULL);
        Ref frf=symbol_intern_cstring("KERNEL-FORMAT-RUN");
        set_symbol_function(frf,built_in_function_new(frf,ll_fmt,kernel_FORMAT_RUN));
    }
    /* Slice 25 — environment, PARSE-FLOAT, compiler stubs. */
    {   Ref ll_0=list(NULL);
        Ref ll_s=list(symbol_intern_cstring("STRING"),
                      S(&REST),symbol_intern_cstring("KEYS"),NULL);
        Ref ll_n=list(symbol_intern_cstring("NAME"),
                      S(&OPTIONAL),symbol_intern_cstring("DEFINITION"),NULL);
        Ref ll_f=list(symbol_intern_cstring("INPUT-FILE"),
                      S(&REST),symbol_intern_cstring("KEYS"),NULL);
        Ref ll_x=list(S(&OPTIONAL),S(X),NULL);
        set_symbol_function(symbol_intern_cstring("MACHINE-TYPE"),
            built_in_function_new(symbol_intern_cstring("MACHINE-TYPE"),ll_0,kernel_MACHINE_TYPE));
        set_symbol_function(symbol_intern_cstring("MACHINE-INSTANCE"),
            built_in_function_new(symbol_intern_cstring("MACHINE-INSTANCE"),ll_0,kernel_MACHINE_INSTANCE));
        set_symbol_function(symbol_intern_cstring("MACHINE-VERSION"),
            built_in_function_new(symbol_intern_cstring("MACHINE-VERSION"),ll_0,kernel_MACHINE_VERSION));
        set_symbol_function(symbol_intern_cstring("SOFTWARE-TYPE"),
            built_in_function_new(symbol_intern_cstring("SOFTWARE-TYPE"),ll_0,kernel_SOFTWARE_TYPE));
        set_symbol_function(symbol_intern_cstring("SOFTWARE-VERSION"),
            built_in_function_new(symbol_intern_cstring("SOFTWARE-VERSION"),ll_0,kernel_SOFTWARE_VERSION));
        set_symbol_function(symbol_intern_cstring("SHORT-SITE-NAME"),
            built_in_function_new(symbol_intern_cstring("SHORT-SITE-NAME"),ll_0,kernel_SHORT_SITE_NAME));
        set_symbol_function(symbol_intern_cstring("LONG-SITE-NAME"),
            built_in_function_new(symbol_intern_cstring("LONG-SITE-NAME"),ll_0,kernel_LONG_SITE_NAME));
        set_symbol_function(symbol_intern_cstring("PARSE-FLOAT"),
            built_in_function_new(symbol_intern_cstring("PARSE-FLOAT"),ll_s,kernel_PARSE_FLOAT));
        set_symbol_function(symbol_intern_cstring("COMPILE"),
            built_in_function_new(symbol_intern_cstring("COMPILE"),ll_n,kernel_COMPILE));
        set_symbol_function(symbol_intern_cstring("COMPILE-FILE"),
            built_in_function_new(symbol_intern_cstring("COMPILE-FILE"),ll_f,kernel_COMPILE_FILE));
        set_symbol_function(symbol_intern_cstring("DISASSEMBLE"),
            built_in_function_new(symbol_intern_cstring("DISASSEMBLE"),ll_x,kernel_DISASSEMBLE));
        set_symbol_function(symbol_intern_cstring("ED"),
            built_in_function_new(symbol_intern_cstring("ED"),ll_x,kernel_ED));
        /* Slice 25b — EVAL, MACROEXPAND-1, MACROEXPAND */
        Ref ll_form=list(symbol_intern_cstring("FORM"),NULL);
        Ref ll_form_e=list(symbol_intern_cstring("FORM"),S(&OPTIONAL),
                           symbol_intern_cstring("ENV"),NULL);
        set_symbol_function(symbol_intern_cstring("EVAL"),
            built_in_function_new(symbol_intern_cstring("EVAL"),ll_form,kernel_EVAL));
        set_symbol_function(symbol_intern_cstring("MACROEXPAND-1"),
            built_in_function_new(symbol_intern_cstring("MACROEXPAND-1"),ll_form_e,kernel_MACROEXPAND_1));
        set_symbol_function(symbol_intern_cstring("MACROEXPAND"),
            built_in_function_new(symbol_intern_cstring("MACROEXPAND"),ll_form_e,kernel_MACROEXPAND));
    }}

void bocl_call(Ref fname,Ref arguments){
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    princ(T("Calling: "),out);
    prin1(list(fname,arguments,NULL),out);
    print(symbol_function(fname),out);
    terpri(out);
    MultipleValues results={0};
    kernel_apply(symbol_function(fname),
                 arguments,
                 &results);
    const char* sep="--> ";
    for(uword i=0;i<results.count;i++){
        printf("%s",sep);
        sep="    ";
        prin1(results.values[i],out);
        terpri(out);}}

void bocl_test(void){
    prin1(S(PAPA),variable_value(S(*STANDARD-OUTPUT*)));
    terpri(variable_value(S(*STANDARD-OUTPUT*)));
    bocl_call(S(CAR),list(list(S(FOO),NULL),NULL));}


/**** THE END ****/

