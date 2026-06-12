#include <setjmp.h>
#include "conditions.h"
#include "kernel.h"
#include "cons.h"
#include "environment.h"
#include "variable.h"
#include "stream.h"
#include "printer.h"

extern Ref Object_new(Ref class, Slots* slots);

/* ------------------------------------------------------------------ */
/*  Constructor                                                          */
/* ------------------------------------------------------------------ */

static Condition* Condition_slots(Ref format_control, Ref format_arguments){
    Condition* that = allocate(sizeof(*that));
    that->objcount = 2;
    that->format_control   = format_control;
    that->format_arguments = format_arguments;
    return that;}

Ref make_simple_condition(Ref class, Ref format_control, Ref format_arguments){
    check_type(class, T_Class);  /* any class */
    return Object_new(class, (Slots*)Condition_slots(format_control, format_arguments));}

/* ------------------------------------------------------------------ */
/*  Signal mechanism                                                     */
/* ------------------------------------------------------------------ */

/* Walk the full environment chain looking for handlers.
   For frame_handler (HANDLER-CASE): set matched_clause+condition, longjmp.
   For frame_handler_bind (HANDLER-BIND): call the handler function in place
   (no unwind); if it returns normally, continue the search.
   Returns NIL_Symbol if no HANDLER-CASE catches the condition (per CL SIGNAL
   semantics — SIGNAL returns NIL when unhandled). */
extern Frame* g_frame_stack;   /* the global dynamic control-frame stack (eval.c) */

Ref signal_condition(Ref cond, unused Ref environment){
    Ref cls = class_of(cond);

    /* Walk the global dynamic frame stack (innermost first), so handlers
       established by any dynamically-enclosing caller are visible — including
       across function-call boundaries. */
    Frame* frame = g_frame_stack;
    while(frame){
        /* ---- HANDLER-BIND ---- */
        if(frame->type == frame_handler_bind){
            Ref bindings = frame->parameters.handler_bind.bindings;
            while(consp(bindings)){
                Ref pair = car(bindings);
                Ref type_class = car(pair);
                Ref handler_fn  = cdr(pair);
                if(subclassp(cls, type_class)){
                    /* Deactivate this frame while the handler runs so
                       re-signaling the same condition from the handler
                       skips this binding (CL spec requirement). */
                    frame->type = frame_handler_bind_inactive;
                    MultipleValues hresults;
                    kernel_apply(handler_fn, list(cond, NULL), &hresults);
                    frame->type = frame_handler_bind; /* restore */
                    /* Handler returned normally — continue signaling. */
                    goto next_frame;
                }
                bindings = cdr(bindings);}}

        /* ---- HANDLER-CASE ---- */
        else if(frame->type == frame_handler){
            Ref clauses = frame->parameters.handler.clauses;
            while(consp(clauses)){
                Ref clause = cons_car(clauses);
                Ref type_class = car(clause);
                if(subclassp(cls, type_class)){
                    frame->parameters.handler.matched_clause = clause;
                    frame->parameters.handler.condition = cond;
                    /* Unwind toward this handler through every intervening
                       frame (running their cleanups) via the UNWIND machinery:
                       longjmp into the current top frame, chaining to target. */
                    Frame* top = g_frame_stack;
                    if(top == NULL){
                        FATAL("signal_condition: %s",
                              "no frames yet a handler matched");}
                    top->target = frame;
                    int code = (top == frame) ? 2 : 1;
                    longjmp(top->non_local_exit, code);}
                clauses = cons_cdr(clauses);}}

        next_frame:
        frame = frame->next;}

    /* No handler caught the condition. */
    return NIL_Symbol;}

/* ------------------------------------------------------------------ */
/*  Kernel built-ins                                                     */
/*                                                                       */
/*  ERROR / SIGNAL / WARN all accept (datum &rest args). If datum is a   */
/*  STRING, build a SIMPLE-ERROR / SIMPLE-WARNING. If datum is already   */
/*  a CONDITION, use it directly. Other forms (a class symbol with      */
/*  initargs) wait for the macro system.                                */
/* ------------------------------------------------------------------ */

extern Ref eval_global_environment(void);

