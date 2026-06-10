#ifndef memory_h
#define memory_h
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "kernel_types.h"

void* allocate(size_t size);

/* Use-after-free finder (BOCL_POISON); see memory.c.  class_of() reports a use
   of a poisoned (freed-and-quarantined) block. */
extern bool gc_poison_mode;
_Noreturn void gc_poison_report(Ref object);

bool garbage_collection_is_enabled(void);
bool set_garbage_collection_enabled(bool on);
uword garbage_collect(void);

void set_rootset(Block* block);
uword open_live_pool(void);
void add_to_live_pool(Ref object);
void close_live_pool(uword pool);

void memory_initialize(void);
void memory_dump(FILE* out);

#endif
