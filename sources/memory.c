#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>
#include <sysexits.h>
#include "backtrace_compat.h"   /* execinfo backtrace() or no-op stubs (musl) */
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

/* ---- objcount bit layout (generational GC) -------------------------------
   objcount is 32 bits in every config (halfword on LP64, uword=uint32 on 32-bit).
   We carve two GC flag bits off the top, leaving 30 bits for the actual count:

       bit 31  GC_MARK_BIT   set during the mark phase (cleared by clear_block)
       bit 30  GC_OLD_BIT    generation: 1 = old (gen-1), 0 = young (gen-0)
       bit 0..29 GC_COUNT_MASK  number of leading Ref slots in the block

   FREE_BLOCK is all-ones, so it has BOTH flag bits set; every predicate that
   could see a free block (marked_block_p, old_block_p) must exclude FREE_BLOCK
   first.  The mark bit is sticky-clearable: clear_block clears ONLY bit 31, so
   the old bit survives a mark/sweep — which is exactly what a generational
   collector needs (clearing a survivor's mark must not demote its generation). */
#define GC_MARK_BIT   (~(FREE_BLOCK>>1))   /* high bit              (0x80000000) */
#define GC_OLD_BIT    (GC_MARK_BIT>>1)     /* next bit down         (0x40000000) */
#define GC_COUNT_MASK (FREE_BLOCK>>2)      /* low 30 bits           (0x3FFFFFFF) */

static bool free_block_p(Block* block){
    return block->objcount==FREE_BLOCK;}

static bool marked_block_p(Block* block){
    /* The mark bit is the high bit of objcount.  FREE_BLOCK (all-ones) also has
       that bit set, so a free block must NOT be reported as marked — otherwise
       sweep_zone (which tests marked_block_p before reclaiming) would clear_block
       a free block to FREE_BLOCK>>1 and drop it from the free list, leaking and
       corrupting the heap. */
    return block->objcount!=FREE_BLOCK && 0!=(block->objcount&GC_MARK_BIT);}

static void mark_block(Block* block){
    block->objcount|=GC_MARK_BIT;}

static void clear_block(Block* block){
    block->objcount&=~GC_MARK_BIT;}   /* clears ONLY the mark bit; old bit + count survive */

static halfword block_objcount(Block* block){
    return block->objcount&GC_COUNT_MASK;}

static bool old_block_p(Block* block){
    /* As with marked_block_p, FREE_BLOCK (all-ones) has the old bit set too, so
       a free block must never be reported as old. */
    return block->objcount!=FREE_BLOCK && 0!=(block->objcount&GC_OLD_BIT);}

static void set_old_block(Block* block){
    block->objcount|=GC_OLD_BIT;}      /* promote: sticky, survives clear_block */

static void clear_old_block(Block* block){
    block->objcount&=~GC_OLD_BIT;}     /* demote back to young (un-pin, see gc_demote_pinned) */

/* Public, GC-safe replacement for raw `obj->...objcount` reads that mean
   "how many Ref slots does this block have": masks off the mark/old flag bits.
   Used by hash_table.c / structure.c, which read a VectorOfObject's element
   count straight from objcount — those reads would include GC_OLD_BIT once a
   block is promoted, yielding a wildly wrong count. */
uword object_slot_count(void* object){
    return block_objcount((Block*)object);}

/* ---- Remembered set / write barrier (generational GC) --------------------
   A minor collection traces only gen-0 (young) objects; it does NOT trace
   gen-1 (old) objects.  So a young object reachable ONLY through an old
   object's slot would be missed and wrongly swept.  The write barrier records
   every old block that has a Ref stored into it since the last minor GC; the
   minor collector then scans those remembered blocks' direct slots as extra
   roots.  We over-approximate (record on ANY store while the block is old,
   regardless of whether the stored value is actually young) — the minor mark
   filters to young children, and duplicates are harmless (marking is
   idempotent).  A cheap consecutive-duplicate check absorbs tight mutation
   loops on one object.  The set is malloc-managed (like mark_wstack) and lives
   for the process; the minor collector clears it (remset_top=0) after scanning.

   IMPORTANT: the barrier target is the block that PHYSICALLY holds the mutated
   slot — for `obj->slots->X.field` that is obj->slots (the Slots block), and
   for svref it is vector->slots->array.elements (the inner VectorOfObject) —
   NOT the outer Object, because the minor mark only follows a remembered
   block's DIRECT children. */
