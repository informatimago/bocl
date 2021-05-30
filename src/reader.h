#ifndef reader_h
#define reader_h
#include "kernel_types.h"

/* The variables ar ignored, the kernel reader is hardwired. */

#define READ_BASE                 S(*READ-BASE*)
#define READ_DEFAULT_FLOAT_FORMAT S(*READ-DEFAULT-FLOAT-FORMAT*)
#define READ_EVAL                 S(*READ-EVAL*)
#define READ_SUPPRESS             S(*READ-SUPPRESS*)
#define READTABLE                 S(*READTABLE*)

/* The recursive_p parameters are ignored, since we don't deal with #=/## references */

Object* kernel_read(Object* input_stream,Object* eof_error_p,Object* eof_value,Object* recursive_p);
Object* kernel_read_preserving_whitespace(Object* input_stream,Object* eof_error_p,Object* eof_value,Object* recursive_p);
Object* kernel_read_delimited_list(Object* character,Object* input_stream,Object* recursive_p);
Object* kernel_read_from_string(Object* input_string,Object* eof_error_p,Object* eof_value,Object* start,Object* end,Object* preserve_whitespace);


void reader_initialize();

#endif
