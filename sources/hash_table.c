#include <ctype.h>
#include <string.h>
#include "hash_table.h"
#include "kernel.h"
#include "cons.h"

extern Ref Object_new(Ref class, Slots* slots);
extern Array* Array_slots(Ref element_type, halfword rank, halfword* dimensions);

/* ------------------------------------------------------------------ */
/*  EQ + EQUALP                                                         */
/*                                                                       */
/*  The existing kernel has EQL and EQUAL but not EQ (pointer equality)  */
/*  or EQUALP (case-insensitive equal + element-wise compare). Adding    */
/*  them here so the hash-table TEST dispatch can use the full set.      */
/* ------------------------------------------------------------------ */

bool eq(Ref a, Ref b){ return a == b; }

/* Case-insensitive string equality. */
static bool string_equalp(Ref a, Ref b){
    if(a == b){ return true; }
    uword la = string_length(a);
    if(la != string_length(b)){ return false; }
    octet* ea = string_octets(a);
    octet* eb = string_octets(b);
    for(uword i = 0; i < la; i++){
        if(tolower(ea[i]) != tolower(eb[i])){ return false; }}
    return true;}

bool equalp(Ref a, Ref b){
    /* Numbers: cross-type numeric equality. */
    if((integerp(a) || floatp(a)) && (integerp(b) || floatp(b))){
        floating av = integerp(a) ? (floating)integer_value(a) : float_value(a);
        floating bv = integerp(b) ? (floating)integer_value(b) : float_value(b);
        return av == bv;}
    /* Characters: case-insensitive. */
    if(characterp(a) && characterp(b)){
        word ca = integer_value(character_code(a));
        word cb = integer_value(character_code(b));
        return tolower((int)ca) == tolower((int)cb);}
    /* Strings: case-insensitive. */
    if(stringp(a) && stringp(b)){ return string_equalp(a, b); }
    /* Conses: element-wise EQUALP. */
    if(consp(a) && consp(b)){
        return equalp(car(a), car(b)) && equalp(cdr(a), cdr(b));}
    /* Fall back to EQUAL semantics for anything else. */
    return equal(a, b);}

/* ------------------------------------------------------------------ */
/*  Comparator dispatch                                                  */
/* ------------------------------------------------------------------ */

typedef bool (*test_fn)(Ref, Ref);

/* Normalise a hash-table :TEST designator to its canonical symbol.  CL allows
   either a symbol (EQ/EQL/EQUAL/EQUALP) or the corresponding function object
   (#'eq …).  A function object passed as #'equal is, by identity, the value of
   the EQUAL symbol's function cell, so we can map it back. */
static Ref canonical_test_symbol(Ref test){
    if(test==S(EQ) || test==S(EQL) || test==S(EQUAL) || test==S(EQUALP)){
        return test;}
    if(test==symbol_function(S(EQ)))     { return S(EQ); }
    if(test==symbol_function(S(EQL)))    { return S(EQL); }
    if(test==symbol_function(S(EQUAL)))  { return S(EQUAL); }
    if(test==symbol_function(S(EQUALP))) { return S(EQUALP); }
    return test;}   /* unrecognised — let resolve_test signal the error */

static test_fn resolve_test(Ref test_symbol){
    if(test_symbol == S(EQ))     { return eq; }
    if(test_symbol == S(EQL))    { return eql; }
    if(test_symbol == S(EQUAL))  { return equal; }
    if(test_symbol == S(EQUALP)) { return equalp; }
    ERROR("HASH-TABLE: unsupported test function %s",
          string_cstring(prin1_to_string(test_symbol)));}

/* ------------------------------------------------------------------ */
/*  Constructor                                                          */
/* ------------------------------------------------------------------ */

static HashTable* HashTable_slots(Ref test){
    HashTable* that = allocate(sizeof(*that));
    that->objcount = 6;
    that->test    = test;
    that->entries = NIL_Symbol;
    that->count   = I(0);
    that->nominal_size     = I(16);    /* CL defaults; overridden by MAKE-HASH-TABLE */
    that->rehash_size      = F(1.5);
    that->rehash_threshold = F(1.0);
    return that;}

Ref hash_table_new(Ref test){
    /* Accept a function-object designator (#'equal) as well as a symbol. */
    test = canonical_test_symbol(test);
    /* Validate test up front. */
    (void)resolve_test(test);
    return Object_new(HASH_TABLE_Class, (Slots*)HashTable_slots(test));}

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                     */
/* ------------------------------------------------------------------ */

static HashTable* check_ht(Ref ht){
    check_type(ht, HASH_TABLE_Class);
    return &ht->slots->hash_table;}

