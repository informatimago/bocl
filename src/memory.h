#ifndef memory_h
#define memory_h
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "kernel_types.h"

void* allocate(size_t size);

bool garbage_collection_is_enabled(void);
bool set_garbage_collection_enabled(bool on);
uword garbage_collect(void);

void set_rootset(Block* block);

void memory_initialize(void);
void memory_dump(FILE* out);

#endif
