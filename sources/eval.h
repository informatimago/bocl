#ifndef eval_h
#define eval_h
#include "kernel.h"

Ref evale(Ref form,Ref environment,MultipleValues* results);
Ref eval_global_environment(void);
/* Run BODY(ctx) under a top-level CATCH for BOCL:QUIT (non-local exit). */
Ref bocl_run_with_toplevel_catch(Ref (*body)(void*), void* ctx);

#endif
