#ifndef memory_h
#define memory_h
#include <stdlib.h>
#include "kernel_types.h"

#define OUT_OF_MEMORY       "OUT OF MEMORY"
#define NOT_IMPLEMENTED_YET "NOT IMPLEMENTED YET"

_Noreturn void fatal(const char* fname,char* message);
_Noreturn void error(const char* fname,char* message);
_Noreturn void type_error(const char* fname,Object* object,Object* class);
void* check_pointer(const char* fname,void* pointer);

uword garbage_collect(void);
void* allocate(size_t size);

#endif
