#ifndef printer_h
#define printer_h
#include "kernel_types.h"
#include "kernel_functions.h"
/* The built-in-functions are declared in kernel_functions.h */

/* Output primitives added for Slice 10. */
Ref bocl_write_char(Ref ch, Ref stream);
Ref stream_line_start_p(Ref stream);
Ref bocl_write_string(Ref string, Ref stream, Ref start_ref, Ref end_ref);
Ref bocl_fresh_line(Ref stream);

/* Kernel-level FORMAT runner.
   Processes ctrl[start..end), consuming arguments from *args_ptr.
   Returns the remaining argument list (for ~{~} nesting).
   The caller owns *args_ptr; this function mutates it in place. */
Ref bocl_format_run(Ref stream, Ref ctrl_string, uword start, uword end, Ref* args_ptr);

/* Registered built-in: kernel-format-run stream ctrl start end args */
void kernel_FORMAT_RUN(Ref function, Ref arguments, MultipleValues* results);

/* Set the *PRINT-LEVEL* depth counter (used by the prelude array printer to sync
   element nesting depth before delegating to PRIN1/PRINC). */
void printer_set_level_depth(word d);

/* *PRINT-CIRCLE* bridge for the prelude array printer.  printer_circle_active
   reports whether circle printing is on; printer_circle_action returns a
   positive N (back-reference, print "#N#"), a negative -N (shared first visit:
   assign N, print "#N=" then the object), or 0 (print the object normally). */
bool printer_circle_active(void);
word printer_circle_action(Ref obj);

#endif
