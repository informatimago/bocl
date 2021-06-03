#ifndef bocl_h
#define bocl_h
#include "kernel.h"

Ref bocl_read_eval_loop_cstring(const char* text);
Ref bocl_read_eval_cstring(const char* text);
Ref bocl_read_cstring(const char* text);
Ref bocl_load_cstring(const char* path);
Ref bocl_in_package(Ref package_designator);

#endif
