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

/* Walk env's frames looking for the first HANDLER-CASE clause whose
   type matches the condition's class. Returns the frame + selects the
   clause+condition into its handler slot; returns NULL if no match. */
static Frame* find_matching_handler(Ref cond, Ref environment){
    if(!typep(environment, ENVIRONMENT_Class)){ return NULL; }
    Ref cls = class_of(cond);
    Frame* frame = environment->slots->environment.frames;
    while(frame){
        if(frame->type == frame_handler){
            Ref clauses = frame->parameters.handler.clauses;
            while(consp(clauses)){
                Ref clause = cons_car(clauses);
                Ref type_class = car(clause);
                if(subclassp(cls, type_class)){
                    frame->parameters.handler.matched_clause = clause;
                    frame->parameters.handler.condition = cond;
                    return frame;}
                clauses = cons_cdr(clauses);}}
        frame = frame->next;}
    return NULL;}

/* Walk up from the current top frame to the target handler frame,
   signalling target on the way. We don't actually need the same plumbing
   as RETURN-FROM because intermediate frames (e.g. UNWIND-PROTECT) will
   re-enter via their own setjmp; we just longjmp to the topmost frame
   and let the UNWIND state machine chain to next. */
Ref signal_condition(Ref cond, Ref environment){
    Frame* target = find_matching_handler(cond, environment);
    if(target == NULL){
        /* No handler — per CL, SIGNAL returns NIL when nothing handles. */
        return NIL_Symbol;}
    /* Reuse the standard NON_LOCAL_EXIT machinery: set the top frame's
       target to `target`, longjmp into it. The handler frame's setjmp
       returns non_local_exit_target / _unwind appropriately. */
    Frame* top = environment->slots->environment.frames;
    if(top == NULL){
        FATAL("signal_condition: %s","no frames in env yet a handler matched");}
    top->target = target;
    int code = (top == target) ? 2 /* target */ : 1 /* unwind */;
    longjmp(top->non_local_exit, code);}

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
    if(typep(datum, STRING_Class)){
        return make_simple_condition(default_class, datum, rest);}
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
    Condition* c = &cond->slots->condition;
    char* fmt = typep(c->format_control, STRING_Class)
        ? string_cstring(c->format_control)
        : (char*)"unhandled condition";
    ERROR("Unhandled %s: %s",
          string_cstring(symbol_name(class_name(class_of(cond)))),
          fmt);
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

static void register_fn(Ref fname, Ref lambda_list, KernelFunctionPtr fn){
    set_symbol_function(fname, built_in_function_new(fname, lambda_list, fn));}

void kernel_initialize_conditions(void){
    Ref ll_datum = list(S(DATUM), NULL);
    Ref ll_obj   = list(S(OBJECT), NULL);
    Ref ll_cls   = list(S(CLASS-OR-NAME), NULL);
    Ref ll_c     = list(S(CONDITION), NULL);

    register_fn(S(SIGNAL),                            ll_datum, kernel_SIGNAL);
    register_fn(S(ERROR),                             ll_datum, kernel_ERROR);
    register_fn(S(WARN),                              ll_datum, kernel_WARN);
    register_fn(S(MAKE-CONDITION),                    ll_cls,   kernel_MAKE_CONDITION);
    register_fn(S(CONDITIONP),                        ll_obj,   kernel_CONDITIONP);
    register_fn(S(SIMPLE-CONDITION-FORMAT-CONTROL),   ll_c,     kernel_SIMPLE_CONDITION_FORMAT_CONTROL);
    register_fn(S(SIMPLE-CONDITION-FORMAT-ARGUMENTS), ll_c,     kernel_SIMPLE_CONDITION_FORMAT_ARGUMENTS);
}
