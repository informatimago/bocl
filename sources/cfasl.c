/* cfasl.c — compact binary fast-load file (issue plan-1-compact-fasl).
   cfasl-1: the low-level byte / varint / float I/O primitives (see cfasl.h for the
   format spec).  The object-graph writer (cfasl-2) and loader (cfasl-3) build on
   these.  All multibyte integers are LEB128 varints and floats are little-endian,
   so the file is endianness-independent. */

#include "cfasl.h"
#include "kernel.h"
#include "kernel_functions.h"
#include "kernel_types.h"
#include "kernel_objects.h"
#include "packages.h"
#include "memory.h"
#include "bytecode.h"    /* vm-8: serialize CompiledFunction templates */
#include <stdlib.h>
#include <string.h>

/* ---- Output buffer ------------------------------------------------------ */

void cfasl_out_init(cfasl_out* b){ b->data=NULL; b->len=0; b->cap=0; b->error=0; }
void cfasl_out_free(cfasl_out* b){ free(b->data); b->data=NULL; b->len=b->cap=0; }

static void cfasl_out_reserve(cfasl_out* b, size_t extra){
    if(b->error){ return; }
    if(b->len + extra <= b->cap){ return; }
    size_t ncap = b->cap ? b->cap*2 : 1024;
    while(ncap < b->len + extra){ ncap *= 2; }
    uint8_t* nd = (uint8_t*)realloc(b->data, ncap);
    if(!nd){ b->error = 1; return; }
    b->data = nd; b->cap = ncap;
}

void cfasl_put_u8(cfasl_out* b, uint8_t v){
    cfasl_out_reserve(b, 1);
    if(b->error){ return; }
    b->data[b->len++] = v;
}

void cfasl_put_bytes(cfasl_out* b, const void* p, size_t n){
    cfasl_out_reserve(b, n);
    if(b->error){ return; }
    memcpy(b->data + b->len, p, n);
    b->len += n;
}

void cfasl_put_uvarint(cfasl_out* b, uint64_t v){
    uint8_t tmp[10]; int n = 0;
    do { uint8_t byte = (uint8_t)(v & 0x7fu); v >>= 7;
         if(v){ byte |= 0x80u; } tmp[n++] = byte; } while(v);
    cfasl_put_bytes(b, tmp, (size_t)n);
}

void cfasl_put_svarint(cfasl_out* b, int64_t v){
    /* zig-zag */
    uint64_t zz = ((uint64_t)v << 1) ^ (uint64_t)(v >> 63);
    cfasl_put_uvarint(b, zz);
}

void cfasl_put_f32(cfasl_out* b, float v){
    uint32_t bits; memcpy(&bits, &v, 4);
    uint8_t le[4];
    for(int i=0;i<4;i++){ le[i] = (uint8_t)(bits & 0xff); bits >>= 8; }
    cfasl_put_bytes(b, le, 4);
}

void cfasl_put_f64(cfasl_out* b, double v){
    uint64_t bits; memcpy(&bits, &v, 8);
    uint8_t le[8];
    for(int i=0;i<8;i++){ le[i] = (uint8_t)(bits & 0xff); bits >>= 8; }
    cfasl_put_bytes(b, le, 8);
}

/* ---- Input cursor ------------------------------------------------------- */

void cfasl_in_init(cfasl_in* c, const uint8_t* data, size_t len){
    c->data = data; c->len = len; c->pos = 0; c->error = 0;
}

int cfasl_at_end(cfasl_in* c){ return c->pos >= c->len; }

uint8_t cfasl_get_u8(cfasl_in* c){
    if(c->pos >= c->len){ c->error = 1; return 0; }
    return c->data[c->pos++];
}

void cfasl_get_bytes(cfasl_in* c, void* dst, size_t n){
    if(c->pos + n > c->len){ c->error = 1; return; }
    memcpy(dst, c->data + c->pos, n);
    c->pos += n;
}

uint64_t cfasl_get_uvarint(cfasl_in* c){
    uint64_t v = 0; int shift = 0;
    for(;;){
        if(c->pos >= c->len){ c->error = 1; return v; }
        uint8_t byte = c->data[c->pos++];
        v |= (uint64_t)(byte & 0x7fu) << shift;
        if(!(byte & 0x80u)){ break; }
        shift += 7;
        if(shift >= 64){ c->error = 1; break; }
    }
    return v;
}

int64_t cfasl_get_svarint(cfasl_in* c){
    uint64_t zz = cfasl_get_uvarint(c);
    return (int64_t)(zz >> 1) ^ -(int64_t)(zz & 1);   /* un-zig-zag */
}

