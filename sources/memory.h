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
uword open_live_pool(void);
void close_live_pool(uword pool);

#define WITH_LIVE_POOL uword memory_live_pool=open_live_pool()
#define LP_RETURN1(value) do{close_live_pool(memory_live_pool); return (value); }while(0)
#define LP_RETURN()       do{close_live_pool(memory_live_pool); return; }while(0)

void memory_initialize(void);
void memory_dump(FILE* out);

#endif
