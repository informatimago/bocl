#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel.h"
#include "cons.h"
#include "variable.h"

/* Phase 5.2: FUNCALL / APPLY / MAPCAR / MAPC / EVERY / SOME / NOTANY /
   NOTEVERY as kernel built-ins. These all evaluate the function argument
   to a callable object (Function / Closure / BuiltInFunction) and then
   kernel_apply it; the higher-order surface is the same for users and
   the kernel sequence layer above. */

/* (FUNCALL fn &rest args) — apply fn to args. */
static void kernel_FUNCALL(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref fn = pop_mandatory_argument(function, &current);
    /* `current` is now the list of args. */
    kernel_apply(fn, current, results);
    close_live_pool(pool);}

/* (APPLY fn arg1 arg2 ... last-list)
   The last argument is a list; the others are prepended individually. */
static void kernel_APPLY(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref fn = pop_mandatory_argument(function, &current);
    /* Flatten: take all but the last element of `current`, then splice in
       the elements of the last (which must be a list). */
    if(nullp(current)){
        ERROR("APPLY: %s","needs a final list argument");}
    /* Split current = (head1 head2 ... last-list). */
    Ref leading = NIL_Symbol;
    while(consp(cons_cdr(current))){
        push(cons_car(current), &leading);
        current = cons_cdr(current);}
    leading = nreverse(leading);
    Ref last_list = cons_car(current);
    /* Build leading @ last_list. */
    Ref args = NIL_Symbol;
    {Ref it = leading;
     while(consp(it)){ push(cons_car(it), &args); it = cons_cdr(it); }
     it = last_list;
     while(consp(it)){ push(cons_car(it), &args); it = cons_cdr(it); }}
    args = nreverse(args);
    kernel_apply(fn, args, results);
    close_live_pool(pool);}

/* Internal helper: given a function and a list of N lists, walk all N in
   parallel; for each tuple of corresponding cars, kernel_apply fn and
   invoke `visit` with the result. Stop when ANY list is exhausted. */
typedef void (*tuple_visitor)(Ref result_value, void* ctx);

static void map_walk(Ref fn, Ref lists, tuple_visitor visit, void* ctx){
    while(true){
        /* If any list is empty, stop. */
        {Ref it = lists;
         while(consp(it)){
             if(!consp(cons_car(it))){ return; }
             it = cons_cdr(it);}}
        /* Build the tuple of cars and advance the lists.
           push() closes its own GC pool, so the newly built cons cells are
           NOT in the live pool after each call.  Pin tuple and out after
           every push so a GC triggered by the next allocation cannot collect
           the partially-built lists. */
        /* Pin fn, lists and the incrementally-built tuple/out across the
           allocations that build them and the kernel_apply call.  push()
           closes its own GC frame so the resulting cons cells are NOT in
           the live pool after each call; re-pin them immediately.
           We also keep the pool open across kernel_apply and visit because
           collect_visit calls cons(), which can trigger GC. */
        uword mw_pool = open_live_pool();
        add_to_live_pool(fn);
        add_to_live_pool(lists);
        Ref tuple = NIL_Symbol;
        Ref out   = NIL_Symbol;
        {Ref it = lists;
         while(consp(it)){
             push(cons_car(cons_car(it)), &tuple); add_to_live_pool(tuple);
             push(cons_cdr(cons_car(it)), &out);   add_to_live_pool(out);
             it = cons_cdr(it);}
         tuple = nreverse(tuple);
         out   = nreverse(out);
         lists = out;}
        MultipleValues r = {0};
        kernel_apply(fn, tuple, &r);
        Ref result = (r.count > 0) ? r.values[0] : NIL_Symbol;
        add_to_live_pool(result);
        visit(result, ctx);
        close_live_pool(mw_pool);}}

/* MAPCAR — collect the results. */
typedef struct {
    Ref head;
    Ref tail;
} collect_ctx;

static void collect_visit(Ref v, void* p){
    collect_ctx* c = (collect_ctx*)p;
    if(nullp(c->head)){
        c->head = cons(v, NIL_Symbol);
        c->tail = c->head;
    } else {
        Ref new_cell = cons(v, NIL_Symbol);
        cons_rplacd(c->tail, new_cell);
        c->tail = new_cell;}}

static void kernel_MAPCAR(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref fn = pop_mandatory_argument(function, &current);
    if(nullp(current)){
        ERROR("MAPCAR: %s","needs at least one list");}
    collect_ctx c = { NIL_Symbol, NIL_Symbol };
    map_walk(fn, current, collect_visit, &c);
    results->count = 1;
    results->values[0] = c.head;
    close_live_pool(pool);}

/* MAPC — like MAPCAR but discard results, return the first list. */
typedef struct { int discard; } discard_ctx;
static void discard_visit(unused Ref v, unused void* p){ /* nothing */ }

static void kernel_MAPC(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref fn = pop_mandatory_argument(function, &current);
    if(nullp(current)){
        ERROR("MAPC: %s","needs at least one list");}
    Ref first_list = cons_car(current);
    discard_ctx c = { 0 };
    map_walk(fn, current, discard_visit, &c);
    results->count = 1;
    results->values[0] = first_list;
    close_live_pool(pool);}