float cfasl_get_f32(cfasl_in* c){
    uint8_t le[4]; cfasl_get_bytes(c, le, 4);
    uint32_t bits = 0;
    for(int i=3;i>=0;i--){ bits = (bits << 8) | le[i]; }
    float v; memcpy(&v, &bits, 4); return v;
}

double cfasl_get_f64(cfasl_in* c){
    uint8_t le[8]; cfasl_get_bytes(c, le, 8);
    uint64_t bits = 0;
    for(int i=7;i>=0;i--){ bits = (bits << 8) | le[i]; }
    double v; memcpy(&v, &bits, 8); return v;
}

/* ======================================================================== */
/* Object-graph writer (cfasl-2)                                            */
/* ======================================================================== */

/* Streaming shared-object table: a private open-addressing hash mapping an
   object pointer to the id assigned the first time it was emitted.  Because GC
   is non-moving and the writer allocates no bocl objects, the raw Ref pointer
   is a stable key for the lifetime of the serialization. */
struct cfasl_wctx {
    cfasl_out* out;
    void**     keys;      /* NULL slot = empty                                */
    uint64_t*  ids;
    size_t     cap;       /* power of two (0 while unallocated)               */
    size_t     count;
    uint64_t   next_id;   /* id to assign to the next freshly-emitted object  */
    int        error;     /* unsupported type or OOM                          */
};

cfasl_wctx* cfasl_wctx_new(cfasl_out* out){
    cfasl_wctx* w = (cfasl_wctx*)calloc(1, sizeof(*w));
    if(!w){ return NULL; }
    w->out = out;
    return w;
}

void cfasl_wctx_free(cfasl_wctx* w){
    if(!w){ return; }
    free(w->keys);
    free(w->ids);
    free(w);
}

int cfasl_wctx_error(cfasl_wctx* w){
    return !w || w->error || (w->out && w->out->error);
}

static size_t cfasl_ptr_hash(const void* p){
    /* fibonacci/murmur-style finalizer so the low pointer bits mix into the
       whole word (adjacent objects hash far apart). */
    uintptr_t x = (uintptr_t)p;
    x ^= x >> 33; x *= (uintptr_t)0xff51afd7ed558ccdULL;
    x ^= x >> 33; x *= (uintptr_t)0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return (size_t)x;
}

static int cfasl_wctx_grow(cfasl_wctx* w){
    size_t ncap = w->cap ? w->cap*2 : 256;
    void**    nk = (void**)calloc(ncap, sizeof(*nk));
    uint64_t* ni = (uint64_t*)malloc(ncap*sizeof(*ni));
    if(!nk || !ni){ free(nk); free(ni); w->error = 1; return 0; }
    for(size_t i=0;i<w->cap;i++){
        if(!w->keys[i]){ continue; }
        size_t m = ncap - 1, j = cfasl_ptr_hash(w->keys[i]) & m;
        while(nk[j]){ j = (j+1) & m; }
        nk[j] = w->keys[i]; ni[j] = w->ids[i];
    }
    free(w->keys); free(w->ids);
    w->keys = nk; w->ids = ni; w->cap = ncap;
    return 1;
}

/* If OBJ is already in the table, store its id and return 1; else return 0. */
static int cfasl_wctx_lookup(cfasl_wctx* w, Ref obj, uint64_t* id_out){
    if(!w->cap){ return 0; }
    size_t m = w->cap - 1, j = cfasl_ptr_hash(obj) & m;
    while(w->keys[j]){
        if(w->keys[j] == (void*)obj){ *id_out = w->ids[j]; return 1; }
        j = (j+1) & m;
    }
    return 0;
}

/* Record OBJ->id (caller guarantees OBJ is not already present). */
static void cfasl_wctx_insert(cfasl_wctx* w, Ref obj, uint64_t id){
    if((w->count+1)*10 >= (w->cap?w->cap:1)*7){    /* keep load factor < 0.7 */
        if(!cfasl_wctx_grow(w)){ return; }
    }
    size_t m = w->cap - 1, j = cfasl_ptr_hash(obj) & m;
    while(w->keys[j]){ j = (j+1) & m; }
    w->keys[j] = (void*)obj; w->ids[j] = id; w->count++;
}

/* Assign the next id to OBJ, record it, and return that id.  Composite objects
   call this BEFORE emitting their sub-components so a cycle back to OBJ emits a
   BACKREF instead of recursing forever. */
