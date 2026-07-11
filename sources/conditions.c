#include <setjmp.h>
#include "conditions.h"
#include "kernel.h"
#include "cons.h"
#include "environment.h"
#include "variable.h"
#include "stream.h"
#include "printer.h"
#include "bytecode.h"    /* vm_call_frame_bindings — for frame_vm_call backtrace frames */
#include "eval.h"        /* frame_top_live — skip byte-code VM markers when unwinding */

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
                    /* Deactivate the WHOLE cluster while the handler runs so a
                       re-signal of the same condition from inside the handler skips
                       every binding of this HANDLER-BIND (CLHS 9.1.4.1) and reaches
                       only enclosing handlers. */
                    frame->type = frame_handler_bind_inactive;
                    MultipleValues hresults;
                    kernel_apply(handler_fn, list(cond, NULL), &hresults);
                    frame->type = frame_handler_bind; /* restore */
                    (void)hresults;
                    /* Handler declined (returned normally) — keep searching with the
                       NEXT binding in THIS same cluster (left-to-right), then the
                       enclosing frames.  (Do NOT skip to the next frame: a single
                       HANDLER-BIND may bind the same type more than once, and each
                       such handler must run in turn.) */
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
                    Frame* top = frame_top_live();   /* skip byte-code VM markers (no setjmp) */
                    if(top == NULL){
                        FATAL("signal_condition: %s",
                              "no frames yet a handler matched");}
                    top->target = frame;
                    int code = (top == frame) ? 2 : 1;
                    longjmp(top->non_local_exit, code);}
                clauses = cons_cdr(clauses);}}

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

/* eval_global_environment / frame_top_live are declared in eval.h (included above). */

