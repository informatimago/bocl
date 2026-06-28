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

/* An output FILE-STREAM whose writes funcall a Lisp WRITE-FUNCTION with the
   emitted bytes (a string).  This is the kernel hook enabling CL-level
   compound streams (broadcast / two-way / echo / synonym) and DRIBBLE: every
   printer path obtains its FILE* through stream_output_cstream(), so printing
   to such a stream redirects each byte through WRITE-FUNCTION.  Implemented in
   callback_stream.c with fopencookie (glibc) / funopen (BSD/macOS). */
Ref make_output_callback_stream(Ref write_function);
void kernel_MAKE_OUTPUT_CALLBACK_STREAM(Ref function,Ref arguments,MultipleValues* results);

#endif