static uint64_t cfasl_wctx_assign(cfasl_wctx* w, Ref obj){
    uint64_t id = w->next_id++;
    cfasl_wctx_insert(w, obj, id);
    return id;
}

void cfasl_write_object(cfasl_wctx* w, Ref obj){
    if(cfasl_wctx_error(w)){ return; }

    /* ---- immediates: value-encoded, never shared, get no id ------------- */
    if(nullp(obj)){        cfasl_put_u8(w->out, CFASL_TAG_NIL); return; }
    if(obj == T_Symbol){   cfasl_put_u8(w->out, CFASL_TAG_T);   return; }
    if(integerp(obj) && !bignump(obj)){
        cfasl_put_u8(w->out, CFASL_TAG_FIXNUM);
        cfasl_put_svarint(w->out, (int64_t)integer_value(obj));
        return;
    }
    if(characterp(obj)){
        cfasl_put_u8(w->out, CFASL_TAG_CHARACTER);
        cfasl_put_uvarint(w->out, (uint64_t)integer_value(character_code(obj)));
        return;
    }
    if(floatp(obj)){
        cfasl_put_u8(w->out, CFASL_TAG_DOUBLE_FLOAT);
        cfasl_put_f64(w->out, float_value(obj));
        return;
    }

    /* ---- shared objects: emit a BACKREF on a repeat/cycle --------------- */
    uint64_t id;
    if(cfasl_wctx_lookup(w, obj, &id)){
        cfasl_put_u8(w->out, CFASL_TAG_BACKREF);
        cfasl_put_uvarint(w->out, id);
        return;
    }

    if(consp(obj)){
        cfasl_wctx_assign(w, obj);              /* id before car, for cycles */
        cfasl_put_u8(w->out, CFASL_TAG_CONS);
        cfasl_write_object(w, cons_car(obj));
        cfasl_write_object(w, cons_cdr(obj));
        return;
    }
    if(stringp(obj)){
        cfasl_wctx_assign(w, obj);
        cfasl_put_u8(w->out, CFASL_TAG_STRING);
        uword n = string_length(obj);
        cfasl_put_uvarint(w->out, (uint64_t)n);
        cfasl_put_bytes(w->out, string_octets(obj), (size_t)n);
        return;
    }
    if(symbolp(obj)){
        cfasl_wctx_assign(w, obj);
        if(keywordp(obj)){
            cfasl_put_u8(w->out, CFASL_TAG_KEYWORD);
            cfasl_write_object(w, symbol_name(obj));
            return;
        }
        Ref pkg = symbol_package(obj);
        if(nullp(pkg)){
            cfasl_put_u8(w->out, CFASL_TAG_UNINTERNED);
            cfasl_write_object(w, symbol_name(obj));
            return;
        }
        cfasl_put_u8(w->out, CFASL_TAG_SYMBOL);
        cfasl_write_object(w, pkg->slots->package.name);   /* package-name string */
        cfasl_write_object(w, symbol_name(obj));
        return;
    }
    if(ratiop(obj)){
        cfasl_wctx_assign(w, obj);
        cfasl_put_u8(w->out, CFASL_TAG_RATIO);
        cfasl_write_object(w, ratio_numerator(obj));   /* may itself be a bignum -> error */
        cfasl_write_object(w, ratio_denominator(obj));
        return;
    }
    if(obj->class == VECTOR_Class){        /* a genuine simple-vector (svref-able) */
        cfasl_wctx_assign(w, obj);
        cfasl_put_u8(w->out, CFASL_TAG_SIMPLE_VECTOR);
        uword n = vector_length(obj);
        cfasl_put_uvarint(w->out, (uint64_t)n);
        for(uword i=0;i<n;i++){ cfasl_write_object(w, svref(obj, integer_from_word((word)i))); }
        return;
    }

    if(compiled_function_p(obj)){          /* vm-8: a byte-code template */
        cfasl_wctx_assign(w, obj);         /* id before fields (record-before, for sharing) */
        cfasl_put_u8(w->out, CFASL_TAG_COMPILED_FUNCTION);
        cfasl_put_uvarint(w->out, (uint64_t)BYTECODE_VERSION);
        cfasl_write_object(w, compiled_function_name(obj));
        cfasl_write_object(w, compiled_function_lambda_list(obj));
        cfasl_write_object(w, compiled_function_code(obj));        /* the octet string */
        cfasl_write_object(w, compiled_function_constants(obj));   /* pool; may nest templates */
        cfasl_write_object(w, compiled_function_arity(obj));
        cfasl_write_object(w, compiled_function_max_stack(obj));
        cfasl_write_object(w, compiled_function_n_locals(obj));
        cfasl_write_object(w, compiled_function_source(obj));
        return;
    }

    /* Unsupported here (bignum, structure, bit-vector, non-simple arrays,
       streams, ...): flag the context so the caller keeps the text fasl.  These
       are rare in top-level forms; cfasl-6 extends coverage. */
    w->error = 1;
}

