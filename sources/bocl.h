#ifndef bocl_h
#define bocl_h
#include "kernel.h"


Ref bocl_eval(Ref sexp);
Ref bocl_read_cstring(const char* text);
Ref bocl_read_eval_cstring(const char* text);
Ref bocl_repl(Ref inp,Ref out);
Ref bocl_read_eval_loop_cstring(const char* text);
Ref bocl_in_package(Ref package_designator);
Ref bocl_load_cstring(const char* path);

void bocl_call(Ref fname,Ref arguments);
void bocl_test(void);

#endif