static Block** remset     = NULL;
static uword   remset_cap = 0;
static uword   remset_top = 0;

void gc_write_barrier(void* container){
    Block* b = (Block*)container;
    if(!b || !old_block_p(b)) return;
    if(remset_top && remset[remset_top-1]==b) return;   /* consecutive-dup filter */
    if(remset_top >= remset_cap){
        uword nc = remset_cap ? remset_cap*2 : 4096;
        remset = realloc(remset, nc*sizeof(Block*));
        remset_cap = nc;}
    remset[remset_top++] = b;}

/* ---- Pinned set: live-pool objects kept young across a collection ---------
   An object under construction is rooted in the live pool across the nested
   allocations its constructor performs (the existing rooting discipline).  If a
   collection promoted such a half-built object, its still-pending initializing
   stores would become un-barriered old->young edges (construction stores carry
   no write barrier).  So a collection PINS every young object directly held by
   the live pool — promotes nothing it cannot also keep young — by demoting them
   back to gen-0 after the sweep.  They are promoted normally once they leave the
   live pool (their constructor returned, so they are fully built).  Old objects
   in the live pool (merely rooted, not under construction) are left alone. */
static Block** pinned     = NULL;
static uword   pinned_cap = 0;
static uword   pinned_top = 0;


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

/* Allocate SIZE bytes out of the first adequate block on LIST (first-fit).
   IS_LARGE says LIST is the zone's large_list, which must keep its invariant
   "every entry is >= LARGE_THRESHOLD" so that any request < LARGE_THRESHOLD is
   satisfied by the head block in O(1).  Splitting a block can shrink the
   remainder below the threshold; when that happens on the large_list we MOVE
   the remainder to the small_list, otherwise the sub-threshold remainders pile
   up at the head and every later allocation first-fit-walks past them — an
   O(n) drift that made ANSI-suite loading quadratic in the live heap size. */
static Block* allocate_from_list(Zone* zone,FreeBlock* list,halfword size,bool is_large){
    FreeBlock* prev=before_best_free_block(list,size);
    if(!prev){ return NULL; }
    FreeBlock* fb=prev->next;
    halfword remaining=CHECK_SIZE_TO_HALFWORD(fb->size-size);
    if(MinimumBlockSize<=remaining){
        /* split: carve the allocated block off the tail, remainder stays at fb */
        Block* block=(void*)((char*)fb+remaining);
        fb->size=remaining;
        block->size=size;
        block->objcount=0;
        if(is_large && remaining<LARGE_THRESHOLD){
            prev->next=fb->next;                 /* unlink from large_list      */
            fb->next=zone->small_list.next;      /* re-file under small_list     */
            zone->small_list.next=fb;}
        return block;
    }else{
        Block* block=(Block*)fb;
        prev->next=fb->next;
        block->objcount=0;
        return block;}}

static Block* allocate_in_zone(Zone* zone,halfword size){
    /* Try large_list first (every entry is >= LARGE_THRESHOLD, so any
       allocation request < LARGE_THRESHOLD finds an adequate block
       immediately in O(1)).  Fall back to small_list for small requests. */
    Block* block=allocate_from_list(zone,&(zone->large_list),size,true);
    if(!block && size<LARGE_THRESHOLD){
        block=allocate_from_list(zone,&(zone->small_list),size,false);}
    return block;}

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
    /* Every block must be at least MinimumBlockSize so that, once freed, it can
       hold a FreeBlock's `next` pointer.  Sub-minimum blocks (e.g. zero-length
       VectorOfObject storage for #()) would otherwise overwrite the FOLLOWING
       block's header when sweep writes fb->next at offset 8 — clobbering its size
       field with an in-zone pointer (heap-walk skips the rest of the zone). */
    if(hsize < MinimumBlockSize){ hsize = MinimumBlockSize; }
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

/* Heap-integrity walk (BOCL_HEAPCHECK=N): every N allocations, walk every zone
   block-by-block and verify the blocks tile the zone exactly with plausible
   sizes/objcounts.  A stray out-of-bounds write that clobbers a block header is
   caught here (the next walk lands mid-block or overruns the zone), pinpointing
   the form whose evaluation corrupted the heap.  Aborts with a backtrace on the
   first inconsistency. */