/* ======================================================================== */
/* Object-graph reader (cfasl-3)                                             */
/* ======================================================================== */

/* Mirror of the writer's shared-object table: an id -> object array, filled in
   emission order so CFASL_TAG_BACKREF resolves.  The reader ALLOCATES bocl
   objects and holds them (in this C array and in C locals) across FURTHER
   allocations that can trigger GC.  Object_new() balances its own live pool, so
   a freshly constructed object is NOT left rooted — the caller must root it.  We
   therefore add_to_live_pool() every recorded object (and every intermediate
   held across a nested read); the caller's per-form live pool frame trims them
   once the form is evaluated.  Containers are recorded (or their id reserved)
   BEFORE their sub-components, so cycles reconstruct. */
struct cfasl_rctx {
    cfasl_in* in;
    Ref*      objs;
    size_t    cap;
    size_t    count;   /* == next id to assign */
    int       error;
    int       lenient_packages;   /* validation mode: an unknown package reads as
        an UNINTERNED symbol instead of an error.  The real loader must NOT set
        this: there, a missing package is a genuine failure (the defining form
        should have run).  A validator cannot evaluate the package-DEFINING
        forms, so without this the verdict would depend on which packages the
        SESSION happens to have (a file defining its own package would validate
        only after something else created it). */
};

cfasl_rctx* cfasl_rctx_new(cfasl_in* in){
    cfasl_rctx* r = (cfasl_rctx*)calloc(1, sizeof(*r));
    if(!r){ return NULL; }
    r->in = in;
    return r;
}

void cfasl_rctx_free(cfasl_rctx* r){
    if(!r){ return; }
    free(r->objs);
    free(r);
}

int cfasl_rctx_error(cfasl_rctx* r){
    return !r || r->error || (r->in && r->in->error);
}

void cfasl_rctx_set_lenient_packages(cfasl_rctx* r, int flag){
    if(r){ r->lenient_packages = flag; }
}

/* Append an id slot (holding OBJ) and return its id.  OBJ is rooted in the
   caller's live pool so a later allocation's GC cannot reclaim it out from under
   a still-pending BACKREF (Object_new does not leave its result rooted). */
static size_t cfasl_rctx_record(cfasl_rctx* r, Ref obj){
    if(r->count == r->cap){
        size_t ncap = r->cap ? r->cap*2 : 64;
        Ref* n = (Ref*)realloc(r->objs, ncap*sizeof(*n));
        if(!n){ r->error = 1; return r->count; }
        r->objs = n; r->cap = ncap;
    }
    r->objs[r->count] = obj;
    add_to_live_pool(obj);
    return r->count++;
}

/* Patch a reserved id slot with the finished object and root it (the reserve
   recorded only a NIL placeholder, so the real object still needs rooting). */
static void cfasl_rctx_patch(cfasl_rctx* r, size_t id, Ref obj){
    r->objs[id] = obj;
    add_to_live_pool(obj);
}

