#ifndef bocl_h
#define bocl_h
#include "kernel.h"


Ref bocl_eval(Ref sexp);
Ref bocl_read_cstring(const char* text);
Ref bocl_read_eval_cstring(const char* text);
/* Evaluate a top-level form through the Lisp %REPL-EVAL wrapper, honouring
   BOCL:*ON-ERROR* (debug/appease/abort/exit) as the REPL does. */
Ref bocl_toplevel_eval(Ref sexp);
Ref bocl_toplevel_eval_cstring(const char* text);
Ref bocl_repl(Ref inp,Ref out);
Ref bocl_read_eval_loop_cstring(const char* text);
Ref bocl_in_package(Ref package_designator);
Ref bocl_load_cstring(const char* path);
/* Load cl-exports.lisp (sibling) then the prelude — use in test setups. */
Ref bocl_load_prelude(const char* prelude_path);

void bocl_register_load(void);
void bocl_call(Ref fname,Ref arguments);
void bocl_test(void);

#endif