/* EVERY — kernel_apply fn for each tuple; return NIL on the first false,
   else T. */
typedef struct { int found_false; int found_true; } pred_ctx;

static void every_visit(Ref v, void* p){
    pred_ctx* c = (pred_ctx*)p;
    if(c->found_false){ return; }
    if(nullp(v)){ c->found_false = 1; }}

static void kernel_EVERY(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref fn = pop_mandatory_argument(function, &current);
    if(nullp(current)){
        ERROR("EVERY: %s","needs at least one list");}
    pred_ctx c = { 0, 0 };
    map_walk(fn, current, every_visit, &c);
    results->count = 1;
    results->values[0] = c.found_false ? NIL_Symbol : T_Symbol;
    close_live_pool(pool);}

/* SOME — return the first non-NIL result, else NIL. */
typedef struct { Ref first_true; int captured; } some_ctx;

static void some_visit(Ref v, void* p){
    some_ctx* c = (some_ctx*)p;
    if(c->captured){ return; }
    if(!nullp(v)){ c->first_true = v; c->captured = 1; }}

static void kernel_SOME(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref fn = pop_mandatory_argument(function, &current);
    if(nullp(current)){
        ERROR("SOME: %s","needs at least one list");}
    some_ctx c = { NIL_Symbol, 0 };
    map_walk(fn, current, some_visit, &c);
    results->count = 1;
    results->values[0] = c.captured ? c.first_true : NIL_Symbol;
    close_live_pool(pool);}

/* NOTEVERY: (not (every ...)). NOTANY: (not (some ...)). Implemented by
   reusing EVERY/SOME counters. */
static void kernel_NOTEVERY(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref fn = pop_mandatory_argument(function, &current);
    if(nullp(current)){
        ERROR("NOTEVERY: %s","needs at least one list");}
    pred_ctx c = { 0, 0 };
    map_walk(fn, current, every_visit, &c);
    results->count = 1;
    results->values[0] = c.found_false ? T_Symbol : NIL_Symbol;
    close_live_pool(pool);}

static void kernel_NOTANY(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref fn = pop_mandatory_argument(function, &current);
    if(nullp(current)){
        ERROR("NOTANY: %s","needs at least one list");}
    some_ctx c = { NIL_Symbol, 0 };
    map_walk(fn, current, some_visit, &c);
    results->count = 1;
    results->values[0] = c.captured ? NIL_Symbol : T_Symbol;
    close_live_pool(pool);}

/* (IDENTITY x) — returns x. Trivial but ubiquitous as a TEST/KEY default. */
static void kernel_IDENTITY(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(arguments);
    Ref current = arguments;
    Ref x = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = x;
    close_live_pool(pool);}

/* (MAKE-STRING n &key initial-element) — fresh string of length n.
   initial-element defaults to a space; only the keyword form is honoured
   here (the standard's :element-type is accepted and ignored). */
static void kernel_MAKE_STRING(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref len_ref = pop_mandatory_argument(function, &current);
    Ref initial = pop_key_argument(function, &current, K(INITIAL-ELEMENT), NIL_Symbol);
    /* :element-type is accepted but ignored. */
    pop_key_argument(function, &current, K(ELEMENT-TYPE), NIL_Symbol);
    word len = integer_value(len_ref);
    if(len < 0){ ERROR("MAKE-STRING: negative length %ld",(long)len); }
    Ref s = string_new((halfword)len);
    /* Fill with INITIAL-ELEMENT if supplied; otherwise default to SPACE
       so freshly-minted strings don't contain uninitialized memory. */
    octet code = ' ';
    if(characterp(initial)){
        code = (octet)integer_value(character_code(initial));}
    {   octet* bytes = string_octets(s);
        for(word i = 0; i < len; i++){
            bytes[i] = code;}}
    results->count = 1;
    results->values[0] = s;
    close_live_pool(pool);}

/* (CHAR-SET string index char) — mutate string[index] to char. The kernel
   has the underlying char_set with C signature (new_char, string, index)
   — we adapt to the standard CL (setf (char s i) c) arg order here. */
static void kernel_CHAR_SET(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref str   = pop_mandatory_argument(function, &current);
    Ref index = pop_mandatory_argument(function, &current);
    Ref ch    = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    if(!stringp(str)){ TYPE_ERROR(str, STRING_Class); }
    if(!characterp(ch)){ TYPE_ERROR(ch, CHARACTER_Class); }
    word i = integer_value(index);
    word len = (word)string_length(str);
    if(i < 0 || i >= len){
        ERROR("CHAR-SET: index %ld out of range 0..%ld",(long)i,(long)(len-1));}
    octet* bytes = string_octets(str);
    bytes[i] = (octet)integer_value(character_code(ch));
    results->count = 1;
    results->values[0] = ch;
    close_live_pool(pool);}