Ref cfasl_read_object(cfasl_rctx* r){
    if(cfasl_rctx_error(r)){ return NIL_Symbol; }
    uint8_t tag = cfasl_get_u8(r->in);
    switch(tag){
    case CFASL_TAG_NIL:          return NIL_Symbol;
    case CFASL_TAG_T:            return T_Symbol;
    case CFASL_TAG_FIXNUM:       return integer_from_word((word)cfasl_get_svarint(r->in));
    case CFASL_TAG_CHARACTER:    return code_character(integer_from_word((word)cfasl_get_uvarint(r->in)));
    case CFASL_TAG_DOUBLE_FLOAT: return float_from_floating(cfasl_get_f64(r->in));
    case CFASL_TAG_BACKREF:{
        uint64_t id = cfasl_get_uvarint(r->in);
        if(id >= r->count){ r->error = 1; return NIL_Symbol; }
        return r->objs[id];
    }
    case CFASL_TAG_CONS:{
        Ref cell = cons(NIL_Symbol, NIL_Symbol);   /* real cell first, for cycles */
        size_t id = cfasl_rctx_record(r, cell);
        cons_rplaca(cell, cfasl_read_object(r));
        cons_rplacd(cell, cfasl_read_object(r));
        (void)id;
        return cell;
    }
    case CFASL_TAG_STRING:{
        uint64_t n = cfasl_get_uvarint(r->in);
        Ref s = string_new((halfword)n);
        cfasl_rctx_record(r, s);
        cfasl_get_bytes(r->in, string_octets(s), (size_t)n);
        return s;
    }
    case CFASL_TAG_SYMBOL:{
        size_t id = cfasl_rctx_record(r, NIL_Symbol);   /* reserve id before components */
        Ref pkgname = cfasl_read_object(r);             /* recorded+rooted by its STRING case */
        Ref name    = cfasl_read_object(r);
        Ref sym = NIL_Symbol;
        if(!r->error){
            char* p = string_cstring(pkgname);
            char* nm = string_cstring(name);
            Ref pkg = package_find_cstring(p);
            if(nullp(pkg)){
                if(r->lenient_packages){ sym = symbol_new(name); }   /* validate mode */
                else{ r->error = 1; }}                   /* package must exist by now */
            else { sym = package_intern_cstring_in(nm, pkg); }
            free(p); free(nm);
        }
        cfasl_rctx_patch(r, id, sym);
        return sym;
    }
    case CFASL_TAG_KEYWORD:{
        size_t id = cfasl_rctx_record(r, NIL_Symbol);
        Ref name = cfasl_read_object(r);
        Ref sym = NIL_Symbol;
        if(!r->error){
            char* nm = string_cstring(name);
            sym = package_intern_cstring_in(nm, KEYWORD_Package);
            free(nm);
        }
        cfasl_rctx_patch(r, id, sym);
        return sym;
    }
    case CFASL_TAG_UNINTERNED:{
        size_t id = cfasl_rctx_record(r, NIL_Symbol);
        Ref name = cfasl_read_object(r);
        Ref sym = r->error ? NIL_Symbol : symbol_new(name);
        cfasl_rctx_patch(r, id, sym);
        return sym;
    }
    case CFASL_TAG_RATIO:{
        size_t id = cfasl_rctx_record(r, NIL_Symbol);
        Ref num = cfasl_read_object(r);
        add_to_live_pool(num);                    /* hold NUM across the DEN read (may be an unrooted fixnum) */
        Ref den = cfasl_read_object(r);
        Ref rat = r->error ? NIL_Symbol : ratio_new(num, den);
        cfasl_rctx_patch(r, id, rat);
        return rat;
    }
    case CFASL_TAG_SIMPLE_VECTOR:{
        uint64_t n = cfasl_get_uvarint(r->in);
        Ref lst = NIL_Symbol;                       /* length-n placeholder */
        for(uint64_t i=0;i<n;i++){ lst = cons(NIL_Symbol, lst); add_to_live_pool(lst); }
        Ref vec = vector_new(lst);
        cfasl_rctx_record(r, vec);                  /* id before elements; roots vec */
        for(uint64_t i=0;i<n && !r->error;i++){
            svref_set(vec, integer_from_word((word)i), cfasl_read_object(r));
        }
        return vec;
    }
    case CFASL_TAG_COMPILED_FUNCTION:{          /* vm-8: a byte-code template */
        size_t id = cfasl_rctx_record(r, NIL_Symbol);   /* reserve id before fields */
        uint64_t bcver = cfasl_get_uvarint(r->in);
        if(bcver != (uint64_t)BYTECODE_VERSION){         /* incompatible byte-code */
            r->error = 1; return NIL_Symbol; }
        Ref name   = cfasl_read_object(r); add_to_live_pool(name);
        Ref ll     = cfasl_read_object(r); add_to_live_pool(ll);
        Ref code   = cfasl_read_object(r); add_to_live_pool(code);
        Ref consts = cfasl_read_object(r); add_to_live_pool(consts);
        Ref arity  = cfasl_read_object(r); add_to_live_pool(arity);
        Ref maxst  = cfasl_read_object(r); add_to_live_pool(maxst);
        Ref nloc   = cfasl_read_object(r); add_to_live_pool(nloc);
        Ref source = cfasl_read_object(r); add_to_live_pool(source);
        Ref cf = r->error ? NIL_Symbol
                 : compiled_function_new(name, ll, code, consts, arity, maxst, nloc, source);
        cfasl_rctx_patch(r, id, cf);
        return cf;
    }
    default:
        r->error = 1;
        return NIL_Symbol;
    }
}
