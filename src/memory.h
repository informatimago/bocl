#ifndef memory_h
#define memory_h
#include <stdlib.h>
#include <stdbool.h>
#include "kernel_types.h"

#define OUT_OF_MEMORY       "OUT OF MEMORY"
#define NOT_IMPLEMENTED_YET "NOT IMPLEMENTED YET"

_Noreturn void fatal(const char* fname,char* message);
_Noreturn void error(const char* fname,char* message);
_Noreturn void type_error(const char* fname,Object* object,Object* class);
void* check_pointer(const char* fname,void* pointer);

void* allocate(size_t size);

bool garbage_collection_is_enabled(void);
bool set_garbage_collection_enabled(bool on);
uword garbage_collect(void);

void set_rootset(Block* block);

void memory_initialize(void);

#endif