/* ------------------------------------------------------------------ */
/*  Bucketed storage.                                                    */
/*                                                                       */
/*  ENTRIES is NIL (empty table) or a simple-vector of N buckets, each   */
/*  bucket an alist ((key . value) ...).  OBJECT_HASH chooses the bucket; */
/*  it is consistent with EQUALP (case-insensitive chars/strings, cross- */
/*  type numbers), so it is also valid for the finer EQ/EQL/EQUAL tests  */
/*  — those just see extra collisions, resolved by the per-bucket TEST.   */
/* ------------------------------------------------------------------ */

#define HT_INITIAL_BUCKETS 16
#define HT_LOAD_FACTOR     2          /* resize when count > buckets*this */

static uword string_hash_ci(Ref s){
    uword h = 2166136261u;
    uword n = string_length(s);
    octet* o = string_octets(s);
    for(uword i = 0; i < n; i++){
        h = (h ^ (uword)(unsigned char)tolower((int)o[i])) * 16777619u;}
    return h;}

static uword object_hash(Ref key, int depth){
    if(depth <= 0){ return 7u; }
    if(integerp(key) || floatp(key)){
        floating v = integerp(key) ? (floating)integer_value(key) : float_value(key);
        return (uword)(word)v; }
    if(characterp(key)){
        return (uword)tolower((int)integer_value(character_code(key))); }
    if(symbolp(key)){ return string_hash_ci(symbol_name(key)) * 31u + 17u; }
    if(stringp(key)){ return string_hash_ci(key); }
    if(consp(key)){
        return object_hash(cons_car(key), depth - 1) * 1000003u
             + object_hash(cons_cdr(key), depth - 1) + 1u; }
    return 13u;}

/* A fresh simple-vector of N NIL buckets (Array_slots NIL-inits its elements). */
static Ref make_bucket_vector(halfword n){
    halfword dims[1]; dims[0] = n;
    return Object_new(VECTOR_Class, (Slots*)Array_slots(T_Symbol, 1, dims));}

/* Direct, allocation-free access to a bucket vector's Ref[] storage.  A bucket
   vector is a VECTOR_Class object wrapping an Array whose ELEMENTS is the
   VectorOfObject holding the buckets (cf. kernel.c svref / class_of(VECTOR)).
   Using raw C indices avoids INTEGER_FROM_WORD, which allocates for indices
   >= CHAR_CODE_LIMIT and would let a GC reclaim the (unrooted) new vector
   mid-rehash.  bocl's GC is non-moving, so the returned pointer stays valid. */
static Ref* ht_storage(Ref vec, halfword* n_out){
    VectorOfObject* vo = vec->slots->array.elements;
    *n_out = (halfword)object_slot_count(vo);   /* mask off GC flag bits (old/mark) */
    return vo->elements;}

/* Write-barrier the bucket VectorOfObject behind ENTRIES.  A raw st[bi]=node
   store puts a (possibly young) cons into the bucket vector's element storage,
   which is a block separate from the hash-table slots block. */
static void ht_remember_buckets(Ref entries){
    gc_write_barrier(entries->slots->array.elements);}

static halfword ht_nbuckets(HashTable* slots){
    halfword n; ht_storage(slots->entries, &n); return n;}

/* Double the bucket count and redistribute — re-links the EXISTING spine conses
   into the new vector, so the only allocation is the new vector itself (after
   which there is no allocation, hence no GC, until it is installed). */
static void ht_resize(HashTable* slots){
    halfword oldn; Ref* old_st = ht_storage(slots->entries, &oldn);
    halfword newn = (halfword)(oldn * 2);
    Ref old_vec = slots->entries;                 /* reachable via slots (rooted) */
    Ref new_vec = make_bucket_vector(newn);        /* the only allocation here */
    halfword nn;  Ref* new_st = ht_storage(new_vec, &nn);
    old_st = ht_storage(old_vec, &oldn);           /* re-fetch (GC may have run) */
    for(halfword bi = 0; bi < oldn; bi++){
        Ref node = old_st[bi];
        while(consp(node)){
            Ref next = cons_cdr(node);
            Ref pair = cons_car(node);
            uword idx = object_hash(cons_car(pair), 6) % (uword)newn;
            cons_rplacd(node, new_st[idx]);
            new_st[idx] = node;
            node = next;}}
    slots->entries = new_vec;
    gc_write_barrier(slots);}   /* entries lives in the hash-table slots block */

/* Returns the matching (key . value) cons in KEY's bucket, or NIL if absent. */
static Ref find_entry(HashTable* slots, Ref key){
    if(nullp(slots->entries)){ return NIL_Symbol; }
    test_fn cmp = resolve_test(slots->test);
    halfword n; Ref* st = ht_storage(slots->entries, &n);
    Ref e = st[object_hash(key, 6) % (uword)n];
    while(consp(e)){
        Ref pair = cons_car(e);
        if(cmp(cons_car(pair), key)){ return pair; }
        e = cons_cdr(e);}
    return NIL_Symbol;}

