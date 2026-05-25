#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel.h"
#include "cons.h"

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
        /* Build the tuple of cars and advance the lists. */
        Ref tuple = NIL_Symbol;
        {Ref out = NIL_Symbol;
         Ref it = lists;
         while(consp(it)){
             push(cons_car(cons_car(it)), &tuple);
             push(cons_cdr(cons_car(it)), &out);
             it = cons_cdr(it);}
         tuple = nreverse(tuple);
         out = nreverse(out);
         lists = out;}
        MultipleValues r = {0};
        kernel_apply(fn, tuple, &r);
        Ref result = (r.count > 0) ? r.values[0] : NIL_Symbol;
        visit(result, ctx);}}

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

/* (GENSYM &optional prefix) — return a fresh uninterned symbol whose name
   is `prefix` followed by an integer counter. Default prefix is "G". */
static word gensym_counter = 0;
static void kernel_GENSYM(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(arguments);
    Ref current = arguments;
    Ref prefix = nullp(current)
                 ? string_new_cstring("G")
                 : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    char* prefix_c = string_cstring(prefix);
    char counter_buf[32];
    snprintf(counter_buf, sizeof(counter_buf), "%ld", (long)(++gensym_counter));
    size_t plen = strlen(prefix_c);
    size_t clen = strlen(counter_buf);
    char* full = malloc(plen + clen + 1);
    memcpy(full, prefix_c, plen);
    memcpy(full + plen, counter_buf, clen);
    full[plen + clen] = '\0';
    Ref sym = symbol_new(string_new_cstring(full));
    free(full);
    free(prefix_c);
    results->count = 1;
    results->values[0] = sym;
    close_live_pool(pool);}

static void register_fn(Ref fname, Ref lambda_list, KernelFunctionPtr fn){
    set_symbol_function(fname, built_in_function_new(fname, lambda_list, fn));}

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
}
