#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>
#include <sysexits.h>
#include <execinfo.h>
#include <stdint.h>
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


/* Segregated free-list threshold:
   Blocks >= LARGE_THRESHOLD go into large_list; smaller go into small_list.
   During allocation we always try large_list first — every block there is
   >= LARGE_THRESHOLD, so it satisfies any typical bocl allocation (<128 B)
   in O(1) without scanning.  The small_list is a fallback when large_list
   is exhausted, which is rare during normal operation.                      */
#define LARGE_THRESHOLD 128u

typedef struct Zone {
    uword size; /* total Zone size */
    uword offset_to_contents; /* offset to first Block */
    FreeBlock large_list; /* blocks >= LARGE_THRESHOLD; first is largest  */
    FreeBlock small_list; /* blocks <  LARGE_THRESHOLD; first is largest  */
    FreeBlock* before_best_candidate;
    struct Zone* next_zone;
} Zone;


#define DefaultZoneSize  (64UL*1024UL*1024UL-16UL)
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
    /* Initial free block is the entire zone body — always large. */
    zone->large_list.size=sizeof(zone->large_list);
    zone->large_list.objcount=FREE_BLOCK;
    zone->large_list.next=(FreeBlock*)first_block(zone);
    zone->large_list.next->size=CHECK_SIZE_TO_HALFWORD(size-sizeof(Zone));
    zone->large_list.next->objcount=FREE_BLOCK;
    zone->large_list.next->next=NULL;
    zone->small_list.size=sizeof(zone->small_list);
    zone->small_list.objcount=FREE_BLOCK;
    zone->small_list.next=NULL;
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

/* find_before_free_block and insert_free_block are legacy helpers used
   only by the (now unused) free_block() function. */
static FreeBlock* find_before_free_block(Zone* zone,FreeBlock* fb){
    FreeBlock* cur=&zone->large_list;
    while(cur->next && cur->next!=fb) cur=cur->next;
    if(cur->next==fb) return cur;
    cur=&zone->small_list;
    while(cur->next && cur->next!=fb) cur=cur->next;
    return (cur->next==fb)?cur:NULL;}

static void insert_free_block(Zone* zone,FreeBlock* new){
    /* Legacy path: insert into large_list or small_list by size. */
    if(new->size >= LARGE_THRESHOLD){
        new->next=zone->large_list.next;
        zone->large_list.next=new;
    }else{
        new->next=zone->small_list.next;
        zone->small_list.next=new;}}

static FreeBlock* before_best_free_block(FreeBlock* free_list,halfword size){
    /* First-fit: return a pointer to the node whose ->next is the first
       free block large enough for SIZE.  Returns NULL if none exists.
       First-fit is O(k) — terminates as soon as any adequate block is found.
       Combined with the coalescing sweep this keeps the free list short. */
    FreeBlock* prev = free_list;
    while(prev->next){
        if(prev->next->size >= size)
            return prev;
        prev = prev->next;}
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
    /* Try large_list first (every entry is >= LARGE_THRESHOLD, so any
       allocation request < LARGE_THRESHOLD finds an adequate block
       immediately in O(1)).  Fall back to small_list for small requests. */
    FreeBlock* candidate=before_best_free_block(&(zone->large_list),size);
    if(!candidate && size<LARGE_THRESHOLD)
        candidate=before_best_free_block(&(zone->small_list),size);
    zone->before_best_candidate=candidate;
    return candidate?allocate_in_best_zone(zone,size):NULL;}

static Block* allocate_in_zones(halfword size){
    /* First-fit across zones: allocate from the first zone that has space. */
    {do_zones(zone){
        Block* b=allocate_in_zone(zone,size);
        if(b) return b;}}
    return NULL;}

