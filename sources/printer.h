#ifndef printer_h
#define printer_h
#include "kernel_types.h"
#include "kernel_functions.h"
/* The built-in-functions are declared in kernel_functions.h */

/* Output primitives added for Slice 10. */
Ref bocl_write_char(Ref ch, Ref stream);
Ref bocl_write_string(Ref string, Ref stream, Ref start_ref, Ref end_ref);
Ref bocl_fresh_line(Ref stream);

/* Kernel-level FORMAT runner.
   Processes ctrl[start..end), consuming arguments from *args_ptr.
   Returns the remaining argument list (for ~{~} nesting).
   The caller owns *args_ptr; this function mutates it in place. */
Ref bocl_format_run(Ref stream, Ref ctrl_string, uword start, uword end, Ref* args_ptr);

/* Registered built-in: kernel-format-run stream ctrl start end args */
void kernel_FORMAT_RUN(Ref function, Ref arguments, MultipleValues* results);

#endif