/* (GENSYM &optional x) — fresh uninterned symbol named PREFIX followed by a
   number.  X is either a STRING prefix (default "G"), using and incrementing
   *GENSYM-COUNTER*, or a non-negative INTEGER used directly as the suffix, leaving
   *GENSYM-COUNTER* unchanged (CLHS GENSYM).  Implemented in C — a Lisp re-impl
   allocates enough per call to wake a latent GC gap during the prelude load. */
static void kernel_GENSYM(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(arguments);
    Ref current = arguments;
    Ref arg = nullp(current) ? NIL_Symbol : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    add_to_live_pool(arg);

    char        counter_buf[32];
    const char* prefix_c     = "G";
    char*       prefix_owned = NULL;

    if(integerp(arg)){
        /* integer designator: use directly, do not touch *GENSYM-COUNTER*. */
        snprintf(counter_buf, sizeof(counter_buf), "%ld", (long)integer_value(arg));
    } else {
        if(!nullp(arg)){
            if(!stringp(arg)){ TYPE_ERROR(arg, STRING_Class); }
            prefix_owned = string_cstring(arg);
            prefix_c     = prefix_owned;}
        Ref counter_sym = S(*GENSYM-COUNTER*);
        Ref cv = symbol_value(counter_sym);
        word n = integerp(cv) ? integer_value(cv) : 0;
        snprintf(counter_buf, sizeof(counter_buf), "%ld", (long)n);
        set_symbol_value(counter_sym, integer_from_word(n + 1));}

    size_t plen = strlen(prefix_c);
    size_t clen = strlen(counter_buf);
    char* full = malloc(plen + clen + 1);
    memcpy(full, prefix_c, plen);
    memcpy(full + plen, counter_buf, clen);
    full[plen + clen] = '\0';
    Ref sym = symbol_new(string_new_cstring(full));
    free(full);
    if(prefix_owned){ free(prefix_owned); }
    results->count = 1;
    results->values[0] = sym;
    close_live_pool(pool);}

static void register_fn(Ref fname, Ref lambda_list, KernelFunctionPtr fn){
    set_symbol_function(fname, built_in_function_new(fname, lambda_list, fn));}

/* (MAKE-BIT-VECTOR n &optional initial-element) — create a fresh bit-vector. */
static void kernel_MAKE_BIT_VECTOR(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref len_ref = pop_mandatory_argument(function, &current);
    Ref init    = nullp(current) ? I(0) : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    word len = integer_value(len_ref);
    if(len < 0){ ERROR("MAKE-BIT-VECTOR: negative length %ld",(long)len); }
    octet bit  = (octet)(integerp(init) ? (integer_value(init) & 1) : 0);
    octet* buf = (octet*)malloc((uword)len + 1);
    for(word i = 0; i < len; i++) buf[i] = bit;
    buf[len] = 0;
    Ref bv = bit_vector_new_octets((uword)len, buf);
    free(buf);
    results->count = 1;
    results->values[0] = bv;
    close_live_pool(pool);}

void kernel_initialize_sequences(void){
    Ref ll_fn_rest_args  = list(S(FUNCTION), S(&REST), S(ARGS),  NULL);
    Ref ll_fn_rest_lists = list(S(FUNCTION), S(&REST), S(LISTS), NULL);

    register_fn(S(FUNCALL),  ll_fn_rest_args,  kernel_FUNCALL);
    register_fn(S(APPLY),    ll_fn_rest_args,  kernel_APPLY);
    register_fn(S(MAPCAR),   ll_fn_rest_lists, kernel_MAPCAR);
    register_fn(S(MAPC),     ll_fn_rest_lists, kernel_MAPC);
    register_fn(S(EVERY),    ll_fn_rest_lists, kernel_EVERY);
    register_fn(S(SOME),     ll_fn_rest_lists, kernel_SOME);
    register_fn(S(NOTEVERY), ll_fn_rest_lists, kernel_NOTEVERY);
    register_fn(S(NOTANY),   ll_fn_rest_lists, kernel_NOTANY);
    register_fn(S(IDENTITY), list(S(X), NULL), kernel_IDENTITY);
    register_fn(S(GENSYM),   NIL_Symbol,       kernel_GENSYM);
    define_variable(S(*GENSYM-COUNTER*), I(0));   /* GENSYM's counter, CLHS */

    /* String-library kernel primitives — see prelude.lisp for the Lisp
       layer that sits on top of these. */
    register_fn(S(MAKE-STRING),
                list(S(LENGTH),
                     S(&KEY), S(INITIAL-ELEMENT), S(ELEMENT-TYPE),
                     NULL),
                kernel_MAKE_STRING);
    register_fn(S(CHAR-SET),
                list(S(STRING), S(INDEX), S(CHARACTER), NULL),
                kernel_CHAR_SET);
    register_fn(symbol_intern_cstring("MAKE-BIT-VECTOR"),
                list(symbol_intern_cstring("LENGTH"),
                     S(&OPTIONAL), symbol_intern_cstring("INITIAL-ELEMENT"),
                     NULL),
                kernel_MAKE_BIT_VECTOR);
}
