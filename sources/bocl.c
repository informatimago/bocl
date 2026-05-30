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

Ref bocl_repl(Ref inp,Ref out){
    terpri(out);
    while(true){
        princ(T("bocl> "),out);
        stream_finish_output(out);
        Ref sexp=kernel_read(inp,NIL_Symbol,inp,NIL_Symbol);
        if((sexp==inp)||equal(sexp,list(S(QUIT),NULL))){
            return I(0);}
        Ref result=bocl_eval(sexp);
        print(result,out);
        terpri(out);}}

Ref bocl_read_eval_loop_cstring(const char* text){
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    Ref inp=stream_input_string(string_new_cstring(text),I(0),NIL_Symbol);
    Ref result=bocl_repl(inp,out);
    stream_close(inp);
    return result;}

Ref bocl_in_package(Ref package_designator){
    Ref pkg = package_find(package_designator);
    if(nullp(pkg)){
        ERROR("IN-PACKAGE: no such package %s",
              string_cstring(prin1_to_string(package_designator)));}
    set_symbol_value(S(*PACKAGE*), pkg);
    return pkg;}

Ref bocl_load_cstring(const char* path){
    Ref out = variable_value(S(*STANDARD-OUTPUT*));

    bool do_verbose = is_true(variable_value(S(*LOAD-VERBOSE*)));
    bool do_print   = is_true(variable_value(S(*LOAD-PRINT*)));
    Ref inp = stream_open(physical_pathname_new_cstring(path),
                             K(INPUT),
                             S(CHARACTER),
                             NIL_Symbol,        /* if-exists: irrelevant for :input */
                             K(ERROR),          /* if-does-not-exist */
                             K(DEFAULT));
    if(do_verbose){
        princ(T(";; Loading "),out);
        princ(pathname(inp),out);
        terpri(out);
        stream_finish_output(out);}
    /* kernel_read on EOF returns the stream itself (we pass `inp` as the
       eof-value), so the loop test `sexp != inp` terminates at EOF. */
    Ref sexp=kernel_read(inp,NIL_Symbol,inp,NIL_Symbol);
    while(sexp!=inp){
        if(do_print){
            prin1(sexp,out);
            terpri(out);
            stream_finish_output(out);}
        bocl_eval(sexp);
        sexp=kernel_read(inp,NIL_Symbol,inp,NIL_Symbol);}
    stream_close(inp);
    if(do_verbose){
        princ(T(";; Loaded "),out);
        princ(pathname(inp),out);
        terpri(out);
        stream_finish_output(out);}

    return T_Symbol;}


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

/* APPEND is needed by backquote-expanded code; the kernel previously only
   shipped NCONC. */
extern Ref bocl_append(Ref lists);
extern Ref bocl_write_char(Ref ch, Ref stream);

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

void bocl_register_load(void){
    Ref fname=S(LOAD);
    set_symbol_function(fname,built_in_function_new(fname,list(S(FILESPEC),NULL),kernel_LOAD));
    Ref af=S(APPEND);
    set_symbol_function(af,built_in_function_new(af,list(S(&REST),S(LISTS),NULL),kernel_APPEND));
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
        set_symbol_function(wc_fn,built_in_function_new(wc_fn,wc_ll,kernel_WRITE_CHAR));}}

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

void bocl_test(){
    prin1(S(PAPA),variable_value(S(*STANDARD-OUTPUT*)));
    terpri(variable_value(S(*STANDARD-OUTPUT*)));
    bocl_call(S(CAR),list(list(S(FOO),NULL),NULL));}


/**** THE END ****/