static long  heap_check_n = -1;
static long  heap_check_c = 0;
static uword heap_check_after = 0;     /* BOCL_HEAPCHECK_AFTER: skip the first N allocs (the prelude) */
static uword heap_total_allocs = 0;
static uword block_physical_slot_count(Block* b);   /* defined below (mark phase) */

static void heap_dump_and_abort(const char* msg, Zone* z, Block* b){
    fprintf(stderr,"\nHEAPCHECK: %s  (zone %p [%p..%p], block %p size=%lu objcount=%ld)\n",
            msg,(void*)z,(void*)first_block(z),(void*)end_block(z),
            (void*)b,(unsigned long)(b?b->size:0),(long)(b?b->objcount:0));
    fflush(stderr);
    void* bt[40]; int n=backtrace(bt,40); backtrace_symbols_fd(bt,n,fileno(stderr));
    abort();
}

static void heap_verify_free_list(Zone* z, FreeBlock* sentinel, const char* which){
    char* zstart = (char*)first_block(z);
    char* zend   = (char*)end_block(z);
    uword guard  = 0;
    for(FreeBlock* f = sentinel->next; f != NULL; f = f->next){
        if((char*)f < zstart || (char*)f >= zend) heap_dump_and_abort(which, z, (Block*)f);
        if((uword)f % sizeof(void*) != 0)         heap_dump_and_abort(which, z, (Block*)f);
        if(f->size == 0 || f->size % sizeof(word) != 0 || (char*)f + f->size > zend)
                                                  heap_dump_and_abort(which, z, (Block*)f);
        if(!free_block_p((Block*)f))              heap_dump_and_abort(which, z, (Block*)f);
        if(++guard > (zend - zstart) / (uword)MinimumBlockSize + 16)
                                                  heap_dump_and_abort("free-list cycle", z, (Block*)f);}
}

static void heap_verify(void){
    for(Zone* z=Zones; z!=NULL; z=z->next_zone){
        Block* end = end_block(z);
        Block* b   = first_block(z);
        while(b < end){
            uword bsz = b->size;
            if(bsz == 0)                          heap_dump_and_abort("block size is zero", z, b);
            if(bsz % sizeof(word) != 0)           heap_dump_and_abort("block size not word-aligned", z, b);
            if(((char*)b + bsz) > (char*)end)     heap_dump_and_abort("block overruns zone end", z, b);
            if(!free_block_p(b) && marked_block_p(b))
                                                  heap_dump_and_abort("non-free block has GC mark bit set outside GC", z, b);
            if(!free_block_p(b) && block_objcount(b) > block_physical_slot_count(b))
                                                  heap_dump_and_abort("objcount > physical slots", z, b);
            b = next_block(b);}
        if(b != end)                              heap_dump_and_abort("blocks did not tile zone exactly", z, b);
        heap_verify_free_list(z, &z->large_list, "large free-list node corrupt");
        heap_verify_free_list(z, &z->small_list, "small free-list node corrupt");}
}

/* minor_collect (memory.h) is defined below; used here by the stress hook. */
static long gc_minor_stress_n = -1;   /* BOCL_GCMINORSTRESS: force a MINOR GC every N allocs */
static long gc_minor_stress_c = 0;

/* ---- Proactive generational collection policy (opt-in) -------------------
   The legacy collector fires only when every zone is full, so a garbage-heavy
   load (the ANSI-test suite: ~99% garbage) balloons the heap to gigabytes
   before anything is reclaimed, and the per-allocation zone walk then goes
   super-linear.  This policy fires a MINOR GC every BOCL_GC_YOUNG_KB kilobytes
   of allocation (keeping the heap small without re-marking the old generation)
   and promotes the survivors with a MAJOR GC every BOCL_GC_MAJOR_EVERY minors.

   ON BY DEFAULT (v1.0.10): the rooting long-tail that previously made frequent
   collection unsafe is fixed (Slice 173 — see issues/open/generational-gc.issue),
   so the proactive policy now defaults on with a BOCL_GC_YOUNG_KB_DEFAULT-KB young
   threshold.  Set BOCL_GC_YOUNG_KB=0 to force the legacy "collect only when full"
   behaviour; any positive BOCL_GC_YOUNG_KB overrides the default threshold. */
