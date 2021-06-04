#include "bocl.h"
#include "memory.h"
#include "error.h"
#include "kernel.h"
#include "stream.h"
#include "reader.h"
#include "printer.h"
#include "variable.h"


Ref bocl_read_eval_loop_cstring(const char* text){
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    prin1(bocl_read_cstring(text),out);
    terpri(out);
    NOT_IMPLEMENTED_YET();
    return symbol_intern_cstring("NIL");}

Ref bocl_read_eval_cstring(const char* text){
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    prin1(bocl_read_cstring(text),out);
    terpri(out);
    NOT_IMPLEMENTED_YET();
    return symbol_intern_cstring("NIL");}

Ref bocl_read_cstring(const char* text){
    Ref stream=stream_input_string(string_new_cstring(text),I(0),NIL_Symbol);
    return kernel_read(stream,NIL_Symbol,NIL_Symbol,NIL_Symbol);}

Ref bocl_load_cstring(const char* path){
    NOT_IMPLEMENTED_YET();
    return symbol_intern_cstring("NIL");}

Ref bocl_in_package(Ref package_designator){
    NOT_IMPLEMENTED_YET();
    return symbol_intern_cstring("NIL");}


