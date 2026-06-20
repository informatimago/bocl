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
void set_gc_stress(long n);   /* runtime GC-stress period override (diagnostic) */
void set_gc_poison(bool on);  /* runtime poison-finder toggle (diagnostic)      */

bool garbage_collection_is_enabled(void);
bool set_garbage_collection_enabled(bool on);
uword garbage_collect(void);

/* Generational write barrier: call AFTER storing a Ref into CONTAINER, where
   CONTAINER is the heap block that physically holds the mutated slot (the
   Slots block for obj->slots->X.field, the inner VectorOfObject for svref,
   the cons cell itself for rplaca/rplacd).  Records CONTAINER for the next
   minor collection if it is old; a no-op for young blocks. */
void gc_write_barrier(void* container);

/* GC-safe count of a block's leading Ref slots (masks off the mark/old flag
   bits).  Use instead of reading obj->...objcount directly when the value
   means "number of element/Ref slots". */
uword object_slot_count(void* object);

void set_rootset(Block* block);
uword open_live_pool(void);
void add_to_live_pool(Ref object);
void close_live_pool(uword pool);

void memory_initialize(void);
void memory_dump(FILE* out);

#endif