#define BOCL_GC_YOUNG_KB_DEFAULT 8192   /* minor GC every 8 MB of allocation */
static long  gc_policy_young_kb     = -1;   /* -1 unchecked, 0 off, >0 = KB threshold */
static long  gc_policy_major_every  = 16;
static uword gc_policy_young_bytes  = 0;
static long  gc_policy_minors_done  = 0;

void* allocate(size_t size){
    if(gc_stress_n < 0){
        const char* s = getenv("BOCL_GCSTRESS");
        gc_stress_n = s ? (atol(s) > 0 ? atol(s) : 1) : 0;}
    if(gc_stress_n > 0 && garbage_collection_is_enabled()){
        if(++gc_stress_c >= gc_stress_n){ gc_stress_c = 0; garbage_collect(); }}
    if(gc_minor_stress_n < 0){
        const char* s = getenv("BOCL_GCMINORSTRESS");
        gc_minor_stress_n = s ? (atol(s) > 0 ? atol(s) : 1) : 0;}
    if(gc_minor_stress_n > 0 && garbage_collection_is_enabled()){
        if(++gc_minor_stress_c >= gc_minor_stress_n){ gc_minor_stress_c = 0; minor_collect(); }}
    if(gc_policy_young_kb < 0){
        const char* s = getenv("BOCL_GC_YOUNG_KB");
        gc_policy_young_kb = s ? (atol(s) < 0 ? 0 : atol(s)) : BOCL_GC_YOUNG_KB_DEFAULT;
        const char* m = getenv("BOCL_GC_MAJOR_EVERY");
        if(m){ gc_policy_major_every = atol(m); }}
    if(gc_policy_young_kb > 0 && garbage_collection_is_enabled()){
        gc_policy_young_bytes += size;
        if(gc_policy_young_bytes >= (uword)gc_policy_young_kb * 1024u){
            gc_policy_young_bytes = 0;
            if(gc_policy_major_every > 0 && ++gc_policy_minors_done >= gc_policy_major_every){
                gc_policy_minors_done = 0; garbage_collect();
            }else{
                minor_collect();}}}
    if(heap_check_n < 0){
        const char* s = getenv("BOCL_HEAPCHECK");
        heap_check_n = s ? (atol(s) > 0 ? atol(s) : 1) : 0;
        const char* a = getenv("BOCL_HEAPCHECK_AFTER");
        heap_check_after = a ? (uword)atol(a) : 0;}
    heap_total_allocs++;
    if(heap_check_n > 0 && heap_total_allocs >= heap_check_after){
        if(++heap_check_c >= heap_check_n){ heap_check_c = 0; heap_verify(); }}
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

static uword sweep_zone(Zone* zone, bool minor){
    /* Single O(N) pass: rebuild both free lists from scratch, coalescing
       adjacent dead/free blocks as we scan in address order.
       Each newly freed block is distributed to large_list or small_list
       based on its size.  After the pass, the largest block in each list
       is promoted to the front for O(1) first-fit allocation.

       Generations: a survivor (marked block, or — in a MINOR sweep — any old
       block) is kept.  A MINOR sweep leaves old blocks untouched (it neither
       traces nor reclaims the old generation) and does NOT promote: its young
       survivors stay young (re-marked each minor until a major tenures them).
       A MAJOR sweep traces+reclaims both generations and PROMOTES every
       survivor to old (set_old_block) — promotion happens only at major GC, so
       the scaffold/young working set never gets promoted out from under a
       constructor's still-pending initializing stores (see gc_collect_pinned).*/
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
        }else if(minor && old_block_p(current)){
            tail = NULL;   /* old generation survives a minor GC untouched */
        }else if(marked_block_p(current)){
            clear_block(current);
            if(!minor){ set_old_block(current); }  /* promotion happens at MAJOR GC only */
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

static uword sweep(bool minor){
    uword total=0;
    {do_zones(zone){
            total+=sweep_zone(zone,minor);}}
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

/* The RootSet and every LivePool are GC scaffold, kept in the YOUNG generation:
   a minor GC never promotes, and a major GC pins them young (gc_collect_pinned),
   so they are always traced afresh from the rootset and an orphaned (popped)
   pool becomes unreachable young and is reclaimed normally.  A LivePool reuses
   its objcount header field as its live-block fill counter (objcount-1 live
   blocks at blocks[0..objcount-2]); these helpers read it masked and write it
   back preserving the mark/old flag bits, so the dual use is robust even if a
   pool ever carried a flag bit. */
static uword live_pool_count(LivePool* p){
    return block_objcount((Block*)p);}
static void live_pool_set_count(LivePool* p, uword n){
    p->objcount = (p->objcount & ~GC_COUNT_MASK) | ((halfword)n & GC_COUNT_MASK);}

void set_rootset(Block* block){
    rootset->kernel_rootset=block;}

uword open_live_pool(void){
    return rootset->live_pool->offset+live_pool_count(rootset->live_pool);}

void add_to_live_pool(Ref object){
    LivePool* pool=rootset->live_pool;
    uword count=live_pool_count(pool);
    /* 1- save the object in the pool */
    pool->blocks[count-1]=(Block*)object;
    live_pool_set_count(pool,count+1);
    count++;
    /* 2- if the pool is full, add some space with gc disabled: */
    if(LivePool_MaxSize+1<=count){
        bool saved=garbage_collection_enabled;
        garbage_collection_enabled=false;
        LivePool* new=(LivePool*)allocate_internal(sizeof(LivePool));
        garbage_collection_enabled=saved;
        new->objcount=1;
        new->next=pool;
        new->offset=pool->offset+count;
        rootset->live_pool=new;}}

void close_live_pool(uword objcount){
    while(objcount < rootset->live_pool->offset){
        rootset->live_pool=rootset->live_pool->next;}
    if(objcount < rootset->live_pool->offset+LivePool_MaxSize+1){
        live_pool_set_count(rootset->live_pool, objcount - rootset->live_pool->offset);}}

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

/* ---- Minor (generational) mark ------------------------------------------
   A minor mark traces ONLY the young (gen-0) generation: it stops at every old
   block (does not trace it) and marks every reachable young block.  This is
   sound only because the write barrier guarantees that every old->young edge
   is recorded in the remembered set — so a young object reachable solely
   through an old object is reached via that old block's slots (scanned as a
   root in minor_collect), not by tracing the old block here.  Reuses the major
   collector's mark_wstack. */
static void minor_push_if_young(Block* b){
    if(b && valid_block_start_p(b) && !(gc_poison_mode && poisoned_block_p(b))
       && !old_block_p(b) && !marked_block_p(b)){
        mark_wstack_push(b);}}

static void minor_drain(void){
    while(mark_wstack_top > 0){
        Block* b = mark_wstack[--mark_wstack_top];
        if(!b || !valid_block_start_p(b) || (gc_poison_mode && poisoned_block_p(b))
           || old_block_p(b) || marked_block_p(b)) continue;
        mark_block(b);
        uword cnt = block_objcount(b);
        uword max = block_physical_slot_count(b);
        if(cnt > max) cnt = max;
        for(uword i = 0; i < cnt; i++){ minor_push_if_young(b->blocks[i]); }}}

/* BOCL_GCCHECK for minor GC: after the minor mark, every SURVIVOR (an old block
   or a marked young block) that references a young block must reference a MARKED
   young block — an unmarked young child of a survivor is about to be swept while
   still reachable, i.e. a missing write barrier (an old->young edge that never
   made it into the remembered set).  This is the primary correctness probe for
   step 2/3; run under BOCL_GCMINORSTRESS to exercise it hard. */
static void verify_no_dangling_minor(void){
    do_zones(zone){
        Block* end = end_block(zone);
        Block* b   = first_block(zone);
        while(b < end){
            Block* nxt = next_block(b);
            bool survivor = !free_block_p(b) && (old_block_p(b) || marked_block_p(b));
            if(survivor){
                uword cnt = block_objcount(b);
                uword max = block_physical_slot_count(b);
                if(cnt > max){ cnt = max; }
                for(uword i = 0; i < cnt; i++){
                    Block* child = b->blocks[i];
                    if(child && valid_block_start_p(child) && !free_block_p(child)
                       && !old_block_p(child) && !marked_block_p(child)){
                        fprintf(stderr,
                            "GCCHECK(minor): survivor %p (%s objcount=%lu size=%lu) slot %lu"
                            " -> DOOMED YOUNG %p (objcount=%lu size=%lu)\n",
                            (void*)b, old_block_p(b)?"old":"marked",
                            (unsigned long)block_objcount(b), (unsigned long)b->size,
                            (unsigned long)i, (void*)child,
                            (unsigned long)block_objcount(child), (unsigned long)child->size);
                        fflush(stderr);}}}
            b = nxt;}}}

/* ---- BOCL_GCLOG: per-collection diagnostics -----------------------------
   When BOCL_GCLOG is set, print one line per collection: kind (minor/major),
   bytes freed this collection, and a post-sweep snapshot of the heap (live
   bytes, free bytes, total heap bytes, zone count) plus the running
   minor/major collection counts.  This is the instrument the diagnosis used
   to show the load is ~99% garbage and the heap balloons before any GC fires.
   Computing live/free walks the whole heap, so it is gated on the env var. */
static int   gc_log_enabled = -1;       /* -1 = unchecked */
uword        gc_minor_count = 0;
uword        gc_major_count = 0;

static void gc_log(const char* kind, uword freed){
    uword zones=0, heap=0, live=0, freebytes=0;
    for(Zone* z=Zones; z; z=z->next_zone){
        zones++; heap += z->size;
        Block* end=end_block(z);
        for(Block* b=first_block(z); b<end; b=next_block(b)){
            if(free_block_p(b)) freebytes += b->size;
            else                live      += b->size;}}
    fprintf(stderr,
        "[GC %-5s] #%lu freed=%lu live=%lu free=%lu heap=%lu zones=%lu"
        " (minor=%lu major=%lu)\n",
        kind,(unsigned long)(gc_minor_count+gc_major_count),
        (unsigned long)freed,(unsigned long)live,(unsigned long)freebytes,
        (unsigned long)heap,(unsigned long)zones,
        (unsigned long)gc_minor_count,(unsigned long)gc_major_count);
    fflush(stderr);}

static void pinned_push(Block* b){
    if(pinned_top >= pinned_cap){
        uword nc = pinned_cap ? pinned_cap*2 : 4096;
        pinned = realloc(pinned, nc*sizeof(Block*));
        pinned_cap = nc;}
    pinned[pinned_top++] = b;}

/* Snapshot the young objects a major collection must keep young: the GC
   scaffold (the RootSet and every active LivePool) plus every object directly
   held by the live pool (the current working set, which includes any object a
   constructor is still initializing).  Run after the mark, before the sweep —
   no allocation happens in between, so the snapshot stays valid. */
static void gc_collect_pinned(void){
    pinned_top = 0;
    if(rootset && !old_block_p((Block*)rootset)){ pinned_push((Block*)rootset); }
    for(LivePool* p = rootset->live_pool; p != NULL; p = p->next){
        if(!old_block_p((Block*)p)){ pinned_push((Block*)p); }
        uword count = live_pool_count(p);            /* count-1 live blocks */
        for(uword i = 0; i + 1 < count; i++){
            Block* b = p->blocks[i];
            if(b && valid_block_start_p(b) && !old_block_p(b)){ pinned_push(b); }}}}

/* After the sweep promoted every survivor to old, demote the pinned (live-pool)
   objects back to young so a constructor still mutating them does so with no
   barrier.  They survived the sweep (they are live-pool roots), so each is a
   valid block. */
static void gc_demote_pinned(void){
    for(uword i = 0; i < pinned_top; i++){
        Block* b = pinned[i];
        if(b && valid_block_start_p(b)){ clear_old_block(b); }}}

/* Rebuild the remembered set to exactly the old blocks that STILL point to a
   young object after this collection (the remaining old->young edges — those
   whose young target was pinned, not promoted).  Bounds the set and drops stale
   entries (blocks freed/reused/demoted).  Replaces a blanket clear: a young
   target that was pinned rather than promoted must stay reachable via its old
   parent at the next collection. */
static bool block_has_young_child(Block* b){
    uword cnt = block_objcount(b);
    uword max = block_physical_slot_count(b);
    if(cnt > max) cnt = max;
    for(uword i = 0; i < cnt; i++){
        Block* c = b->blocks[i];
        if(c && valid_block_start_p(c) && !free_block_p(c) && !old_block_p(c)){
            return true; }}
    return false;}

static void remset_add_raw(Block* b){
    if(remset_top >= remset_cap){
        uword nc = remset_cap ? remset_cap*2 : 4096;
        remset = realloc(remset, nc*sizeof(Block*));
        remset_cap = nc;}
    remset[remset_top++] = b;}

/* Incremental rebuild (after a MINOR GC): keep the remembered entries that
   still point at a young object, drop the rest.  A minor GC promotes nothing,
   so it creates no new old->young edges — filtering the existing set suffices. */
static void gc_rebuild_remset(void){
    uword w = 0;
    for(uword r = 0; r < remset_top; r++){
        Block* R = remset[r];
        if(!R || !valid_block_start_p(R) || !old_block_p(R)) continue;
        if(w && remset[w-1]==R) continue;        /* cheap dedup */
        if(block_has_young_child(R)){ remset[w++] = R; }}
    remset_top = w;}

/* Full rebuild (after a MAJOR GC): a major promotes survivors, which can leave
   an old object pointing at a still-young (pinned) object — most importantly an
   object's promoted wrapper pointing at its pinned Slots block (the wrapper is
   trimmed from the live pool by Object_new's close_live_pool, so it promotes,
   while the slots block lingers pinned).  That construction edge carries no
   write barrier, so we recover the complete remembered set by scanning every
   old block for a young child.  O(heap), but only at the infrequent major. */
static void gc_rebuild_remset_full(void){
    remset_top = 0;
    do_zones(zone){
        Block* end = end_block(zone);
        Block* b   = first_block(zone);
        while(b < end){
            Block* nxt = next_block(b);
            if(!free_block_p(b) && old_block_p(b) && block_has_young_child(b)){
                remset_add_raw(b); }
            b = nxt;}}}

uword garbage_collect(void){
    if(!garbage_collection_enabled){
        return 0;}
    if(gc_log_enabled < 0){ gc_log_enabled = getenv("BOCL_GCLOG")?1:0; }
    gc_major_count++;
    mark((Block*)rootset);
    if(getenv("BOCL_GCCHECK")){ verify_no_dangling(); }
    /* Major sweep traces and reclaims BOTH generations and promotes every
       survivor to old, then we re-pin the live-pool (under-construction)
       objects young and rebuild the remembered set from what actually remains
       old->young.  This keeps the major collection sound after arbitrary
       mutation while re-aging the persistent set to the old generation. */
    gc_collect_pinned();
    uword freed = sweep(false);
    gc_demote_pinned();
    gc_rebuild_remset_full();   /* full scan: recover old->young edges created by promotion */
    if(gc_log_enabled){ gc_log("major", freed); }
    return freed;}

/* Minor (generational) collection: reclaim only young garbage without
   re-marking the old generation, and WITHOUT promoting (promotion is a
   major-GC job).  Because nothing is promoted, a minor GC never creates an
   old->young edge and needs no pinning: the young scaffold/working set simply
   stays young.  Roots are (a) the rootset, traced normally — it and the active
   LivePool chain are young, so the young-only mark walks them and every live
   object, stopping at the old persistent set (kernel_rootset etc.); and (b) the
   remembered set: the direct slots of every old block holding an old->young
   edge.  An orphaned (popped) LivePool is unreachable from the rootset, so it
   is left young-and-unmarked and reclaimed like any other dead young block. */
uword minor_collect(void){
    if(!garbage_collection_enabled){
        return 0;}
    if(gc_log_enabled < 0){ gc_log_enabled = getenv("BOCL_GCLOG")?1:0; }
    gc_minor_count++;
    mark_wstack_top = 0;
    /* (a) rootset (young scaffold) — traces the active pool chain + live objects */
    minor_push_if_young((Block*)rootset);
    /* (b) remembered-set roots: scan the direct slots of each remembered old
       block (the recorded old->young edges). */
    for(uword r = 0; r < remset_top; r++){
        Block* R = remset[r];
        if(!R || !valid_block_start_p(R) || !old_block_p(R)) continue;
        uword cnt = block_objcount(R);
        uword max = block_physical_slot_count(R);
        if(cnt > max) cnt = max;
        for(uword i = 0; i < cnt; i++){ minor_push_if_young(R->blocks[i]); }}
    minor_drain();
    if(getenv("BOCL_GCCHECK")){ verify_no_dangling_minor(); }
    uword freed = sweep(true);     /* young-only: skips old, reclaims young dead, no promote */
    gc_rebuild_remset();
    if(gc_log_enabled){ gc_log("minor", freed); }
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
