#ifndef reader_h
#define reader_h
#include "kernel_types.h"
#include "kernel_functions.h"
/* The built-in-functions are declared in kernel_functions.h */

/* The variables ar ignored, the kernel reader is hardwired. */

#define READ_BASE                 S(*READ-BASE*)
#define READ_DEFAULT_FLOAT_FORMAT S(*READ-DEFAULT-FLOAT-FORMAT*)
#define READ_EVAL                 S(*READ-EVAL*)
#define READ_SUPPRESS             S(*READ-SUPPRESS*)
#define READTABLE                 S(*READTABLE*)

void reader_initialize(void);

#endif
