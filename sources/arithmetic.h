#ifndef arithmetic_h
#define arithmetic_h
#include "kernel.h"

/* Phase 2 numeric primitives. All registered at startup by
   kernel_initialize_arithmetic(). Each is a 2-arg or 1-arg kernel function
   in the same KernelFunctionPtr shape as built_in_functions.c. The n-ary
   forms (+ a b c …) etc. are provided on the Lisp side once the macro
   system is up. */

void kernel_initialize_arithmetic(void);

#endif
