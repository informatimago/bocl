#ifndef stream_h
#define stream_h
#include "kernel.h"
#include "macros.h"
#include "kernel_functions.h"
/* The built-in-functions are declared in kernel_functions.h */

void stream_initialize(void);

FILE* stream_output_cstream(Ref stream);
FILE* stream_input_cstream(Ref stream);
Ref   stream_close(Ref stream);
Ref   stream_close_any(Ref stream);
bool  stream_input_stream_p(Ref stream);
bool  stream_output_stream_p(Ref stream);
Ref   stream_write_byte(Ref byte_ref, Ref stream);
Ref   stream_read_byte(Ref stream, Ref eof_error_p, Ref eof_value);

#endif
