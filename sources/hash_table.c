#include <ctype.h>
#include <string.h>
#include "hash_table.h"
#include "kernel.h"
#include "cons.h"

extern Ref Object_new(Ref class, Slots* slots);

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
    that->objcount = 3;
    that->test    = test;
    that->entries = NIL_Symbol;
    that->count   = I(0);
    return that;}

Ref hash_table_new(Ref test){
    /* Validate test up front. */
    (void)resolve_test(test);
    return Object_new(HASH_TABLE_Class, (Slots*)HashTable_slots(test));}

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                     */
/* ------------------------------------------------------------------ */

static HashTable* check_ht(Ref ht){
    check_type(ht, HASH_TABLE_Class);
    return &ht->slots->hash_table;}

/* Returns the matching (key . value) cons, or NIL_Symbol if absent. */
static Ref find_entry(HashTable* slots, Ref key){
    test_fn cmp = resolve_test(slots->test);
    Ref e = slots->entries;
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
    /* Other keywords (:size, :rehash-size, …) are accepted and ignored. */
    results->count = 1;
    results->values[0] = hash_table_new(test);
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
        slots->entries = cons(cons(key, value), slots->entries);
        slots->count = integer_from_word(integer_value(slots->count) + 1);
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
    test_fn cmp = resolve_test(slots->test);
    /* Walk the alist, splicing out the first matching entry. */
    Ref prev = NIL_Symbol;
    Ref node = slots->entries;
    while(consp(node)){
        Ref pair = cons_car(node);
        if(cmp(cons_car(pair), key)){
            if(nullp(prev)){
                slots->entries = cons_cdr(node);
            } else {
                cons_rplacd(prev, cons_cdr(node));}
            slots->count = integer_from_word(integer_value(slots->count) - 1);
            results->count = 1;
            results->values[0] = T_Symbol;
            close_live_pool(pool);
            return;}
        prev = node;
        node = cons_cdr(node);}
    results->count = 1;
    results->values[0] = NIL_Symbol;
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
    Ref e = slots->entries;
    MultipleValues inner = {0};
    while(consp(e)){
        Ref pair = cons_car(e);
        Ref args = cons(cons_car(pair), cons(cons_cdr(pair), NIL_Symbol));
        kernel_apply(fn, args, &inner);
        e = cons_cdr(e);}
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
    register_fn(S(GETHASH),           ll_k_ht,   kernel_GETHASH);
    register_fn(S(SETHASH),           ll_k_v_ht, kernel_SETHASH);
    register_fn(S(REMHASH),           ll_k_ht,   kernel_REMHASH);
    register_fn(S(CLRHASH),           ll_ht,     kernel_CLRHASH);
    register_fn(S(MAPHASH),           ll_fn_ht,  kernel_MAPHASH);
}
