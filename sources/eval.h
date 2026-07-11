#ifndef eval_h
#define eval_h
#include "kernel.h"

Ref evale(Ref form,Ref environment,MultipleValues* results);

/* Record (documentation NAME 'function) = DOC (no-op for NIL doc or before the
   Lisp helper exists).  Shared by DEFUN (eval.c) and BOCL:DEFINE-FUNCTION (bocl.c)
   so both set documentation the same way. */
void bocl_set_function_documentation(Ref name,Ref doc);

/* Control-stack guard limit / current usage (bytes) — for the slots of
   BOCL:OUT-OF-STACK-MEMORY-ERROR. */
size_t eval_stack_guard_limit(void);
size_t eval_stack_guard_used(void);
void eval_init_stack_guard(char* approx_base);
void eval_stack_guard_check(void);   /* shared guard — used by evale and bytecode_apply */
Ref eval_global_environment(void);
/* Run BODY(ctx) under a top-level CATCH for BOCL:QUIT (non-local exit). */
Ref bocl_run_with_toplevel_catch(Ref (*body)(void*), void* ctx);

#endif