static Ref build_condition(Ref default_class, Ref arguments){
    if(!consp(arguments)){
        ERROR("ERROR / SIGNAL / WARN: missing %s","datum");}
    Ref datum = cons_car(arguments);
    Ref rest  = cons_cdr(arguments);
    if(typep(datum, CONDITION_Class)){
        if(consp(rest)){
            ERROR("Extra arguments after %s","an already-built condition");}
        return datum;}
    /* Accept CLOS instances (from DEFINE-CONDITION) as conditions directly. */
    if(clos_instance_p(datum)){
        if(consp(rest)){
            ERROR("Extra arguments after %s","a CLOS condition instance");}
        return datum;}
    if(typep(datum, STRING_Class)){
        return make_simple_condition(default_class, datum, rest);}
    /* Also accept a CLOS class designator (symbol naming a DEFINE-CONDITION class)
     * followed by initargs: (error 'my-error :msg "oops"). */
    if(symbolp(datum)){
        /* Try to make a condition instance from the class name + initargs. */
        Ref lisp_make = symbol_function(symbol_intern_cstring("MAKE-INSTANCE"));
        if(lisp_make != NULL && lisp_make != UNBOUND_Object){
            MultipleValues mi_res = {0};
            kernel_apply(lisp_make, cons(datum, rest), &mi_res);
            if(mi_res.count > 0 && mi_res.values[0] != NULL &&
               clos_instance_p(mi_res.values[0])){
                return mi_res.values[0];}}}
    ERROR("Unsupported datum type to ERROR / SIGNAL / WARN: %s",
          string_cstring(prin1_to_string(datum)));}

static void kernel_SIGNAL(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref cond = build_condition(SIMPLE_CONDITION_Class, arguments);
    Ref ret = signal_condition(cond, eval_global_environment());
    results->count = 1;
    results->values[0] = ret;
    close_live_pool(pool);}

static void kernel_ERROR(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref cond = build_condition(SIMPLE_ERROR_Class, arguments);
    Ref ret = signal_condition(cond, eval_global_environment());
    (void)ret;
    /* If signal_condition returned, no handler matched — invoke the
       unhandled-error fallback. For now: print the message and exit
       via the C error() path. */
    /* Render the condition's REPORT (format-control applied to its
       format-arguments) via PRINC, not the raw format-control — otherwise an
       unhandled "LOAD: file ~S does not exist" prints the literal ~S. */
    char* report = string_cstring(princ_to_string(cond));
    ERROR("Unhandled %s: %s",
          string_cstring(symbol_name(class_name(class_of(cond)))),
          report);
    /* unreachable */
    (void)results;
    close_live_pool(pool);}

static void kernel_WARN(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref cond = build_condition(SIMPLE_WARNING_Class, arguments);
    Ref ret = signal_condition(cond, eval_global_environment());
    if(nullp(ret)){
        /* No handler — print the warning to *error-output*. */
        Ref err = variable_value(S(*ERROR-OUTPUT*));
        princ(T(";; WARNING: "), err);
        Condition* c = &cond->slots->condition;
        if(typep(c->format_control, STRING_Class)){
            princ(c->format_control, err);
        }
        terpri(err);
        stream_finish_output(err);}
    results->count = 1;
    results->values[0] = NIL_Symbol;
    close_live_pool(pool);}

/* (MAKE-CONDITION class &rest initargs) — minimal: only :FORMAT-CONTROL
   and :FORMAT-ARGUMENTS initargs honoured. Anything else is ignored. */
static void kernel_MAKE_CONDITION(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref class_or_name = pop_mandatory_argument(function, &current);
    /* Accept the class object, or a symbol naming a class. */
    Ref cls = symbolp(class_or_name) ? find_class(class_or_name) : class_or_name;
    if(!typep(cls, T_Class)){
        ERROR("MAKE-CONDITION: %s is not a class",
              string_cstring(prin1_to_string(class_or_name)));}
    Ref fmt  = pop_key_argument(function, &current, K(FORMAT-CONTROL),   NIL_Symbol);
    Ref args = pop_key_argument(function, &current, K(FORMAT-ARGUMENTS), NIL_Symbol);
    results->count = 1;
    results->values[0] = make_simple_condition(cls, fmt, args);
    close_live_pool(pool);}