/* ------------------------------------------------------------------ */
/*  Kernel built-ins                                                     */
/* ------------------------------------------------------------------ */

static void kernel_MAKE_HASH_TABLE(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref test = pop_key_argument(function, &current, K(TEST), S(EQL));
    Ref ht   = hash_table_new(test);
    add_to_live_pool(ht);
    /* Store :SIZE / :REHASH-SIZE / :REHASH-THRESHOLD verbatim for the accessors
       (bocl's alist table never rehashes, so they are only carried, not used).
       Each is popped then stored straight into the rooted HT — no allocation in
       between, so no rooting gap. */
    HashTable* slots = &ht->slots->hash_table;
    slots->nominal_size     = pop_key_argument(function, &current, K(SIZE),             I(16));
    slots->rehash_size      = pop_key_argument(function, &current, K(REHASH-SIZE),      F(1.5));
    slots->rehash_threshold = pop_key_argument(function, &current, K(REHASH-THRESHOLD), F(1.0));
    results->count = 1;
    results->values[0] = ht;
    close_live_pool(pool);}

static void kernel_HASH_TABLE_P(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref obj = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = as_boolean(typep(obj, HASH_TABLE_Class));
    close_live_pool(pool);}

static void kernel_HASH_TABLE_COUNT(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref ht = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = check_ht(ht)->count;
    close_live_pool(pool);}

static void kernel_HASH_TABLE_TEST(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref ht = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = check_ht(ht)->test;
    close_live_pool(pool);}

static void kernel_HASH_TABLE_SIZE(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref ht = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = check_ht(ht)->nominal_size;
    close_live_pool(pool);}

static void kernel_HASH_TABLE_REHASH_SIZE(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref ht = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = check_ht(ht)->rehash_size;
    close_live_pool(pool);}

static void kernel_HASH_TABLE_REHASH_THRESHOLD(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref ht = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = check_ht(ht)->rehash_threshold;
    close_live_pool(pool);}

/* (GETHASH key hash-table &optional default) — returns two values:
   the value found (or default), and a boolean whether the key was present. */
static void kernel_GETHASH(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref key = pop_mandatory_argument(function, &current);
    Ref ht = pop_mandatory_argument(function, &current);
    Ref def = nullp(current) ? NIL_Symbol : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    Ref pair = find_entry(check_ht(ht), key);
    if(nullp(pair)){
        results->count = 2;
        results->values[0] = def;
        results->values[1] = NIL_Symbol;
    } else {
        results->count = 2;
        results->values[0] = cons_cdr(pair);
        results->values[1] = T_Symbol;}
    close_live_pool(pool);}

/* (SETHASH key hash-table value) — no SETF macros yet, so a primitive.
   Returns the value (CL convention for (setf (gethash …) v)). */
static void kernel_SETHASH(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref key = pop_mandatory_argument(function, &current);
    Ref ht = pop_mandatory_argument(function, &current);
    Ref value = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    HashTable* slots = check_ht(ht);
    Ref pair = find_entry(slots, key);
    if(nullp(pair)){
        /* Lazily allocate the bucket vector on first insert — assigned with no
           intervening allocation, so there is no rooting gap. */
        if(nullp(slots->entries)){
            slots->entries = make_bucket_vector(HT_INITIAL_BUCKETS);}
        Ref newpair = cons(key, value);                       /* rooted in pool */
        halfword n; Ref* st = ht_storage(slots->entries, &n);
        uword bi = object_hash(key, 6) % (uword)n;
        Ref node = cons(newpair, st[bi]);   /* newpair rooted; st stable (non-moving) */
        st[bi] = node;                       /* st still valid — GC does not move it */
        ht_remember_buckets(slots->entries); /* young node stored into bucket vector */
        slots->count = integer_from_word(integer_value(slots->count) + 1);
        gc_write_barrier(slots);             /* entries + count live in the slots block */
        if((uword)integer_value(slots->count) > (uword)n * HT_LOAD_FACTOR){
            ht_resize(slots);}
    } else {
        cons_rplacd(pair, value);}
    results->count = 1;
    results->values[0] = value;
    close_live_pool(pool);}

