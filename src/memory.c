#include <stdio.h>
#include "memory.h"
#include "kernel.h"


/*

Simple Heap Manager with Simple Garbage Collector

The Memory Manager allocates memory zones using malloc, and keep a
list of zones.

It allocates memory blocks from the zones, or (for big blocks)
creating a new specially sized zone using malloc.

To keep it simple and efficient, we implement a simple mark-and-sweep
garbage collector.  We trade potential fragmenting, with speed,
avoiding to copy live data.

The Garbage Collector needs to know:
- the size of a block
- where to find the references; for this, we gather all the references
  at the start of a block, and give an objcount.

    typedef struct Block{
        halfword size;
        halfword objcount;
        struct Block* blocks[0];
    } Block;

All allocated data structures have this structure (and may be followed
by additionnal fields after objcount Objects*, that are not references
to other Blocks (Object or Slots).

Blocks are allocated in whole number of words.  (64-bit or 32-bit
words, depending on the architecture).  The minimum size of a Block is
2 words: a free Block contains one reference to the next free Block.
Contiguous free Blocks are merged, to reduce fragmentation.

*/

typedef struct Zone {
    uword size; /* total Zone size */
    uword offset_to_contents; /* offset to first Block */
    Block* first_free_block; /* pointer to the free list */
} Zone;



_Noreturn void fatal(const char* fname,char* message){
    fprintf(stderr,"\nFATAL ERROR: in %s:  %s\n\n",fname,message);
    exit(1);}

_Noreturn void error(const char* fname,char* message){
    // TODO: Implement error() with the error handling system.
    fprintf(stderr,"\nERROR: in %s: %s\n\n",fname,message);
    exit(2);}

_Noreturn void type_error(const char* fname,Object* object,Object* class){
    // TODO: Implement type_error() with the error handling system.
    char* got=string_cstring(class_name(class_of(object)));
    char* exp=string_cstring(class_name(class));
    fprintf(stderr,"\nTYPE ERROR: function %s got an object of class %s, expected an object of class %s\n\n",
            fname,got,exp);
    free(exp);
    free(got);
    exit(2);}


void* check_pointer(const char* fname,void* pointer){
    if(pointer){
        return pointer;}
    fatal(fname,"Got a NULL pointer");}

uword garbage_collect(void){
    return 0;}

void* allocate(size_t size){
    halfword hsize=((size+sizeof(word)-1)/sizeof(word))*sizeof(word);
    Block* block=malloc(hsize);  // TODO: replace with actual GC memory management.
    if(block==0){
        garbage_collect();
        block=malloc(hsize); // TODO: replace with actual GC memory management.
        if(block==0){
            fatal(__FUNCTION__,OUT_OF_MEMORY);}}
    block->size=hsize;
    block->objcount=0;
    return block;
}



/**** THE END ****/