/* (CONDITIONP obj) — convenience. */
static void kernel_CONDITIONP(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref obj = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = as_boolean(typep(obj, CONDITION_Class));
    close_live_pool(pool);}

/* (SIMPLE-CONDITION-FORMAT-CONTROL c) — accessor. */
static void kernel_SIMPLE_CONDITION_FORMAT_CONTROL(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref c = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    check_type(c, CONDITION_Class);
    results->count = 1;
    results->values[0] = c->slots->condition.format_control;
    close_live_pool(pool);}

static void kernel_SIMPLE_CONDITION_FORMAT_ARGUMENTS(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref c = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    check_type(c, CONDITION_Class);
    results->count = 1;
    results->values[0] = c->slots->condition.format_arguments;
    close_live_pool(pool);}

/* ------------------------------------------------------------------ */
/*  Restart system: FIND-RESTART / INVOKE-RESTART / ABORT             */
/* ------------------------------------------------------------------ */

/* Walk the full env chain looking for any frame_restart frame that
   has a restart named `name`.  Returns the frame, or NULL if not
   found.  Innermost (most recently established) match wins. */
static Frame* find_restart_frame_by_name(Ref name, unused Ref environment){
    for(Frame* frame=g_frame_stack; frame; frame=frame->next){
        if(frame->type == frame_restart){
            Ref restarts = frame->parameters.restart.restarts;
            while(consp(restarts)){
                if(car(car(restarts)) == name){ return frame; }
                restarts = cdr(restarts);}}}
    return NULL;}

/* (FIND-RESTART name) → restart-designator or NIL.
   In bocl's minimal impl the "restart object" is just the name symbol. */
static void kernel_FIND_RESTART(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref name = pop_mandatory_argument(function, &current);
    /* Accept either a symbol or a restart object (which is also the symbol). */
    if(!symbolp(name)){
        ERROR("FIND-RESTART: expected a symbol, got %s",
              string_cstring(prin1_to_string(name)));}
    Frame* rf = find_restart_frame_by_name(name, eval_global_environment());
    results->count = 1;
    results->values[0] = rf ? name : NIL_Symbol;
    close_live_pool(pool);}

/* (INVOKE-RESTART restart-designator &rest args)
   Finds the innermost active restart with the given name and performs
   a non-local exit to its RESTART-CASE frame with `args` bound.
   Never returns normally. */
static void kernel_INVOKE_RESTART(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref designator = pop_mandatory_argument(function, &current);
    Ref restart_args = current; /* remaining args passed to restart body */
    Ref name = symbolp(designator) ? designator : designator;
    if(!symbolp(name)){
        ERROR("INVOKE-RESTART: expected a symbol, got %s",
              string_cstring(prin1_to_string(designator)));}
    Ref env = eval_global_environment();
    Frame* target = find_restart_frame_by_name(name, env);
    if(target == NULL){
        ERROR("INVOKE-RESTART: no active restart named %s",
              string_cstring(prin1_to_string(name)));}
    /* RESTART-BIND restart: its spec's 2nd element is a callable — apply it in
       place (in the dynamic context of this INVOKE-RESTART) and return its values,
       rather than unwinding to a RESTART-CASE clause. */
    {   Ref rs = target->parameters.restart.restarts;
        while(consp(rs)){
            Ref spec = car(rs);
            if(consp(spec) && car(spec)==name){
                Ref fn = consp(cdr(spec)) ? cadr(spec) : NIL_Symbol;
                if(functionp(fn) || closurep(fn) || built_in_function_p(fn)){
                    kernel_apply(fn, restart_args, results);
                    close_live_pool(pool);
                    return;}
                break;}
            rs = cdr(rs);}}
    /* Set the matched restart on the frame (RESTART-CASE: unwind below). */
    target->parameters.restart.matched_name = name;
    target->parameters.restart.matched_args = restart_args;
    /* Trigger a non-local exit toward `target`: longjmp into the current top
       frame, letting the UNWIND chain propagate through intermediates. */
    Frame* top = g_frame_stack;
    if(top == NULL){
        FATAL("INVOKE-RESTART: %s","no active frame");}
    top->target = target;
    int code = (top == target) ? (int)non_local_exit_target : (int)non_local_exit_unwind;
    longjmp(top->non_local_exit, code);
    /* NOTREACHED */
    (void)results;
    close_live_pool(pool);}

