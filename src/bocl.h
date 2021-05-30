#ifndef bocl_h
#define bocl_h
#include "kernel.h"

Object* bocl_read_eval_loop_cstring(const char* text);
Object* bocl_read_eval_cstring(const char* text);
Object* bocl_read_cstring(const char* text);
Object* bocl_load_cstring(const char* path);
Object* bocl_in_package(Object* package_designator);

#endif
