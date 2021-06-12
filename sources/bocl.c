#include "bocl.h"
#include "memory.h"
#include "boerror.h"
#include "kernel.h"
#include "stream.h"
#include "reader.h"
#include "printer.h"
#include "variable.h"
#include "macros.h"


Ref bocl_eval(Ref sexp){
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
   princ(T(";; The Mighty Eval Function is not there yet, so your sexps are safe!"),out);
   print(sexp,out);
   terpri(out);
   return S(NIL);}

Ref bocl_read_cstring(const char* text){
    Ref stream=stream_input_string(string_new_cstring(text),I(0),NIL_Symbol);
    return kernel_read(stream,NIL_Symbol,NIL_Symbol,NIL_Symbol);}

Ref bocl_read_eval_cstring(const char* text){
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    Ref sexp=bocl_read_cstring(text);
    prin1(sexp,out);
    terpri(out);
    Ref result=bocl_eval(sexp);
    stream_finish_output(out);
    return result;}

Ref bocl_repl(Ref inp,Ref out){
    terpri(out);
    while(true){
        princ(T("bocl> "),out);
        stream_finish_output(out);
        Ref sexp=kernel_read(inp,NIL_Symbol,inp,NIL_Symbol);
        prin1(sexp,out);
        terpri(out);
        if((sexp==inp)||equal(sexp,list(S(QUIT),NULL))){
            return I(0);}
        Ref result=bocl_eval(sexp);
        print(result,out);
        terpri(out);}}

Ref bocl_read_eval_loop_cstring(const char* text){
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    Ref inp=stream_input_string(string_new_cstring(text),I(0),NIL_Symbol);
    return bocl_repl(inp,out);}

Ref bocl_in_package(Ref package_designator){
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    princ(T(";; Assume a package system is implemented and that we're in-package "),out);
    princ(package_designator,out);
    terpri(out);
    stream_finish_output(out);
    return S(NIL);}

Ref bocl_load_cstring(const char* path){
    Ref out = variable_value(S(*STANDARD-OUTPUT*));

    princ(T("STREAM-OPEN not implemented yet."),out);
    terpri(out);
    return NIL_Symbol;

    bool do_verbose = is_true(variable_value(S(*LOAD-VERBOSE*)));
    bool do_print   = is_true(variable_value(S(*LOAD-PRINT*)));
    Ref inp = stream_open(physical_pathname_new_cstring(path),
                             K(INPUT),
                             S(CHARACTER),
                             K(CREATE),
                             K(CREATE),
                             K(DEFAULT));
    if(do_verbose){
        princ(T(";; Loading "),out);
        princ(pathname(inp),out);
        terpri(out);}
    Ref sexp=kernel_read(inp,NIL_Symbol,inp,NIL_Symbol);
    while(sexp!=inp){
        if(do_print){
            prin1(sexp,out);
            terpri(out);}
        bocl_eval(sexp);
        sexp=kernel_read(inp,NIL_Symbol,inp,NIL_Symbol);}
    if(do_verbose){
        princ(T(";; Loaded "),out);
        princ(pathname(inp),out);
        terpri(out);}

    return S(NIL);}


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
    bocl_call(S(CAR),list(list(S(FOO),NULL),NULL));}


/**** THE END ****/

