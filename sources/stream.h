#ifndef stream_h
#define stream_h
#include "kernel.h"
#include "macros.h"
#include "kernel_functions.h"
/* The built-in-functions are declared in kernel_functions.h */

void stream_initialize(void);

FILE* stream_output_cstream(Ref stream);

#endif