/* (INVOKE-RESTART-INTERACTIVELY name) — same as invoke-restart with no args. */
static void kernel_INVOKE_RESTART_INTERACTIVELY(Ref function, Ref arguments, MultipleValues* results){
    /* Delegate: add NIL args list */
    kernel_INVOKE_RESTART(function, arguments, results);}

/* (COMPUTE-RESTARTS &optional condition) — the active restart objects, innermost
   first.  A restart object IS its name symbol in bocl; the optional condition is
   ignored (restarts are not condition-associated here). */
static void kernel_COMPUTE_RESTARTS(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(arguments);
    Ref acc = NIL_Symbol;
    for(Frame* frame=g_frame_stack; frame; frame=frame->next){
        if(frame->type == frame_restart){
            Ref rs = frame->parameters.restart.restarts;
            while(consp(rs)){ push(car(car(rs)), &acc); rs = cdr(rs); }}}
    results->count = 1;
    results->values[0] = nreverse(acc);
    close_live_pool(pool);}

/* (RESTART-NAME restart) — the name of a restart object (which is its name). */
static void kernel_RESTART_NAME(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref restart = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = restart;
    close_live_pool(pool);}

/* (ABORT) — invoke the innermost ABORT restart or fatal-error if none. */
static void kernel_ABORT(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref env = eval_global_environment();
    Frame* target = find_restart_frame_by_name(S(ABORT), env);
    if(target == NULL){
        FATAL("ABORT: no active %s restart", "ABORT");}
    target->parameters.restart.matched_name = S(ABORT);
    target->parameters.restart.matched_args = NIL_Symbol;
    Frame* top = g_frame_stack;
    if(top == NULL){ FATAL("ABORT: %s","no active frame"); }
    top->target = target;
    int code = (top == target) ? (int)non_local_exit_target : (int)non_local_exit_unwind;
    longjmp(top->non_local_exit, code);
    (void)results;
    close_live_pool(pool);}

static void register_fn(Ref fname, Ref lambda_list, KernelFunctionPtr fn){
    set_symbol_function(fname, built_in_function_new(fname, lambda_list, fn));}

void kernel_initialize_conditions(void){
    Ref ll_datum   = list(S(DATUM), NULL);
    Ref ll_obj     = list(S(OBJECT), NULL);
    Ref ll_cls     = list(S(CLASS-OR-NAME), NULL);
    Ref ll_c       = list(S(CONDITION), NULL);
    Ref ll_restart = list(S(RESTART-DESIGNATOR), NULL);
    Ref ll_empty   = NIL_Symbol;

    register_fn(S(SIGNAL),                            ll_datum,   kernel_SIGNAL);
    register_fn(S(ERROR),                             ll_datum,   kernel_ERROR);
    register_fn(S(WARN),                              ll_datum,   kernel_WARN);
    register_fn(S(MAKE-CONDITION),                    ll_cls,     kernel_MAKE_CONDITION);
    register_fn(S(CONDITIONP),                        ll_obj,     kernel_CONDITIONP);
    register_fn(S(SIMPLE-CONDITION-FORMAT-CONTROL),   ll_c,       kernel_SIMPLE_CONDITION_FORMAT_CONTROL);
    register_fn(S(SIMPLE-CONDITION-FORMAT-ARGUMENTS), ll_c,       kernel_SIMPLE_CONDITION_FORMAT_ARGUMENTS);
    register_fn(S(FIND-RESTART),                      ll_restart, kernel_FIND_RESTART);
    register_fn(S(INVOKE-RESTART),                    ll_restart, kernel_INVOKE_RESTART);
    register_fn(S(INVOKE-RESTART-INTERACTIVELY),      ll_restart, kernel_INVOKE_RESTART_INTERACTIVELY);
    register_fn(S(COMPUTE-RESTARTS),                  list(S(&OPTIONAL),S(CONDITION),NULL), kernel_COMPUTE_RESTARTS);
    register_fn(S(RESTART-NAME),                      list(S(RESTART),NULL), kernel_RESTART_NAME);
    register_fn(S(ABORT),                             ll_empty,   kernel_ABORT);
}