static Ref build_condition(Ref default_class, Ref arguments){
    if(!consp(arguments)){
        ERROR("ERROR / SIGNAL / WARN: missing %s","datum");}
    Ref datum = cons_car(arguments);
    Ref rest  = cons_cdr(arguments);
    if(typep(datum, CONDITION_Class)){
        /* Supplying initargs together with an already-built condition is a
           type-error (SBCL/CCL signal SIMPLE-TYPE-ERROR here); must-condition
           form 24 catches it via a (type-error () t) clause. */
        if(consp(rest)){
            TYPE_ERROR(datum, CONDITION_Class);}
        return datum;}
    /* Accept CLOS instances (from DEFINE-CONDITION) as conditions directly. */
    if(clos_instance_p(datum)){
        if(consp(rest)){
            TYPE_ERROR(datum, CONDITION_Class);}
        return datum;}
    if(typep(datum, STRING_Class)){
        return make_simple_condition(default_class, datum, rest);}
    /* Accept a condition TYPE designator (a symbol naming a class) followed by
     * initargs: (error 'simple-error :format-control "…") or (error 'my-error
     * :msg "oops").  Route through MAKE-CONDITION (not MAKE-INSTANCE directly) so
     * built-in SIMPLE conditions go to the kernel constructor and REPORT (their
     * CLOS classes are slotless) while user DEFINE-CONDITION classes still get a
     * MAKE-INSTANCE with their own slots.  The result may be a kernel CONDITION
     * or a CLOS instance — accept either.  See issue make-condition-clos-initargs. */
    if(symbolp(datum)){
        Ref lisp_make = symbol_function(symbol_intern_cstring("MAKE-CONDITION"));
        if(lisp_make != NULL && lisp_make != UNBOUND_Object){
            MultipleValues mi_res = {0};
            kernel_apply(lisp_make, cons(datum, rest), &mi_res);
            if(mi_res.count > 0 && mi_res.values[0] != NULL &&
               (typep(mi_res.values[0], CONDITION_Class) ||
                clos_instance_p(mi_res.values[0]))){
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
    /* class_of(cond) is a CLOS %cls for make-condition'd / CLOS-defined
       conditions (e.g. the reader's END-OF-FILE / READER-ERROR); class_name
       would check_type-reject it.  class_designator_name accepts both. */
    ERROR("Unhandled %s: %s",
          string_cstring(symbol_name(class_designator_name(class_of(cond)))),
          report);
    /* unreachable */
    (void)results;
    close_live_pool(pool);}

static void kernel_WARN(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref cond = build_condition(SIMPLE_WARNING_Class, arguments);
    /* CLHS WARN: if the condition is not of type WARNING, signal a TYPE-ERROR and
       do NOT signal the warning. */
    if(!typep(cond, WARNING_Class)){
        TYPE_ERROR(cond, WARNING_Class);}
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
    /* find_class returns C NULL (not NIL) when the symbol names no class — guard
       BEFORE typep, which would dereference the NULL.  A type designator that does
       not name a condition class is a TYPE-ERROR (CLHS make-condition), and the
       must-condition suite catches exactly that ((error 'no-such-type) inside a
       (type-error () t) handler-case). */
    if(cls == NULL || !typep(cls, T_Class)){
        TYPE_ERROR(class_or_name, CONDITION_Class);}
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

/* A restart with a :TEST-FUNCTION is applicable only when that function, called on
   the condition (NIL if none), returns true (CLHS).  Only RESTART-BIND stores a
   test (spec = (name fn interactive-fn test-fn)); a RESTART-CASE spec's 2nd element
   is a lambda-list, not callable, and carries no test — always applicable. */
static bool restart_applicable(Ref spec, Ref condition){
    if(!consp(spec) || !consp(cdr(spec))){ return true; }
    Ref second = cadr(spec);
    bool bind_kind = functionp(second)||closurep(second)||built_in_function_p(second);
    if(!bind_kind){ return true; }
    Ref test_fn = NIL_Symbol;
    Ref p = cddr(spec);                              /* (interactive-fn test-fn) */
    if(consp(p) && consp(cdr(p))){ test_fn = cadr(p); }
    if(functionp(test_fn)||closurep(test_fn)||built_in_function_p(test_fn)){
        MultipleValues tr = {0};
        kernel_apply(test_fn, cons(condition, NIL_Symbol), &tr);
        return tr.count > 0 && !nullp(tr.values[0]);}
    return true;}

/* WITH-CONDITION-RESTARTS association (CLHS 9.1.4.2.4): if NAME appears in the
   dynamic *%RESTART-ASSOCIATIONS* list (entries (condition . restart-names)), the
   restart is found only when CONDITION is one of the conditions it was associated
   with.  Returns true when NAME is associated but NOT with CONDITION → hidden. */
static bool restart_name_hidden(Ref name, Ref condition){
    static Ref assoc_sym = NULL;
    if(assoc_sym == NULL){ assoc_sym = symbol_intern_cstring("*%RESTART-ASSOCIATIONS*"); }
    if(!symbol_boundp(assoc_sym)){ return false; }
    bool appears = false, matches = false;
    for(Ref a = symbol_value(assoc_sym); consp(a); a = cdr(a)){
        Ref entry = car(a);
        if(!consp(entry)){ continue; }
        Ref acond = car(entry);
        for(Ref ns = cdr(entry); consp(ns); ns = cdr(ns)){
            if(car(ns) == name){
                appears = true;
                if(acond == condition){ matches = true; }
                break;}}}
    return appears && !matches;}

/* (FIND-RESTART identifier &optional condition) → restart-designator or NIL.
   In bocl's minimal impl the "restart object" is just the name symbol.  A restart
   whose :TEST-FUNCTION declines (on CONDITION) is skipped, as is one associated
   (via WITH-CONDITION-RESTARTS) with a different condition. */
static void kernel_FIND_RESTART(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref name = pop_mandatory_argument(function, &current);
    Ref condition = consp(current) ? car(current) : NIL_Symbol;
    /* Accept either a symbol or a restart object (which is also the symbol). */
    if(!symbolp(name)){
        ERROR("FIND-RESTART: expected a symbol, got %s",
              string_cstring(prin1_to_string(name)));}
    if(restart_name_hidden(name, condition)){
        results->count = 1;
        results->values[0] = NIL_Symbol;
        close_live_pool(pool);
        return;}
    Ref found = NIL_Symbol;
    for(Frame* frame=g_frame_stack; frame; frame=frame->next){
        if(frame->type == frame_restart){
            Ref rs = frame->parameters.restart.restarts;
            while(consp(rs)){
                Ref spec = car(rs);
                if(consp(spec) && car(spec)==name && restart_applicable(spec, condition)){
                    found = name; goto done;}
                rs = cdr(rs);}}}
  done:
    results->count = 1;
    results->values[0] = found;
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
    Ref name = designator;   /* only a symbol restart designator is supported */
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
    Frame* top = frame_top_live();   /* skip byte-code VM markers (no setjmp) */
    if(top == NULL){
        FATAL("INVOKE-RESTART: %s","no active frame");}
    top->target = target;
    int code = (top == target) ? (int)non_local_exit_target : (int)non_local_exit_unwind;
    longjmp(top->non_local_exit, code);
    /* NOTREACHED */
    (void)results;
    close_live_pool(pool);}

/* (INVOKE-RESTART-INTERACTIVELY restart-designator) — invoke the named restart,
   computing its arguments via the restart's :interactive-function (a restart-bind
   restart stores it as the 3rd element of its spec).  With no interactive
   function, invoke with no args. */
static void kernel_INVOKE_RESTART_INTERACTIVELY(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref name = pop_mandatory_argument(function, &current);
    if(!symbolp(name)){
        ERROR("INVOKE-RESTART-INTERACTIVELY: expected a symbol, got %s",
              string_cstring(prin1_to_string(name)));}
    Frame* target = find_restart_frame_by_name(name, eval_global_environment());
    if(target == NULL){
        ERROR("INVOKE-RESTART-INTERACTIVELY: no active restart named %s",
              string_cstring(prin1_to_string(name)));}
    /* Find the restart spec; (name fn interactive-fn) for restart-bind. */
    Ref spec = NIL_Symbol;
    {   Ref rs = target->parameters.restart.restarts;
        while(consp(rs)){ if(consp(car(rs))&&car(car(rs))==name){ spec=car(rs); break; } rs=cdr(rs); }}
    /* The interactive function lives at a different position by spec kind:
       RESTART-BIND spec = (name fn interactive-fn)         -> 3rd element;
       RESTART-CASE spec = (name varlist body interactive)  -> 4th element.
       Tell them apart by whether the 2nd element is callable. */
    Ref ifn = NIL_Symbol;
    if(consp(spec) && consp(cdr(spec))){
        Ref second = cadr(spec);
        bool bind_kind = functionp(second)||closurep(second)||built_in_function_p(second);
        if(bind_kind){
            ifn = consp(cddr(spec)) ? caddr(spec) : NIL_Symbol;
        }else if(consp(cddr(spec)) && consp(cdddr(spec))){
            ifn = car(cdddr(spec));
        }}
    Ref args = NIL_Symbol;
    if(functionp(ifn) || closurep(ifn) || built_in_function_p(ifn)){
        MultipleValues ir = {0};
        kernel_apply(ifn, NIL_Symbol, &ir);
        args = (ir.count > 0) ? ir.values[0] : NIL_Symbol;}
    /* Delegate to INVOKE-RESTART with (name . args). */
    kernel_INVOKE_RESTART(function, cons(name, args), results);
    close_live_pool(pool);}

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
        /* CLHS: ABORT with no applicable abort restart signals a CONTROL-ERROR
           (a recoverable condition), not a process abort. */
        Ref cond = make_simple_condition(CONTROL_ERROR_Class,
                                         T("ABORT: no active ABORT restart"),
                                         NIL_Symbol);
        add_to_live_pool(cond);
        signal_condition(cond, env);
        /* No handler caught it — surface as an ordinary error, never FATAL. */
        ERROR("ABORT: no active ABORT restart%s", "");}
    target->parameters.restart.matched_name = S(ABORT);
    target->parameters.restart.matched_args = NIL_Symbol;
    Frame* top = frame_top_live();   /* skip byte-code VM markers (no setjmp) */
    if(top == NULL){ FATAL("ABORT: %s","no active frame"); }
    top->target = target;
    int code = (top == target) ? (int)non_local_exit_target : (int)non_local_exit_unwind;
    longjmp(top->non_local_exit, code);
    (void)results;
    close_live_pool(pool);}

/* (%BACKTRACE) → list of control-frame descriptors, innermost (top of stack)
   first.  Each descriptor is a list (kind . data):
     (:unwind-protect) (:dynamic-binding)
     (:block name) (:catch tag) (:tagbody)
     (:handler-case) (:handler-bind) (:restart name...)
   bocl has no first-class function-call stack, so this exposes the active
   dynamic CONTROL frames (handler / restart / block / catch / unwind-protect /
   dynamic-binding), which is what the kernel can report today.  The Lisp wrappers
   BOCL:BACKTRACE / FRAME-KIND / FRAME-NAME / PRINT-FRAME build on this. */
static void kernel_BACKTRACE(unused Ref function, unused Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    Ref out = NIL_Symbol;
    for(Frame* f = g_frame_stack; f; f = f->next){
        Ref desc;
        switch(f->type){
          case frame_protect:
            desc = list(keyword_intern_cstring("UNWIND-PROTECT"), NULL); break;
          case frame_dynamic_binding:
            desc = list(keyword_intern_cstring("DYNAMIC-BINDING"), NULL); break;
          case frame_block:
            /* A function-body block carries the function + parameter environment:
               report it as a :CALL frame (name . ((param . value) ...)) so the
               debugger can show parameters at verbosity 1/2.  An explicit BLOCK form
               (function = NIL) stays a plain (:block name). */
            if(!nullp(f->parameters.block.function)){
                Ref binds = environment_local_bindings(f->parameters.block.environment);
                add_to_live_pool(binds);
                desc = cons(keyword_intern_cstring("CALL"),
                            cons(f->parameters.block.name, binds));
            }else{
                desc = list(keyword_intern_cstring("BLOCK"), f->parameters.block.name, NULL);
            }
            break;
          case frame_vm_call: {
            /* A byte-code VM activation (vm-9): report it as a :CALL frame exactly like a
               tree-walked one — (:CALL name (param . value) ...).  block.function is the
               CompiledFunction and block.environment is the activation's LOCALS vector;
               vm_call_frame_bindings (bytecode.c) reconstructs the parameter alist from it
               (so fast-apply need not cons an arg list per call just for a maybe-backtrace). */
            Ref binds = vm_call_frame_bindings(f->parameters.block.function,
                                               f->parameters.block.environment);
            add_to_live_pool(binds);
            desc = cons(keyword_intern_cstring("CALL"), cons(f->parameters.block.name, binds));
            break; }
          case frame_catch:
            desc = list(keyword_intern_cstring("CATCH"), f->parameters.catch.object, NULL); break;
          case frame_tagbody:
            desc = list(keyword_intern_cstring("TAGBODY"), NULL); break;
          case frame_handler:
            desc = list(keyword_intern_cstring("HANDLER-CASE"), NULL); break;
          case frame_handler_bind:
          case frame_handler_bind_inactive:
            desc = list(keyword_intern_cstring("HANDLER-BIND"), NULL); break;
          case frame_restart: {
            /* (:restart name1 name2 ...) — names of the frame's restarts. */
            Ref names = NIL_Symbol;
            for(Ref rs = f->parameters.restart.restarts; consp(rs); rs = cdr(rs)){
                push(car(car(rs)), &names);
                add_to_live_pool(names);}
            desc = cons(keyword_intern_cstring("RESTART"), nreverse(names));
            break;}
          default:
            desc = list(keyword_intern_cstring("FRAME"), NULL); break;}
        add_to_live_pool(desc);
        push(desc, &out);
        add_to_live_pool(out);}
    out = nreverse(out);
    close_live_pool(pool);
    add_to_live_pool(out);
    results->count = 1;
    results->values[0] = out;}

static void register_fn(Ref fname, Ref lambda_list, KernelFunctionPtr fn){
    set_symbol_function(fname, built_in_function_new(fname, lambda_list, fn));}

void kernel_initialize_conditions(void){
    Ref ll_datum   = list(S(DATUM), NULL);
    Ref ll_obj     = list(S(OBJECT), NULL);
    Ref ll_cls     = list(S(CLASS-OR-NAME), NULL);
    Ref ll_c       = list(S(CONDITION), NULL);
    Ref ll_restart = list(S(RESTART-DESIGNATOR), NULL);
    Ref ll_find_restart = list(S(RESTART-DESIGNATOR), S(&OPTIONAL), S(CONDITION), NULL);
    Ref ll_empty   = NIL_Symbol;

    register_fn(S(SIGNAL),                            ll_datum,   kernel_SIGNAL);
    register_fn(S(ERROR),                             ll_datum,   kernel_ERROR);
    register_fn(S(WARN),                              ll_datum,   kernel_WARN);
    register_fn(S(MAKE-CONDITION),                    ll_cls,     kernel_MAKE_CONDITION);
    register_fn(S(CONDITIONP),                        ll_obj,     kernel_CONDITIONP);
    register_fn(S(SIMPLE-CONDITION-FORMAT-CONTROL),   ll_c,       kernel_SIMPLE_CONDITION_FORMAT_CONTROL);
    register_fn(S(SIMPLE-CONDITION-FORMAT-ARGUMENTS), ll_c,       kernel_SIMPLE_CONDITION_FORMAT_ARGUMENTS);
    register_fn(S(FIND-RESTART),                      ll_find_restart, kernel_FIND_RESTART);
    register_fn(S(INVOKE-RESTART),                    ll_restart, kernel_INVOKE_RESTART);
    register_fn(S(INVOKE-RESTART-INTERACTIVELY),      ll_restart, kernel_INVOKE_RESTART_INTERACTIVELY);
    register_fn(S(COMPUTE-RESTARTS),                  list(S(&OPTIONAL),S(CONDITION),NULL), kernel_COMPUTE_RESTARTS);
    register_fn(S(RESTART-NAME),                      list(S(RESTART),NULL), kernel_RESTART_NAME);
    register_fn(symbol_intern_cstring("%BACKTRACE"),  ll_empty,   kernel_BACKTRACE);
    register_fn(S(ABORT),                             ll_empty,   kernel_ABORT);
}
