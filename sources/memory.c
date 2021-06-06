#include <stdio.h>
#include <stdarg.h>
#include <sysexits.h>
#include "memory.h"
#include "boerror.h"
#include "kernel.h"
#include "macros.h"
#include "variable.h"
#include "stream.h"
#include "printer.h"


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


static Block* first_block(Zone* zone){
    return (void*)(((char*)zone)+zone->offset_to_contents);}

static Block* end_block(Zone* zone){
    return (void*)(((char*)zone)+zone->size);}

static Block* next_block(Block* block){
    return (void*)(((char*)block)+block->size);}

static bool free_block_p(Block* block){
    return block->objcount==FREE_BLOCK;}

static bool marked_block_p(Block* block){
    return 0!=(block->objcount&(~(FREE_BLOCK>>1)));}

static void mark_block(Block* block){
    block->objcount|=(~(FREE_BLOCK>>1));}

static void clear_block(Block* block){
    block->objcount&=(FREE_BLOCK>>1);}

static halfword block_objcount(Block* block){
    return block->objcount&(FREE_BLOCK>>1);}


static Zone* zone_allocate(size_t size){
    /* Allocate and initialize a new zone of the given total size. */
    Zone* zone=CHECK_POINTER(malloc(size));
    zone->size=CHECK_SIZE_TO_UWORD(size);
    zone->offset_to_contents=sizeof(Zone);
    zone->free_list.size=sizeof(zone->free_list);
    zone->free_list.objcount=FREE_BLOCK;
    zone->free_list.next=(FreeBlock*)first_block(zone);
    zone->free_list.next->size=CHECK_SIZE_TO_HALFWORD(size-sizeof(Zone));
    zone->free_list.next->objcount=FREE_BLOCK;
    zone->free_list.next->next=NULL;
    zone->before_best_candidate=NULL;
    zone->next_zone=Zones;
    Zones=zone;
    return zone;}

#define do_zones(zonevar)                                                   \
    Zone* CS(current_zone,__LINE__) ; Zone* zonevar;                        \
    for( CS(current_zone,__LINE__) = zonevar = Zones ;                      \
         CS(current_zone,__LINE__) != NULL ;                                \
         (CS(current_zone,__LINE__) = CS(current_zone,__LINE__)->next_zone, \
          zonevar = CS(current_zone,__LINE__)))

static FreeBlock* find_before_free_block(Zone* zone,FreeBlock* free_block){
    FreeBlock* current=&zone->free_list;
    while((current->next!=NULL)&&(current->next!=free_block)){
        current=current->next;}
    return (current->next==NULL)
            ?NULL
            :current;}

static void insert_free_block(Zone* zone,FreeBlock* new){
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

static FreeBlock* before_best_free_block(FreeBlock* free_list,halfword size){
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

static Block* allocate_in_best_zone(Zone* zone,halfword size){
    /*
    Assume zone is the best zone, and zone->before_best_candidate
    points to the best candidate free block.
    Allocates a block from it.
    */
    FreeBlock* free=zone->before_best_candidate;
    halfword remaining=CHECK_SIZE_TO_HALFWORD(free->next->size-size);
    if(MinimumBlockSize<=remaining){
        /* split */
        Block* block=(void*)((char*)(free->next)+remaining);
        free->next->size=remaining;
        block->size=size;
        block->objcount=0;
        return block;
    }else{
        Block* block=(Block*)free->next;
        free->next=free->next->next;
        block->objcount=0;
        return block;}}

static Block* allocate_in_zone(Zone* zone,halfword size){
    /*
    Finds the best candidate free block in the zone, and allocates a
    block from it.  If no suitable free block exit in the zone, then
    return NULL. (GC might be in order).
    */
    zone->before_best_candidate=before_best_free_block(&(zone->free_list),size);
    return (zone->before_best_candidate==NULL)
            ?NULL
            :allocate_in_best_zone(zone,size);}

static Block* allocate_in_zones(halfword size){
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
    halfword hsize=CHECK_SIZE_TO_HALFWORD(((size+sizeof(word)-1)/sizeof(word))*sizeof(word));
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

static void mark(Block* rootset){
    if(rootset && !marked_block_p(rootset)){
        mark_block(rootset);
        for(uword i=0;i<block_objcount(rootset);i++){
            mark(rootset->blocks[i]);}}}


static Block* free_block(Zone* zone,Block* block){
    /* if it's contiguous to free blocks, then join them */
    /* Damned, to test if the previous block is free, we'd have to
    walk the whole freelist! for each garbage block!  */
    Block* next=next_block(block);
    if(free_block_p(next)){
        FreeBlock* previous=find_before_free_block(zone,(FreeBlock*)next);
        if(previous==NULL){
            FATAL("Cannot find the free block in the free block list %s!","(should not occur)");}
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

static uword sweep_zone(Zone* zone){
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

static uword sweep(void){
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


static bool in_root_set_p(Block* block){
    /* We assume the root set is a lisp list */
    Block* current=rootset;
    if(current==NULL){
        return false;}
    while(block_objcount(current)==2){
        if(current->blocks[0]==block){
            return true;}
        current=current->blocks[1];}
    return false;}

static bool in_free_list(FreeBlock* list,Block* block){
    while(list && ((void*)list!=(void*)block)){
        list=list->next;}
    return ((void*)list==(void*)block);}

void memory_dump(FILE* out){
    fprintf(out,"Zones:\n\n");
    {do_zones(zone){
            fprintf(out,"  zone %16p\n",(void*)zone);
            fprintf(out,"      size                         0x%08"UWORD_X_FORMAT" %12"UWORD_FORMAT"\n",zone->size,zone->size);
            fprintf(out,"      offset_to_contents           0x%08"UWORD_X_FORMAT" %12"UWORD_FORMAT"\n",zone->offset_to_contents,zone->offset_to_contents);
            fprintf(out,"      /contents              %16p\n",(void*)first_block(zone));
            fprintf(out,"      free_list              %16p\n",(void*)zone->free_list.next);
            fprintf(out,"      before_best_candidate  %16p\n",(void*)zone->before_best_candidate);
            fprintf(out,"      next_zone              %16p\n",(void*)zone->next_zone);
            fprintf(out,"      /end                   %16p\n",(void*)end_block(zone));
            fprintf(out,"\n");}}
     {do_zones(zone){
             fprintf(out,"    Blocks in zone %16p:\n\n",(void*)zone);
             Block* block=first_block(zone);
             Block* end=end_block(zone);
             while(block<end){
                 fprintf(out,"      block %16p",(void*)block);
                 if(in_root_set_p(block)){
                     fprintf(out," in-root-set");}
                 if(free_block_p(block)){
                     fprintf(out," free-block");}
                 if(in_free_list(&(zone->free_list),block)){
                     fprintf(out," in-free-list");}
                 if(!marked_block_p(block) && marked_block_p(block)){
                     fprintf(out," marked");}
                 fprintf(out,"\n");
                 fprintf(out,"          size                  0x%08"BH_X_FORMAT" %12"BH_FORMAT"\n",block->size,block->size);
                 if(!free_block_p(block)){
                     fprintf(out,"          objcount            %12"BH_FORMAT"\n",block_objcount(block));
                     for(uword i=0;i<block_objcount(block);i++){
                         fprintf(out,"          object[%2"UWORD_FORMAT"]      %16p\n",i,(void*)block->blocks[i]);}}
                 fprintf(out,"          /end            %16p\n\n",(void*)next_block(block));
                 block=next_block(block);}}}}

/**** THE END ****/
