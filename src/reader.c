#include "reader.h"
#include "macros.h"
#include "kernel.h"
#include "variable.h"

/* The recursive_p parameters are ignored, since we don't deal with #=/## references */

Object* kernel_read(Object* input_stream,Object* eof_error_p,Object* eof_value,unused Object* recursive_p){
    NOT_IMPLEMENTED_YET();
    return S(NIL);
}

Object* kernel_read_preserving_whitespace(Object* input_stream,Object* eof_error_p,Object* eof_value,unused Object* recursive_p){
    NOT_IMPLEMENTED_YET();
    return S(NIL);
}

Object* kernel_read_delimited_list(Object* character,Object* input_stream,unused Object* recursive_p){
    NOT_IMPLEMENTED_YET();
    return S(NIL);
}

Object* kernel_read_from_string(Object* input_string,Object* eof_error_p,Object* eof_value,Object* start,Object* end,Object* preserve_whitespace){
    NOT_IMPLEMENTED_YET();
    return S(NIL);
}


void reader_initialize(){
    define_parameter(READ_BASE,integer_from_word(10));
    define_parameter(READ_DEFAULT_FLOAT_FORMAT,S(SINGLE-FLOAT));
    define_parameter(READ_EVAL,S(T));
    define_parameter(READ_SUPPRESS,S(NIL));
    define_parameter(READTABLE,S(NIL));}


/**** THE END ****/