/* (REMHASH key hash-table) — returns T if the key was present and removed. */
static void kernel_REMHASH(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref key = pop_mandatory_argument(function, &current);
    Ref ht = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    HashTable* slots = check_ht(ht);
    results->count = 1;
    results->values[0] = NIL_Symbol;
    if(nullp(slots->entries)){ close_live_pool(pool); return; }
    test_fn cmp = resolve_test(slots->test);
    /* Walk KEY's bucket alist, splicing out the first matching entry. */
    halfword n; Ref* st = ht_storage(slots->entries, &n);
    uword bi = object_hash(key, 6) % (uword)n;
    Ref prev = NIL_Symbol;
    Ref node = st[bi];
    while(consp(node)){
        Ref pair = cons_car(node);
        if(cmp(cons_car(pair), key)){
            if(nullp(prev)){
                st[bi] = cons_cdr(node);
                ht_remember_buckets(slots->entries);
            } else {
                cons_rplacd(prev, cons_cdr(node));}
            slots->count = integer_from_word(integer_value(slots->count) - 1);
            gc_write_barrier(slots);
            results->values[0] = T_Symbol;
            close_live_pool(pool);
            return;}
        prev = node;
        node = cons_cdr(node);}
    close_live_pool(pool);}

static void kernel_CLRHASH(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref ht = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    HashTable* slots = check_ht(ht);
    slots->entries = NIL_Symbol;
    slots->count = I(0);
    gc_write_barrier(slots);
    results->count = 1;
    results->values[0] = ht;
    close_live_pool(pool);}

/* (MAPHASH function hash-table) — calls function with two args (key, value)
   for each entry. Returns NIL. */
static void kernel_MAPHASH(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref fn = pop_mandatory_argument(function, &current);
    Ref ht = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    HashTable* slots = check_ht(ht);
    MultipleValues inner = {0};
    if(!nullp(slots->entries)){
        /* nbuckets only ever grows (resize never shrinks), so a bucket index
           captured up front stays in range even if FN mutates the table; the
           storage pointer is re-fetched each bucket in case FN resized it. */
        halfword n = ht_nbuckets(slots);
        for(halfword bi = 0; bi < n; bi++){
            halfword cur; Ref* st = ht_storage(slots->entries, &cur);
            if(bi >= cur){ break; }
            Ref e = st[bi];
            while(consp(e)){
                Ref pair = cons_car(e);
                Ref args = cons(cons_car(pair), cons(cons_cdr(pair), NIL_Symbol));
                kernel_apply(fn, args, &inner);
                e = cons_cdr(e);}}}
    results->count = 1;
    results->values[0] = NIL_Symbol;
    close_live_pool(pool);}

/* EQ and EQUALP as kernel built-ins. */
static void kernel_EQ(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref a = pop_mandatory_argument(function, &current);
    Ref b = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = as_boolean(eq(a, b));
    close_live_pool(pool);}

static void kernel_EQUALP(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref a = pop_mandatory_argument(function, &current);
    Ref b = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = as_boolean(equalp(a, b));
    close_live_pool(pool);}

static void register_fn(Ref fname, Ref lambda_list, KernelFunctionPtr fn){
    set_symbol_function(fname, built_in_function_new(fname, lambda_list, fn));}

void kernel_initialize_hash_tables(void){
    Ref ll_a_b   = list(S(A), S(B), NULL);
    Ref ll_ht    = list(S(HASH-TABLE), NULL);
    Ref ll_k_ht  = list(S(KEY), S(HASH-TABLE), NULL);
    Ref ll_k_v_ht = list(S(KEY), S(HASH-TABLE), S(VALUE), NULL);
    Ref ll_fn_ht = list(S(FUNCTION), S(HASH-TABLE), NULL);

    register_fn(S(EQ),                ll_a_b,    kernel_EQ);
    register_fn(S(EQUALP),            ll_a_b,    kernel_EQUALP);

    register_fn(S(MAKE-HASH-TABLE),   NIL_Symbol, kernel_MAKE_HASH_TABLE);
    register_fn(S(HASH-TABLE-P),      list(S(OBJECT), NULL), kernel_HASH_TABLE_P);
    register_fn(S(HASH-TABLE-COUNT),  ll_ht,     kernel_HASH_TABLE_COUNT);
    register_fn(S(HASH-TABLE-TEST),   ll_ht,     kernel_HASH_TABLE_TEST);
    register_fn(S(HASH-TABLE-SIZE),              ll_ht, kernel_HASH_TABLE_SIZE);
    register_fn(S(HASH-TABLE-REHASH-SIZE),       ll_ht, kernel_HASH_TABLE_REHASH_SIZE);
    register_fn(S(HASH-TABLE-REHASH-THRESHOLD),  ll_ht, kernel_HASH_TABLE_REHASH_THRESHOLD);
    register_fn(S(GETHASH),           ll_k_ht,   kernel_GETHASH);
    register_fn(S(SETHASH),           ll_k_v_ht, kernel_SETHASH);
    register_fn(S(REMHASH),           ll_k_ht,   kernel_REMHASH);
    register_fn(S(CLRHASH),           ll_ht,     kernel_CLRHASH);
    register_fn(S(MAPHASH),           ll_fn_ht,  kernel_MAPHASH);
}