static Block* allocate_internal(size_t size){
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

/* GC-stress: when BOCL_GCSTRESS=N is set, force a full collection every N
   allocations (N=1 = every allocation).  This makes latent "object not rooted
   before a nested allocate" bugs reproduce deterministically rather than
   depending on heap layout.  A moderate N (e.g. 64) keeps the prelude load
   tractable while still exercising the collector heavily. */
static long gc_stress_n = -1;   /* -1 = unchecked, 0 = off, >0 = period */
static long gc_stress_c = 0;

/* Runtime override (diagnostic): set the GC-stress period after startup so the
   finder can stress a post-prelude workload (e.g. one ANSI test file) without
   paying GCSTRESS over the whole 7000-line prelude load. */
void set_gc_stress(long n){ gc_stress_n = (n<0?0:n); gc_stress_c = 0; }

void* allocate(size_t size){
    if(gc_stress_n < 0){
        const char* s = getenv("BOCL_GCSTRESS");
        gc_stress_n = s ? (atol(s) > 0 ? atol(s) : 1) : 0;}
    if(gc_stress_n > 0 && garbage_collection_is_enabled()){
        if(++gc_stress_c >= gc_stress_n){ gc_stress_c = 0; garbage_collect(); }}
    Block* result=allocate_internal(size);
    /* Zero the object body.  A constructor that sets `objcount` before filling
       its reference slots (with a nested allocation in between that can trigger
       GC) would otherwise expose freed/garbage pointers to the collector, which
       follows them and corrupts the heap — heap-dependent, hard-to-find crashes.
       With the body zeroed, an unfilled slot reads as NULL, and mark() skips
       NULL.  This neutralises that entire class of rooting bug at the source. */
    {   size_t hdr = offsetof(Block, blocks);
        if(size > hdr){ memset((char*)result + hdr, 0, size - hdr); }}
    add_to_live_pool((void*)result);
    return result;}


static bool garbage_collection_enabled=true;

bool garbage_collection_is_enabled(void){
    if(getenv("BOCL_NOGC")) return false;   /* debug override */
    return garbage_collection_enabled;}

bool set_garbage_collection_enabled(bool on){
    bool result=garbage_collection_enabled;
    garbage_collection_enabled=on;
    return result;}

/* Size of the BLOCK_HEADER in bytes (2 × halfword on LP64, 2 × uword otherwise). */
#ifdef _POSIX_V6_LP64_OFF64
#  define BLOCK_HDR_BYTES (2u*(unsigned)sizeof(halfword))
#else
#  define BLOCK_HDR_BYTES (2u*(unsigned)sizeof(uword))
#endif

/* Return true iff PTR is a plausible start-of-block pointer:
   - non-null and pointer-aligned
   - lies within a managed heap zone
   - has a block->size that fits inside that zone
   This guards against corrupted refs that land in the middle of a block. */
static bool valid_block_start_p(Block* ptr){
    if(!ptr) return false;
    if((uword)ptr % sizeof(void*) != 0) return false;   /* alignment */
    for(Zone* z=Zones; z!=NULL; z=z->next_zone){
        char* zstart=(char*)first_block(z);
        char* zend  =(char*)end_block(z);
        if((char*)ptr<zstart || (char*)ptr>=zend) continue;
        /* ptr is in this zone — check size plausibility */
        halfword bsz = ptr->size;   /* safe: ptr is aligned and within zone */
        if(bsz < MinimumBlockSize)  return false;
        if((char*)ptr + bsz > zend) return false;
        return true;}
    return false;}

/* Maximum number of pointer-sized slots a block of the given size can hold.
   Used to cap block_objcount() against corrupted headers. */
static uword block_physical_slot_count(Block* b){
    return (b->size > BLOCK_HDR_BYTES) ? (b->size - BLOCK_HDR_BYTES) / sizeof(Block*) : 0;}

/* -----------------------------------------------------------------------
 * Iterative mark phase — avoids stack overflow on long list structures.
 *
 * We maintain an explicit work-stack (malloc-managed, lives for the
 * lifetime of the process).  For each block popped we mark it, compute
 * how many of its children to inspect (capped at the physical size of
 * the block to guard against corrupted objcount), and push the children.
 * ----------------------------------------------------------------------- */
static Block** mark_wstack     = NULL;
static uword   mark_wstack_cap = 0;
static uword   mark_wstack_top = 0;

static void mark_wstack_push(Block* b){
    if(mark_wstack_top >= mark_wstack_cap){
        uword new_cap = (mark_wstack_cap == 0) ? 4096 : mark_wstack_cap * 2;
        mark_wstack = realloc(mark_wstack, new_cap * sizeof(Block*));
        mark_wstack_cap = new_cap;}
    mark_wstack[mark_wstack_top++] = b;}

/* ---- Use-after-free finder (BOCL_POISON): poison + quarantine -------------
   When BOCL_POISON is set, the sweep does NOT reclaim dead blocks; it POISONs
   them (class slot -> a sentinel) and QUARANTINEs them (never reused), recording
   the C backtrace of the GC that freed them (which contains the function that
   triggered the allocation->GC while still holding an unrooted Ref to the
   block).  If a dangling C-local later USES such a block, class_of sees the
   sentinel and we print that recorded backtrace — pinpointing the missing
   add_to_live_pool with ZERO false positives.  Run under GCSTRESS to trigger
   gaps fast; aborts on the first dangling use.  Leaks (bounded by run length). */
#define POISON_SENTINEL  ((Block*)(uintptr_t)0x015015015015015eULL)
#define POISON_OBJCOUNT  (((uword)FREE_BLOCK)-1u)
#define POISON_RING      262144   /* bounded quarantine: most-recent freed blocks */
#define POISON_BT_DEPTH  24

bool          gc_poison_mode = false;
static bool   gc_poison_bt   = false;   /* BOCL_POISON_BT: capture per-free backtraces (slow) */
/* Lazily malloc'd (only when poison mode is on) so the finder has ZERO static
   footprint otherwise — a ~50 MB .bss would shift the heap layout and could mask
   or move layout-dependent GC behaviour in ordinary runs. */
static Block** poison_ring        = NULL;             /* [POISON_RING]            */
static void*   (*poison_ring_bt)[POISON_BT_DEPTH] = NULL; /* [POISON_RING][DEPTH] */
static int*    poison_ring_btlen  = NULL;             /* [POISON_RING]            */
static int     poison_ring_head   = 0;

static bool poisoned_block_p(Block* b){ return b->objcount==(halfword)POISON_OBJCOUNT; }

/* Poison a dead block + quarantine it in the ring.  When the ring is full, the
   oldest quarantined block is released (objcount=FREE_BLOCK) so the next sweep
   reclaims it — bounding memory to POISON_RING blocks while still catching any
   dangling use that occurs within the last POISON_RING frees (uses almost always
   happen within a few GCs of the free).  We only touch blocks[0] (class) and
   objcount, never the payload slots (raw cons_car/->slots read those). */
static void poison_block(Block* b){
    Block* old = poison_ring[poison_ring_head];
    if(old){ old->objcount = FREE_BLOCK; }          /* release oldest -> reclaimable */
    poison_ring[poison_ring_head]   = b;
    poison_ring_btlen[poison_ring_head] = gc_poison_bt
        ? backtrace(poison_ring_bt[poison_ring_head], POISON_BT_DEPTH)
        : 0;   /* per-free backtrace is the slow part; opt in via BOCL_POISON_BT */
    poison_ring_head = (poison_ring_head + 1) % POISON_RING;
    b->blocks[0] = POISON_SENTINEL;                 /* class slot -> sentinel       */
    b->objcount  = (halfword)POISON_OBJCOUNT;}      /* quarantine marker; payload left
       intact — filling it with a bad pointer was unsound (a constructor reusing an
       evicted block without fully initializing a slot read the stale value). */

_Noreturn void gc_poison_report(Ref object){
    Block* b=(Block*)object;
    fprintf(stderr,"\n*** USE-AFTER-FREE: object %p used after the GC freed it ***\n",(void*)b);
    int slot=-1;
    for(int i=0;i<POISON_RING;i++){ if(poison_ring[i]==b){ slot=i; break; } }
    if(slot>=0){
        fprintf(stderr,"--- backtrace at the GC that FREED it"
                       " (the function missing add_to_live_pool is here) ---\n");
        backtrace_symbols_fd(poison_ring_bt[slot],poison_ring_btlen[slot],fileno(stderr));}
    else{ fprintf(stderr,"(free backtrace evicted from quarantine ring)\n"); }
    fprintf(stderr,"--- backtrace at the USE ---\n");
    void* ub[POISON_BT_DEPTH]; int un=backtrace(ub,POISON_BT_DEPTH);
    backtrace_symbols_fd(ub,un,fileno(stderr));
    fflush(stderr);
    abort();}

static void mark(Block* root){
    /* Seed the worklist — use valid_block_start_p, not just pointer_in_zones,
       to reject refs that land in the middle of another block. */
    if(!root || !valid_block_start_p(root) || (gc_poison_mode&&poisoned_block_p(root)) || marked_block_p(root)) return;
    mark_wstack_top = 0;
    mark_wstack_push(root);
    while(mark_wstack_top > 0){
        Block* b = mark_wstack[--mark_wstack_top];
        if(!b || !valid_block_start_p(b) || (gc_poison_mode&&poisoned_block_p(b)) || marked_block_p(b)) continue;
        mark_block(b);
        uword cnt = block_objcount(b);
        uword max = block_physical_slot_count(b);
        if(cnt > max) cnt = max;   /* guard against corrupted objcount */
        for(uword i = 0; i < cnt; i++){
            Block* child = b->blocks[i];
            /* IMPORTANT: valid_block_start_p MUST come before marked_block_p —
               marked_block_p dereferences child->objcount, which is only safe
               once we know child is a real block start. */
            if(child && valid_block_start_p(child) && !marked_block_p(child))
                mark_wstack_push(child);}}}


static Block* free_block(Zone* zone,Block* block){
    /* if it's contiguous to free blocks, then join them */
    /* Damned, to test if the previous block is free, we'd have to
    walk the whole freelist! for each garbage block!  */
    Block* next=next_block(block);
    if(free_block_p(next)){
        FreeBlock* previous=find_before_free_block(zone,(FreeBlock*)next);
        if(previous==NULL){
            /* The adjacent block looks free (objcount==FREE_BLOCK) but is
               not in the free list.  This can happen when heap corruption
               mangles an objcount field.  Skip coalescing conservatively. */
            FreeBlock* new=(FreeBlock*)block;
            new->objcount=FREE_BLOCK;
            new->next=NULL;
            insert_free_block(zone,new);
            return (Block*)new;}
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

/* Promote the largest block to the front of a free list.  O(N).
   This makes first-fit allocation O(1) for the first (largest) block. */
static void promote_largest(FreeBlock* sentinel){
    FreeBlock* before_max = sentinel;
    halfword   max_size   = 0;
    FreeBlock* prev       = sentinel;
    while(prev->next){
        if(prev->next->size > max_size){
            max_size   = prev->next->size;
            before_max = prev;}
        prev = prev->next;}
    if(before_max != sentinel){
        FreeBlock* largest   = before_max->next;
        before_max->next     = largest->next;
        largest->next        = sentinel->next;
        sentinel->next       = largest;}}

static uword sweep_zone(Zone* zone){
    /* Single O(N) pass: rebuild both free lists from scratch, coalescing
       adjacent dead/free blocks as we scan in address order.
       Each newly freed block is distributed to large_list or small_list
       based on its size.  After the pass, the largest block in each list
       is promoted to the front for O(1) first-fit allocation.             */
    zone->large_list.next     = NULL;
    zone->small_list.next     = NULL;
    zone->before_best_candidate = NULL;

    uword      total   = 0;
    Block*     end     = end_block(zone);
    Block*     current = first_block(zone);
    FreeBlock* tail    = NULL;  /* tail of the current coalescing run */

    while(current < end){
        Block* nxt = next_block(current);   /* pre-compute before possible extension */

        if(gc_poison_mode && poisoned_block_p(current)){
            tail = NULL;   /* quarantined — never reclaimed, never coalesced */
        }else if(marked_block_p(current)){
            clear_block(current);
            tail = NULL;
        }else if(gc_poison_mode && !free_block_p(current)){
            total += current->size;
            poison_block(current);   /* dead block -> poison + quarantine */
            tail = NULL;
        }else{
            /* free block (either mode) or dead block (normal mode) -> reclaim.
               In poison mode this reclaims ring-evicted blocks (now FREE_BLOCK). */
            if(!free_block_p(current))
                total += current->size;
            FreeBlock* fb = (FreeBlock*)current;
            if(tail != NULL && (Block*)((char*)tail + tail->size) == current){
                /* coalesce: extend the tail block in-place */
                tail->size = CHECK_SIZE_TO_HALFWORD(
                    (uword)tail->size + (uword)fb->size);
                /* tail may have crossed the size threshold after extension —
                   we'll re-distribute both lists at the end if needed, but
                   for now leave it in whichever list it was inserted into.
                   allocate_in_zone checks large_list first regardless.     */
            }else{
                /* new free run: put in the appropriate list */
                fb->objcount = FREE_BLOCK;
                if(fb->size >= LARGE_THRESHOLD){
                    fb->next = zone->large_list.next;
                    zone->large_list.next = fb;
                }else{
                    fb->next = zone->small_list.next;
                    zone->small_list.next = fb;}
                tail = fb;}}

        current = nxt;}

    /* Promote the largest block in each list to the front so that
       first-fit on large_list terminates in O(1).                   */
    promote_largest(&zone->large_list);
    promote_largest(&zone->small_list);

    return total;}

static uword sweep(void){
    uword total=0;
    {do_zones(zone){
            total+=sweep_zone(zone);}}
    return total;}



#define LivePool_MaxSize 1020

typedef struct LivePool {
    BLOCK_HEADER;
    struct LivePool* next;
    Block*           blocks[LivePool_MaxSize];
    uword            offset;
} LivePool;

typedef struct RootSet {
    BLOCK_HEADER;
    LivePool* live_pool;
    Block*    kernel_rootset;
} RootSet;

static RootSet* rootset;

void set_rootset(Block* block){
    rootset->kernel_rootset=block;}

uword open_live_pool(void){
    return rootset->live_pool->offset+rootset->live_pool->objcount;}

void add_to_live_pool(Ref object){
    LivePool* pool=rootset->live_pool;
    /* 1- save the object the the pool */
    pool->blocks[pool->objcount-1]=(Block*)object;
    pool->objcount++;
    /* 2- if the pool is full, add some space with gc disabled: */
    if(LivePool_MaxSize+1<=pool->objcount){
        bool saved=garbage_collection_enabled;
        garbage_collection_enabled=false;
        LivePool* new=(LivePool*)allocate_internal(sizeof(LivePool));
        garbage_collection_enabled=saved;
        new->objcount=1;
        new->next=pool;
        new->offset=pool->offset+pool->objcount;
        rootset->live_pool=new;}}

void close_live_pool(uword objcount){
    while(objcount < rootset->live_pool->offset){
        rootset->live_pool=rootset->live_pool->next;}
    if(objcount < rootset->live_pool->offset+LivePool_MaxSize+1){
        rootset->live_pool->objcount = (halfword)(objcount - rootset->live_pool->offset);}}

/* Debug aid (BOCL_GCCHECK): after marking, every SURVIVING object's reference
   slots must point to objects that also survive.  A survivor that references a
   doomed (unmarked) child means that child is about to be wrongly freed while
   still reachable — i.e. a real rooting/objcount bug.  Report the offending
   parent so the bug can be traced to a specific constructor. */
static void verify_no_dangling(void){
    do_zones(zone){
        Block* end = end_block(zone);
        Block* b   = first_block(zone);
        while(b < end){
            Block* nxt = next_block(b);
            if(!free_block_p(b) && marked_block_p(b)){
                uword cnt = block_objcount(b);
                uword max = block_physical_slot_count(b);
                if(cnt > max){ cnt = max; }
                for(uword i = 0; i < cnt; i++){
                    Block* child = b->blocks[i];
                    if(child && valid_block_start_p(child)
                       && !free_block_p(child) && !marked_block_p(child)){
                        fprintf(stderr,
                            "GCCHECK: survivor %p (objcount=%lu size=%lu) slot %lu -> DOOMED %p (objcount=%lu size=%lu)\n",
                            (void*)b, (unsigned long)block_objcount(b), (unsigned long)b->size,
                            (unsigned long)i, (void*)child,
                            (unsigned long)block_objcount(child), (unsigned long)child->size);
                        fflush(stderr);}}}
            b = nxt;}}}

uword garbage_collect(void){
    if(!garbage_collection_enabled){
        return 0;}
    mark((Block*)rootset);
    if(getenv("BOCL_GCCHECK")){ verify_no_dangling(); }
    uword freed = sweep();
    return freed;}



/* Allocate the quarantine ring on demand (env BOCL_POISON or the (%poison t)
   runtime toggle).  Returns false if it cannot be allocated. */
static bool poison_ring_ensure(void){
    if(poison_ring){ return true; }
    poison_ring       = calloc(POISON_RING, sizeof(*poison_ring));
    poison_ring_bt    = calloc(POISON_RING, sizeof(*poison_ring_bt));
    poison_ring_btlen = calloc(POISON_RING, sizeof(*poison_ring_btlen));
    if(!poison_ring || !poison_ring_bt || !poison_ring_btlen){
        fprintf(stderr,"BOCL_POISON: out of memory for quarantine ring\n");
        return false;}
    return true;}

void set_gc_poison(bool on){
    /* Must allocate the ring before enabling: poison_block() writes into it.  The
       runtime toggle bypassed memory_initialize's allocation otherwise (a NULL
       ring -> crash). */
    if(on && !poison_ring_ensure()){ gc_poison_mode = false; return; }
    gc_poison_mode = on;}

void memory_initialize(void){
    gc_poison_bt   = getenv("BOCL_POISON_BT")!=NULL; /* per-free backtraces (slow) */
    if(getenv("BOCL_POISON")){ set_gc_poison(true); }
    zone_allocate(DefaultZoneSize);
    rootset=(RootSet*)allocate_internal(sizeof(*rootset));
    rootset->objcount=2;
    rootset->kernel_rootset=NULL;
    rootset->live_pool=(LivePool*)allocate_internal(sizeof(LivePool));
    rootset->live_pool->objcount=1;
    rootset->live_pool->next=NULL;
    rootset->live_pool->offset=0;}



/* ======================================================================== */

/* For memory dump: */
static bool in_root_set_p(Block* block){
    /* The root set is a lisp list of (compact) conses: each cons is a single
       block {class, car, cdr} with objcount==3 — car is blocks[1], cdr blocks[2]. */
    Block* current=rootset->kernel_rootset;
    if(current==NULL){
        return false;}
    while(current && block_objcount(current)==3){
        if(current->blocks[1]==block){   /* car == the searched object */
            return true;}
        current=current->blocks[2];}     /* cdr == next cons */
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
            fprintf(out,"      large_list             %16p\n",(void*)zone->large_list.next);
            fprintf(out,"      small_list             %16p\n",(void*)zone->small_list.next);
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
                 if(in_free_list(&(zone->large_list),block)||in_free_list(&(zone->small_list),block)){
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
