#include <stdio.h>
#include <stdarg.h>
#include "memory.h"
#include "kernel.h"
#include "macros.h"
             
/* Error Handling */


_Noreturn void fatal(const char* fname,char* message_format,...){
    va_list ap;
    fprintf(stderr,"\nFATAL ERROR: in %s:  ",fname);
    va_start(ap,message_format);
    vfprintf(stderr,message_format,ap);
    va_end(ap);
    fprintf(stderr,"\n\n");
    exit(1);}

_Noreturn void error(const char* fname,char* message_format,...){
    // TODO: Implement error() with the error handling system.
    va_list ap;
    fprintf(stderr,"\nERROR: in %s:  ",fname);
    va_start(ap,message_format);
    vfprintf(stderr,message_format,ap);
    va_end(ap);
    fprintf(stderr,"\n\n");
    exit(2);}


_Noreturn void type_error(const char* fname,Object* object,Object* class){
    // TODO: Implement type_error() with the error handling system.
    char* got=string_cstring(object->class->slots->class.name->slots->symbol.name);
    char* exp=string_cstring( class->class->slots->class.name->slots->symbol.name);
    fprintf(stderr,"\nTYPE ERROR: in %s:  got an object of class %s, expected an object of class %s\n\n",
            fname,got,exp);
    free(exp);
    free(got);
    exit(3);}


void* check_pointer(const char* fname,void* pointer){
    if(pointer){
        return pointer;}
    fatal(fname,"OUT OF MEMORY");}



/*

* Simple Heap Manager with Simple Garbage Collector

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

#+BEGIN_CODE C
    typedef struct Block{
        halfword size;
        halfword objcount;
        struct Block* blocks[0];
    } Block;
#+END_CODE

All allocated data structures have this structure (and may be followed
by additionnal fields after objcount Objects*, that are not references
to other Blocks (Object or Slots).

Blocks are allocated in whole number of words.  (64-bit or 32-bit
words, depending on the architecture).  The minimum size of a Block is
2 words: a free Block contains one reference to the next free Block.
Contiguous free Blocks are merged, to reduce fragmentation.  Free
blocks have their objcount set to -1, to help identifying the
contiguous free blocks for merging.  The free list is sorted by
decreasing block size.

During the mark phase of the garbage collector, the blocks are marked
by setting the high bit of the objcount field.

*/

typedef struct FreeBlock {
    BLOCK_HEADER;
    struct FreeBlock* next;
} FreeBlock;


typedef struct Zone {
    uword size; /* total Zone size */
    uword offset_to_contents; /* offset to first Block */
    FreeBlock free_list; /* free_list.next points to the first free block. */
    FreeBlock* before_best_candidate;
    struct Zone* next_zone;
} Zone;


#define DefaultZoneSize  (16UL*1024UL*1024UL-16UL)
#define MinimumBlockSize (sizeof(FreeBlock))

Zone* Zones=NULL;


Block* first_block(Zone* zone){
    return (Block*)(((char*)zone)+zone->offset_to_contents);}

Block* end_block(Zone* zone){
    return (Block*)(((char*)zone)+zone->size);}

Block* next_block(Block* block){
    return (Block*)(((char*)block)+block->size);}

bool free_block_p(Block* block){
    return block->objcount==FREE_BLOCK;}

bool marked_block_p(Block* block){
    return 0!=(block->objcount&(~(FREE_BLOCK>>1)));}

void mark_block(Block* block){
    block->objcount|=(~(FREE_BLOCK>>1));}

void clear_block(Block* block){
    block->objcount&=(FREE_BLOCK>>1);}



Zone* zone_allocate(size_t size){
    /* Allocate and initialize a new zone of the given total size. */
    Zone* zone=check_pointer(__FUNCTION__,malloc(size));
    zone->size=size;
    zone->offset_to_contents=sizeof(Zone);
    zone->free_list.size=sizeof(zone->free_list);
    zone->free_list.objcount=FREE_BLOCK;
    zone->free_list.next=(FreeBlock*)first_block(zone);
    zone->free_list.next->size=size-sizeof(Zone);
    zone->free_list.next->objcount=FREE_BLOCK;
    zone->free_list.next->next=NULL;
    zone->before_best_candidate=NULL;
    zone->next_zone=Zones;
    Zones=zone;
    return zone;}

#define do_zones(zonevar)                                                   \
    Zone* C(current_zone,__LINE__) ; Zone* zonevar;                         \
    for( C(current_zone,__LINE__) = zonevar = Zones ;                       \
         C(current_zone,__LINE__) != NULL ;                                 \
         (C(current_zone,__LINE__) = C(current_zone,__LINE__)->next_zone,   \
          zonevar = C(current_zone,__LINE__)))

FreeBlock* find_before_free_block(Zone* zone,FreeBlock* free_block){
    FreeBlock* current=&zone->free_list;
    while((current->next!=NULL)&&(current->next!=free_block)){
        current=current->next;}
    return (current->next==NULL)
            ?NULL
            :current;}

void insert_free_block(Zone* zone,FreeBlock* new){
    FreeBlock* previous=&zone->free_list;
    if(previous->next==NULL){
        previous->next=new;}
    else{
        while(previous->next){
            if(previous->next->size < new->size){
                /* insert here */
                new->next=previous->next;
                previous->next=new;
                break;
            }
            /* insert farther */
            previous=previous->next;}}}

FreeBlock* before_best_free_block(FreeBlock* free_list,halfword size){
    /* 
    The first free block is free_list->next.
    If the best free block is the first one, then return free_list,
    else return the block that points to the best one (result->next). 
    If no free block is good, then return NULL.
    */
    while(free_list->next){
        if(free_list->next->size<size){
            /* no good free block */
            return NULL;}
        if(free_list->next->size==size){
            return free_list;}
        if(free_list->next->next==NULL){
            /* last chance */
            return free_list;}
        free_list=free_list->next;}
    return NULL;}

Block* allocate_in_best_zone(Zone* zone,halfword size){
    /*
    Assume zone is the best zone, and zone->before_best_candidate
    points to the best candidate free block.
    Allocates a block from it.
    */
    FreeBlock* free=zone->before_best_candidate;
    halfword remaining=free->next->size-size;
    if(MinimumBlockSize<=remaining){
        /* split */
        Block* block=(Block*)((char*)(free->next)+remaining);
        free->next->size=remaining;
        block->size=size;
        block->objcount=0;
        return block;
    }else{
        Block* block=(Block*)free->next;
        free->next=free->next->next;
        block->objcount=0;
        return block;}}

Block* allocate_in_zone(Zone* zone,halfword size){
    /*
    Finds the best candidate free block in the zone, and allocates a
    block from it.  If no suitable free block exit in the zone, then
    return NULL. (GC might be in order).
    */
    zone->before_best_candidate=before_best_free_block(&(zone->free_list),size);
    return (zone->before_best_candidate==NULL)
            ?NULL
            :allocate_in_best_zone(zone,size);}

Block* allocate_in_zones(halfword size){
    /*
    Search the best suitable free zone in all the zones, 
    and allocate from it, unless non exist then NULL is returned.
    */
    Zone* best_zone=NULL;
    FreeBlock* candidate=NULL;
    {do_zones(zone){
            candidate=before_best_free_block(&(zone->free_list),size);
            zone->before_best_candidate=candidate;
            if(candidate!=NULL){
                if(best_zone==NULL){
                    best_zone=zone;}
                else{
                    if(zone->before_best_candidate->size
                       < best_zone->before_best_candidate->size){
                        best_zone=zone;}}
                if(candidate->size==size){
                    /* good candidate */
                    break;}}}}
    return best_zone
            ?allocate_in_best_zone(best_zone,size)
            :NULL;}

void* allocate(size_t size){
    /*
    Allocates a block of given size (including block header).
    If no free block is available, the garbage collector is invoked (unless disabled).
    */
    halfword hsize=((size+sizeof(word)-1)/sizeof(word))*sizeof(word);
    Block* block=NULL;
    if(DefaultZoneSize<hsize+sizeof(Zone)){
        Zone* new_zone=zone_allocate(hsize+sizeof(Zone));
        block=allocate_in_zone(new_zone,hsize);}
    else{
        block=allocate_in_zones(hsize);
        if(block==0){
            if(garbage_collection_is_enabled()){
                garbage_collect();
                block=allocate_in_zones(hsize);}
            if(block==0){
                Zone* new_zone=zone_allocate(DefaultZoneSize);
                block=allocate_in_zone(new_zone,hsize);}}}
    return block;}


static bool garbage_collection_enabled=true;

bool garbage_collection_is_enabled(void){
    return garbage_collection_enabled;}

bool set_garbage_collection_enabled(bool on){
    bool result=garbage_collection_enabled;
    garbage_collection_enabled=on;
    return result;}

void mark(Block* rootset){
    if(rootset && !marked_block_p(rootset)){
        mark_block(rootset);
        for(uword i=0;i<rootset->objcount;i++){
            mark(rootset->blocks[i]);}}}


Block* free_block(Zone* zone,Block* block){
    /* if it's contiguous to free blocks, then join them */
    /* Damned, to test if the previous block is free, we'd have to
    walk the whole freelist! for each garbage block!  */
    Block* next=next_block(block);
    if(free_block_p(next)){
        FreeBlock* previous=find_before_free_block(zone,(FreeBlock*)next);
        if(previous==NULL){
            fatal(__FUNCTION__,"Cannot find the free block in the free block list (should not occur)!");}
        FreeBlock* joined=previous->next;
        /* remove the joined free block */
        previous->next=joined->next;
        /* update the new free block */
        FreeBlock* new=(FreeBlock*)block;
        new->size+=joined->size;
        new->objcount=FREE_BLOCK;
        new->next=NULL;
        insert_free_block(zone,new);
        return (Block*)new;}
    else{
        /* update the new free block */
        FreeBlock* new=(FreeBlock*)block;
        new->objcount=FREE_BLOCK;
        new->next=NULL;
        insert_free_block(zone,new);
        return (Block*)new;}}

uword sweep_zone(Zone* zone){
    uword total=0;
    Block* end=end_block(zone);
    Block* current=first_block(zone);
    do{
        if(free_block_p(current)){
            /* stay free */
        }else if(marked_block_p(current)){
            /* alive! */
            clear_block(current);
        }else{
            /* garbage */
            total+=current->size;
            current=free_block(zone,current);}
        current=next_block(current);
    }while(current<end);
    return total;}

uword sweep(void){
    uword total=0;
    {do_zones(zone){
            total+=sweep_zone(zone);}}
    return total;}

Block* rootset=NULL;
void set_rootset(Block* block){
    rootset=block;}

uword garbage_collect(void){
    if(!garbage_collection_enabled){
        return 0;}
    mark(rootset);
    return sweep();}

void memory_initialize(void){
    zone_allocate(DefaultZoneSize);
}

/**** THE END ****/
