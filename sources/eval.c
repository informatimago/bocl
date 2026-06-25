#include <setjmp.h>
#include "eval.h"
#include "kernel.h"
#include "boerror.h"
#include "cons.h"
#include "environment.h"
#include "variable.h"
#include "bocl.h"
#include "packages.h"

/* Forward decls for the apply helpers implemented later in this file. */
void kernel_apply_function(Ref function,Function* slots,Ref arguments,MultipleValues* results);
void kernel_apply_closure(Ref closure,Closure* slots,Ref arguments,MultipleValues* results);

/* The single lazily-allocated global lexical environment. Held by the GC rootset
   so it survives across bocl_eval calls. */
static Ref bocl_global_environment = NULL;

Ref eval_global_environment(void){
    if(bocl_global_environment==NULL){
        bocl_global_environment=environment_new(NIL_Symbol,S(T));
        push_to_rootset(bocl_global_environment);}
    return bocl_global_environment;}

/* The lexical environment of the macro call currently being expanded, so a macro
   lambda-list's &ENVIRONMENT parameter can be bound to it (and MACRO-FUNCTION can
   then find lexically-local macros there).  Set around the macro's application;
   it is the live eval `environment`, which is already GC-rooted by the caller. */
static Ref g_macroexpand_environment = NULL;
Ref current_macroexpand_environment(void){
    return g_macroexpand_environment ? g_macroexpand_environment : NIL_Symbol; }

/* non_local_exit_t is declared in kernel.h so conditions.c can use it. */


/* All control frames (BLOCK, CATCH, TAGBODY, HANDLER-CASE, HANDLER-BIND,
   UNWIND-PROTECT, dynamic bindings, RESTART-CASE) live on ONE global dynamic
   stack in LIFO order, rather than on per-environment lists.  This makes their
   lookup follow the DYNAMIC call chain, so CATCH/THROW and HANDLER-CASE /
   HANDLER-BIND work across function-call boundaries (essential for, e.g., a
   test runner that wraps each test in a handler).  Variable and function
   lookup are unaffected — they still walk the lexical environment chain.
   Frame structs are plain C stack objects whose Ref contents are GC-protected
   via the live pool, so keeping them on a global stack is GC-safe. */
Frame* g_frame_stack = NULL;

/* Forward decl: defined below, used by HANDLER-CASE's :no-error clause. */
static void bind_required_parameters(Ref function,Ref lambda_list,Ref arguments,Ref target_env);

/* Strip leading (declare ...) forms from an implicit-progn body (HANDLER-CASE /
   RESTART-CASE clause bodies may begin with declarations; bocl ignores their
   content but must not evaluate `declare` as a function). */
static Ref skip_declares(Ref body){
    while(consp(body) && consp(car(body)) && car(car(body))==S(DECLARE)){
        body=cdr(body);}
    return body;}

static void push_frame(Frame* frame,Ref environment){
    /* Global dynamic stack — used by CATCH/THROW, HANDLER-CASE/HANDLER-BIND and
       all non-local-exit unwinding, so those work across function-call
       boundaries (the dynamic extent). */
    frame->next=g_frame_stack;
    g_frame_stack=frame;
    /* Per-environment lexical list — used by BLOCK/TAGBODY lookup, which is
       lexically scoped and must stay O(lexical-depth) rather than walking the
       whole dynamic stack (otherwise deep nested loads crawl). */
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    frame->env_next=env->frames;
    env->frames=frame;}

static Frame* pop_frame(Frame* frame,Ref environment){
    Frame* result=g_frame_stack;
    if(result==NULL){
        FATAL("Frame stack is %s!","empty");}
    g_frame_stack=result->next;
    /* Unlink from the establishing environment's lexical list too (LIFO: the
       global top is also the top of its own environment's list). */
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    if(env->frames==result){
        env->frames=result->env_next;}
    if((frame!=NULL) && (frame!=result)){
        FATAL("Popped frame is not the %s frame.","expected");}
    return result;}

/* BLOCK / TAGBODY are LEXICALLY scoped: search the establishing environment and
   its lexical parents (via the per-env list), not the dynamic stack. */

static Frame* find_block_frame(Ref block_name,Ref environment){
    while(typep(environment,ENVIRONMENT_Class)){
        Environment* env=&(environment->slots->environment);
        for(Frame* frame=env->frames; frame; frame=frame->env_next){
            if(frame->type==frame_block
               && frame->parameters.block.name==block_name){
                return frame;}}
        environment=env->next_environment;}
    return NULL;}

static Frame* find_tagbody_frame(Ref tag,Ref environment){
    while(typep(environment,ENVIRONMENT_Class)){
        Environment* env=&(environment->slots->environment);
        for(Frame* frame=env->frames; frame; frame=frame->env_next){
            if(frame->type==frame_tagbody
               && !nullp(assq(tag,frame->parameters.tagbody.tags))){
                return frame;}}
        environment=env->next_environment;}
    return NULL;}

/* CATCH/THROW is DYNAMICally scoped: search the global dynamic frame stack so a
   THROW finds a CATCH established by any caller. */
static Frame* find_catch_frame(Ref catch_object,unused Ref environment){
    for(Frame* frame=g_frame_stack; frame; frame=frame->next){
        if(frame->type==frame_catch
           && frame->parameters.catch.object==catch_object){
            return frame;}}
    return NULL;}

unused static Frame* find_handler_frame(unused Ref condition,Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    Frame* frame=env->frames;
    while(frame && (frame->type!=frame_handler)){
        /* (if (find-if (function typep) condition frame->parameters.handler.condition)
               break;) */
        /* (frame->parameters.handler.condition!=catch_object) */
        NOT_IMPLEMENTED_YET();
        frame=frame->next;}
    return frame;}

/* find_restart_frame: implemented in conditions.c as part of FIND-RESTART /
   INVOKE-RESTART.  The stub is removed. */

static _Noreturn void unwind_next_frame(Frame* popped, unused Ref environment){
    /* `popped` has just been removed from the global frame stack; the new top
       is the next frame to unwind through on the way to popped->target. */
    Frame* next = g_frame_stack;
    if(next == NULL){
        FATAL("unwind_next_frame: %s","no next frame; target not reached");}
    non_local_exit_t exit = (next == popped->target)
                                ? non_local_exit_target
                                : non_local_exit_unwind;
    next->target = popped->target;
    longjmp(next->non_local_exit, (int)exit);}

static int check_non_local_exit(int exit){
    if((exit<non_local_exit_initial)||(non_local_exit_target<exit)){
        FATAL("bad non local exit code %d (should be in {0,1,2})",exit);}
    return exit;}

static void check_arg_count(Ref arguments,uword min,uword max){
    uword length=list_length(arguments);
    if((length<min)||(max<length)){
        ERROR("Wrong number of arguments expected, between %"UWORD_FORMAT" and %"UWORD_FORMAT" but got %"UWORD_FORMAT"",min,max,length);}}

static void check_arg_count_min(Ref arguments,uword min){
    uword length=list_length(arguments);
    if((length<min)){
        ERROR("Expected at least %"UWORD_FORMAT" arguments, but got %"UWORD_FORMAT"",min,length);}}

unused static void check_arg_count_even(Ref arguments){
    uword length=list_length(arguments);
    if((1&length)!=0){
        ERROR("Expected an even number of arguments but got %"UWORD_FORMAT"",length);}}

static Ref tagbody_collect_tags(Ref body){
    /* `body` is rooted (the whole list, so cdr-ing is safe); the accumulated
       `tags` list must be re-rooted before each push so a GC inside push's cons
       cannot reclaim the partial list. */
    uword pool=open_live_pool();
    add_to_live_pool(body);
    Ref tags=NIL_Symbol;
    while(consp(body)){
        if(symbolp(car(body))
           || integerp(car(body))){
            add_to_live_pool(tags);
            push(body,&tags);}
        body=cdr(body);}
    close_live_pool(pool);
    add_to_live_pool(tags);   /* re-root the result (close trimmed it) */
    return tags;}


#define COLLECTING(list) Ref list=NIL_Symbol,list##_tail
#define COLLECT(list,element)                                               \
                    do{                                                     \
                        if(nullp(list)){                                    \
                            list=cons(element,NIL_Symbol);                  \
                            list##_tail=list;}                              \
                        else{                                               \
                            rplacd(list##_tail,cons(element,NIL_Symbol));   \
                            list##_tail=cdr(list##_tail);}                  \
                    }while(0)

/*
unwind_step looping program:

if non_local_exit_initial:
1- push_frame(&frame,environment);
2- body
4- pop_frame(&frame,environment);
9- done

if non_local_exit_unwind:
3- cleanup
4- pop_frame(&frame,environment);
5- unwind_next_frame(frame); // note: _NoReturn!
9- done

if non_local_exit_target:
3- cleanup
4- pop_frame(&frame,environment);
9- done
*/

static const int unwind_step[3][4]={{1,2,4,9},{3,4,5,9},{3,4,9,9}};

// Example of usage:
//
//        UNWIND(result){
//            UWBODY{
//                evale(subform,environment,results);}
//            UWCLEANUP{
//                dolist(subform,cleanup){
//                    evale(subform,environment,results);}}}

#define UNWIND(select)                                                                          \
    for(int                                                                                     \
                unwind_select##__LINE__ = check_non_local_exit(select),                         \
                unwind_state##__LINE__ = 0,                                                     \
                unwind_index = unwind_step[unwind_select##__LINE__][unwind_state##__LINE__];    \
        unwind_index != 9;                                                                      \
        (unwind_state##__LINE__++,                                                              \
         unwind_index = unwind_step[unwind_select##__LINE__][unwind_state##__LINE__]))

#define UWBODY                                                            \
    switch(unwind_index){                                                 \
      case 1: push_frame(&frame,environment); break;                      \
      case 4: pop_frame(&frame,environment);  break;                      \
      case 5: unwind_next_frame(&frame,environment); break;               \
      default:                                break;}                     \
    if(unwind_index==2)

#define UWCLEANUP                                                       \
    if(unwind_index==3)

/* Trigger the unwind that takes us to target_frame. We do NOT pop the current
   top frame here — every UNWIND state machine pops *itself* in its UWBODY
   case 4 on re-entry via setjmp. Popping here would have torn the target
   frame off the stack when it was the only frame on it.

   Frames live on the env where the establishing special-op fired; inner
   LET / FLET / etc. create child envs that don't carry those frames. So
   we walk up the env chain looking for the nearest one with a non-NULL
   frames list — that's the "current top frame" for unwind purposes. */
#define NON_LOCAL_EXIT(target_frame)                                    \
    do {                                                                \
        Frame* current_top = g_frame_stack;                             \
        if(current_top==NULL){                                          \
            FATAL("NON_LOCAL_EXIT: %s","no active frame");}             \
        current_top->target=(target_frame);                             \
        non_local_exit_t exit_code=(current_top==(target_frame))        \
            ?non_local_exit_target                                      \
            :non_local_exit_unwind;                                     \
        longjmp(current_top->non_local_exit,(int)exit_code);            \
    } while(0)

/* These macros close the evale live-pool scope and return a result.  Closing
   the pool TRIMS every object allocated during this evale — INCLUDING the
   result — out of the live pool, so the returned value would be unrooted while
   the caller is still about to use it (one allocation away from being
   collected).  Re-root the result(s) in the now-current (outer) pool after
   closing so the caller receives a value that survives GC. */
#define PASS_VALUES()      do{close_live_pool(pool);for(uword _vi=0;_vi<results->count;_vi++){add_to_live_pool(results->values[_vi]);}return ((results->count==0)?NIL_Symbol:results->values[0]);}while(0)
//#define VALUES0()          do{close_live_pool(pool);results->count=0;return NIL_Symbol;}while(0)
#define VALUES1(v1)        do{Ref _v1=(v1);close_live_pool(pool);add_to_live_pool(_v1);results->count=1;return ((results->values[0]=_v1));}while(0)
//#define VALUES2(v1,v2)     do{close_live_pool(pool);results->count=2;results->values[0]=(v1);return ((results->values[1]=(v2)));}while(0)
//#define VALUES3(v1,v2,v3)  do{close_live_pool(pool);results->count=3;results->values[0]=(v1);results->values[1]=(v2);return ((results->values[2]=(v3)));}while(0)
#define FIRST_VALUE()      do{close_live_pool(pool);if(results->count>0){add_to_live_pool(results->values[0]);return results->values[0];}return NIL_Symbol;}while(0)


/* Run BODY(ctx) inside a top-level CATCH for BOCL's quit tag, so BOCL:QUIT can
   unwind — running pending UNWIND-PROTECT cleanups (WITH-OPEN-FILE etc.) — back
   to here.  Returns BODY's value on normal completion, or the value thrown to
   the quit tag by BOCL:QUIT.  main() wraps the whole session in this so QUIT is
   a proper non-local exit. */
Ref bocl_run_with_toplevel_catch(Ref (*body)(void*), void* ctx){
    Ref environment = eval_global_environment();
    Frame frame;
    frame.type = frame_catch;
    frame.parameters.catch.object =
        package_intern_cstring_in("%QUIT-CATCH-TAG", BOCL_Package);
    MultipleValues mv;
    mv.count = 1; mv.values[0] = integer_from_word(0);
    frame.results = &mv;
    Ref result = integer_from_word(0);
    int sel = setjmp(frame.non_local_exit);
    UNWIND(sel){
        UWBODY{ result = body(ctx); }
        UWCLEANUP{ result = (mv.count>0) ? mv.values[0] : integer_from_word(0); }}
    return result;}

static eval_counters_t eval_counters={0};

Ref get_eval_counters(void){
    Ref result=NIL_Symbol;
    push(cons(S(COMPILER-MACRO-CALL),   integer_from_word(eval_counters.compiler_macro)),       &result);
    push(cons(S(GLOBAL-FUNCTION-CALL),  integer_from_word(eval_counters.global_function)),      &result);
    push(cons(S(LOCAL-FUNCTION-CALL),   integer_from_word(eval_counters.local_function)),       &result);
    push(cons(S(GLOBAL-MACRO-CALL),     integer_from_word(eval_counters.global_macro)),         &result);
    push(cons(S(LOCAL-MACRO-CALL),      integer_from_word(eval_counters.local_macro)),          &result);
    push(cons(S(LAMBDA-FUNCTION-CALL),  integer_from_word(eval_counters.lambda_function)),      &result);
    push(cons(S(LOAD_TIME_VALUE),       integer_from_word(eval_counters.LOAD_TIME_VALUE)),      &result);
    push(cons(S(EVAL_WHEN),             integer_from_word(eval_counters.EVAL_WHEN)),            &result);
    push(cons(S(THE),                   integer_from_word(eval_counters.THE)),                  &result);
    push(cons(S(LOCALLY),               integer_from_word(eval_counters.LOCALLY)),              &result);
    push(cons(S(PROGV),                 integer_from_word(eval_counters.PROGV)),                &result);
    push(cons(S(PROGN),                 integer_from_word(eval_counters.PROGN)),                &result);
    push(cons(S(MULTIPLE_VALUE_PROG1),  integer_from_word(eval_counters.MULTIPLE_VALUE_PROG1)), &result);
    push(cons(S(MULTIPLE_VALUE_CALL),   integer_from_word(eval_counters.MULTIPLE_VALUE_CALL)),  &result);
    push(cons(S(MACROLET),              integer_from_word(eval_counters.MACROLET)),             &result);
    push(cons(S(SYMBOL_MACROLET),       integer_from_word(eval_counters.SYMBOL_MACROLET)),      &result);
    push(cons(S(LABELS),                integer_from_word(eval_counters.LABELS)),               &result);
    push(cons(S(FLET),                  integer_from_word(eval_counters.FLET)),                 &result);
    push(cons(S(LET*),                  integer_from_word(eval_counters.LET_)),                 &result);
    push(cons(S(LET),                   integer_from_word(eval_counters.LET)),                  &result);
    push(cons(S(GO),                    integer_from_word(eval_counters.GO)),                   &result);
    push(cons(S(TAGBODY),               integer_from_word(eval_counters.TAGBODY)),              &result);
    push(cons(S(UNWIND_PROTECT),        integer_from_word(eval_counters.UNWIND_PROTECT)),       &result);
    push(cons(S(THROW),                 integer_from_word(eval_counters.THROW)),                &result);
    push(cons(S(CATCH),                 integer_from_word(eval_counters.CATCH)),                &result);
    push(cons(S(RETURN_FROM),           integer_from_word(eval_counters.RETURN_FROM)),          &result);
    push(cons(S(BLOCK),                 integer_from_word(eval_counters.BLOCK)),                &result);
    push(cons(S(IF),                    integer_from_word(eval_counters.IF)),                   &result);
    push(cons(S(SETQ),                  integer_from_word(eval_counters.SETQ)),                 &result);
    push(cons(S(QUOTE),                 integer_from_word(eval_counters.QUOTE)),                &result);
    push(cons(S(FUNCTION),              integer_from_word(eval_counters.FUNCTION)),             &result);
    push(cons(S(LITERAL),               integer_from_word(eval_counters.literal)),              &result);
    push(cons(S(VARIABLE),              integer_from_word(eval_counters.variable)),             &result);
    return result;}

Ref evale(Ref form,Ref environment,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(form);
    add_to_live_pool(environment);

    if(symbolp(form)){
        eval_counters.variable++;
        if(form==NIL_Symbol){
            VALUES1(NIL_Symbol);}
        if(form==T_Symbol){
            VALUES1(T_Symbol);}
        Ref variable = environment_find_variable(form,environment);
        if(!nullp(variable)){
            /* SYMBOL-MACROLET binds a wrapper whose "value" is the
               macro expansion form, not a value. Detect that and
               re-evaluate the expansion. */
            if(env_local_symbol_macro_p(variable)){
                Ref expansion = env_local_symbol_macro_expansion(variable);
                evale(expansion,environment,results);
                PASS_VALUES();}
            if(env_global_symbol_macro_p(variable)){
                Ref expansion = env_global_symbol_macro_expansion(variable);
                evale(expansion,environment,results);
                PASS_VALUES();}
            VALUES1(env_variable_value(variable));}
        /* Not lexically bound — try the symbol's global value cell. */
        if(symbol_boundp(form)){
            VALUES1(symbol_value(form));}
        ERROR("Unbound variable: %s",string_cstring(symbol_name(form)));}

    else if(!consp(form)){
        eval_counters.literal++;
        /* The other atoms are unchanged literals: */
        VALUES1(form);}

    Ref operator=car(form);
    Ref arguments=cdr(form);

    if(symbolp(operator)){

        if(operator==QUOTE_Symbol){
            eval_counters.QUOTE++;
            check_arg_count(arguments,1,1);
            VALUES1(car(arguments));}

        if(operator==FUNCTION_Symbol){
            eval_counters.FUNCTION++;
            check_arg_count(arguments,1,1);
            Ref fname = car(arguments);
            if(symbolp(fname)){
                /* Local function bound by FLET/LABELS? Wrap it as a real Function. */
                Ref local = environment_find_function(fname,environment);
                if(!nullp(local) && env_local_function_p(local)){
                    Ref fn=function_new(env_local_function_name(local),
                                        env_local_function_lambda_list(local),
                                        env_local_function_body(local));
                    VALUES1(closure_new(fn,environment));}
                if(symbol_fboundp(fname)){
                    VALUES1(symbol_function(fname));}
                ERROR("Undefined function: %s",
                      string_cstring(symbol_name(fname)));}
            if(consp(fname)
               && eql(SETF_Symbol,car(fname))
               && consp(cdr(fname))
               && symbolp(cadr(fname))
               && nullp(cddr(fname))){
                /* (FUNCTION (SETF foo)) — the function registered by
                   (defun (setf foo) ...). */
                MultipleValues mv;
                kernel_apply(symbol_function(symbol_intern_cstring("%SETF-FUNCTION")),
                             list(cadr(fname),(Ref)NULL),&mv);
                if(mv.count>=1 && !nullp(mv.values[0])){
                    VALUES1(mv.values[0]);}
                ERROR("FUNCTION: (SETF %s) is not defined",
                      string_cstring(symbol_name(cadr(fname))));}
            if(consp(fname) && eql(LAMBDA_Symbol,car(fname))){
                /* (lambda (params...) body...) — turn into a closure. */
                check_arg_count_min(cdr(fname),1);
                Ref params=cadr(fname);
                Ref body=cddr(fname);
                Ref fn=function_new(LAMBDA_Symbol,params,body);
                VALUES1(closure_new(fn,environment));}
            ERROR("FUNCTION: invalid function name: %s",
                  string_cstring(prin1_to_string(fname)));}

        /* DEFUN / DEFPARAMETER / DEFVAR / DEFCONSTANT are treated as pseudo-special
           operators in this minimal kernel because the macro system on the Lisp
           side is not loaded yet. Once it is, these should move out. */
        if(operator==S(DEFUN)){
            check_arg_count_min(arguments,2);
            Ref name=car(arguments);
            Ref params=cadr(arguments);
            Ref body=cddr(arguments);
            if(symbolp(name)){
                Ref fn=function_new(name,params,body);
                set_symbol_function(name,fn);
                VALUES1(name);}
            /* (defun (setf foo) (val arg...) ...) — register a SETF function for
               FOO, consulted by the SETF macro and (FUNCTION (SETF foo)). */
            if(consp(name) && eql(SETF_Symbol,car(name))
               && consp(cdr(name)) && symbolp(cadr(name)) && nullp(cddr(name))){
                Ref sym=cadr(name);
                Ref fn=function_new(sym,params,body);
                MultipleValues mv;
                kernel_apply(symbol_function(symbol_intern_cstring("%REGISTER-SETF-FUNCTION")),
                             list(sym,fn,(Ref)NULL),&mv);
                VALUES1(name);}
            ERROR("DEFUN: %s is not a valid function name",string_cstring(prin1_to_string(name)));}

        if(operator==S(DEFPARAMETER)){
            check_arg_count(arguments,2,3);  /* name value [doc] */
            Ref name=car(arguments);
            if(!symbolp(name)){
                ERROR("DEFPARAMETER: %s is not a symbol",string_cstring(prin1_to_string(name)));}
            Ref value=evale(cadr(arguments),environment,results);
            define_parameter(name,value);
            VALUES1(name);}

        if(operator==S(DEFVAR)){
            check_arg_count(arguments,1,3);  /* name [value [doc]] */
            Ref name=car(arguments);
            if(!symbolp(name)){
                ERROR("DEFVAR: %s is not a symbol",string_cstring(prin1_to_string(name)));}
            if(consp(cdr(arguments)) && !symbol_boundp(name)){
                Ref value=evale(cadr(arguments),environment,results);
                define_variable(name,value);}
            else{
                define_variable(name,UNBOUND_Object);}
            VALUES1(name);}

        if(operator==S(DEFCONSTANT)){
            check_arg_count(arguments,2,3);  /* name value [doc] */
            Ref name=car(arguments);
            if(!symbolp(name)){
                ERROR("DEFCONSTANT: %s is not a symbol",string_cstring(prin1_to_string(name)));}
            Ref value=evale(cadr(arguments),environment,results);
            define_constant(name,value);
            VALUES1(name);}

        /* DEFMACRO: store the macro function on the symbol's plist under
           $MACRO. evale's call dispatch checks this before falling through
           to the function cell. */
        if(operator==S(DEFMACRO)){
            check_arg_count_min(arguments,2);
            Ref name=car(arguments);
            Ref params=cadr(arguments);
            Ref body=cddr(arguments);
            if(!symbolp(name)){
                ERROR("DEFMACRO: %s is not a symbol",string_cstring(prin1_to_string(name)));}
            Ref fn=function_new(name,params,body);
            add_to_live_pool(fn);   /* function_new leaves fn unrooted; putf() below
                conses (may GC) while fn is not yet on the plist — without this it is
                freed mid-store, leaving a dangling $MACRO value that a later
                expansion dereferences via functionp(macro_fn). */
            Ref plist=symbol_plist(name);
            plist=putf(plist,S($MACRO),fn);
            add_to_live_pool(plist);   /* protect the rebuilt plist before it is stored */
            set_symbol_plist(name,plist);
            VALUES1(name);}

        /* IN-PACKAGE is a macro in CL (it expands to (eval-when ...)). We
           treat it as a pseudo-special-op here so files can use it without
           the Lisp-side macro system loaded. The arg is NOT evaluated; it's
           used as a literal package designator. */
        if(operator==S(IN-PACKAGE)){
            check_arg_count(arguments,1,1);
            Ref designator=car(arguments);
            VALUES1(bocl_in_package(designator));}

        /* (LAMBDA formals body...) at value position is the CL macro that
           expands to (FUNCTION (LAMBDA formals body...)). Without the Lisp-
           side macro system we just synthesise the closure here. The head-
           position case ((lambda …) args…) is handled below. */
        if(operator==LAMBDA_Symbol){
            check_arg_count_min(arguments,1);
            Ref params=car(arguments);
            Ref body=cdr(arguments);
            Ref fn=function_new(LAMBDA_Symbol,params,body);
            VALUES1(closure_new(fn,environment));}

        if(operator==IF_Symbol){
            eval_counters.IF++;
            check_arg_count(arguments,2,3);
            Ref test_expr = car(arguments);
            Ref then_form = cadr(arguments);
            Ref else_form = caddr(arguments);

            if(nullp(evale(test_expr,environment,results))){
                evale(else_form,environment,results);}
            else{
                evale(then_form,environment,results);}
            PASS_VALUES();}

        if(operator==SETQ_Symbol){
            eval_counters.SETQ++;
            /* (SETQ var1 val1 var2 val2 …) — evaluate each value, assign to the
               corresponding variable in left-to-right order; result is the last value
               (NIL if zero pairs). Odd number of arguments is an error. */
            if(nullp(arguments)){
                VALUES1(NIL_Symbol);}
            Ref last_value=NIL_Symbol;
            Ref tail=arguments;
            while(consp(tail)){
                if(!consp(cdr(tail))){
                    ERROR("Odd number of arguments to %s","SETQ");}
                Ref name=car(tail);
                Ref value_form=cadr(tail);
                if(!symbolp(name)){
                    ERROR("SETQ: %s is not a symbol",
                          string_cstring(prin1_to_string(name)));}
                Ref variable=environment_find_variable(name,environment);
                if(!nullp(variable)
                   && (env_local_symbol_macro_p(variable)
                       || env_global_symbol_macro_p(variable))){
                    /* SETQ of a symbol macro IS, by definition, SETF of its
                       expansion (CLHS).  Build (SETF expansion value-form) and
                       evaluate it so SETF handles the place and evaluates the
                       value-form exactly once.  (Mirrors the read path above,
                       which expands a symbol-macro reference to its expansion.) */
                    Ref expansion = env_local_symbol_macro_p(variable)
                        ? env_local_symbol_macro_expansion(variable)
                        : env_global_symbol_macro_expansion(variable);
                    uword pool=open_live_pool();
                    add_to_live_pool(expansion);
                    add_to_live_pool(value_form);
                    Ref form=list(S(SETF),expansion,value_form,NULL);
                    add_to_live_pool(form);
                    last_value=evale(form,environment,results);
                    close_live_pool(pool);}
                else{
                    Ref value=evale(value_form,environment,results);
                    if(!nullp(variable)){
                        env_set_variable_value(value,variable);}
                    else{
                        /* Not lexically bound — assign the symbol's global value
                           cell.  Mirrors most CL implementations' implicit-global
                           behaviour for a bare SETQ at top level. */
                        set_symbol_value(name,value);}
                    last_value=value;}
                tail=cddr(tail);}
            VALUES1(last_value);}

        if(operator==PROGN_Symbol){
            eval_counters.PROGN++;
            /* An empty (progn) returns NIL (CLHS); seed results so it does not
               pass through whatever the previous evaluation happened to leave. */
            results->count=1; results->values[0]=NIL_Symbol;
            {dolist(subform,arguments){
                    evale(subform,environment,results);}}
             PASS_VALUES();}

        if(operator==LOCALLY_Symbol){
            eval_counters.LOCALLY++;
            results->count=0;
            {bool body=0;
             dolist(subform,arguments){
                 if(consp(subform)
                    && eql(DECLARE_Symbol,car(subform))){
                     if(body){
                         ERROR("DECLARE in the middle of a %s body.","LOCALLY");}}
                 else{
                     body=1;
                     evale(subform,environment,results);}}}
             PASS_VALUES();}

        if(operator==LET_Symbol){
            eval_counters.LET++;
            check_arg_count_min(arguments,1);
            Ref bindings=car(arguments);
            Ref specials=NIL_Symbol;
            Ref body=cdr(arguments);
            /* find the special declarations */
            while(consp(body)
                  && consp(car(body))
                  && eql(DECLARE_Symbol,caar(body))){
                Ref decl=pop(&body);
                if(eql(SPECIAL_Symbol,caadr(decl))){
                    dolist(symbol,cdadr(decl)){
                        push(symbol,&specials);}}}
            /* evaluate the initial-values in environment, and */
            /* bind the symbols to the values in new_environment */
            Ref new_environment=environment_new(environment,false);
            add_to_live_pool(new_environment); /* Object_new leaves it unrooted;
              protect across the initial-value evale() calls below, which allocate
              and may GC (otherwise new_environment is freed and reused). */
            bool has_dynamic_binding=false;
            {dolist(binding,bindings){
                    Ref name;
                    Ref value;
                    Ref var;
                    if(consp(binding)){
                        name=car(binding);
                        value=evale(cadr(binding),environment,results);
                        add_to_live_pool(value); /* protect across env_make_lexical_variable */
                    } else {
                        name=binding;
                        value=NIL_Symbol;}
                    if(special_variable_p(name) || !nullp(memq(name,specials))){
                        /* dynamic binding */
                        has_dynamic_binding=true;
                        bool saved_boundp=symbol_boundp(name);
                        Ref saved_value=saved_boundp?symbol_value(name):NIL_Symbol;
                        var=env_make_local_special_variable(name,as_boolean(saved_boundp),saved_value);
                        set_symbol_value(name,value);}
                    else{
                        var=env_make_lexical_variable(name,value);}
                    environment_add_variable(var,new_environment);}}
             environment=new_environment;
             if(has_dynamic_binding){
                 /* Evaluate the body then restore special bindings.
                  * UWCLEANUP handles non-local exits; after the UNWIND block
                  * (normal-return path only) we restore explicitly.
                  * This gives LET the correct CL special-variable semantics. */
                 Frame frame;
                 frame.type=frame_protect;
                 int result=setjmp(frame.non_local_exit);
                 UNWIND(result){
                     UWBODY{
                         dolist(subform,body){
                             evale(subform,environment,results);}}
                     UWCLEANUP{
                         /* Restore the special bindings on non-local exit.  Only
                          * the special variables among BINDINGS were dynamically
                          * bound; lexical ones (the common case in a mixed LET)
                          * are simply skipped — same as the normal-return path
                          * below.  (Treating a lexical binding as an error here
                          * was wrong: a non-local exit out of a LET that binds
                          * even one special alongside lexicals would FATAL.) */
                         {dolist(binding,bindings){
                                 Ref name=consp(binding)?car(binding):binding;
                                 Ref var=environment_find_variable(name,environment);
                                 if(env_local_special_variable_p(var)){
                                     if(is_true(env_local_special_variable_saved_boundp(var))){
                                         set_symbol_value(name,env_local_special_variable_saved_value(var));}
                                     else{
                                         symbol_make_unbound(name);}}}}}}
                 /* Normal-return restore: UWCLEANUP didn't run for normal exit.
                  * Non-local exits throw via unwind_next_frame so never reach here. */
                 {dolist(binding,bindings){
                         Ref name=consp(binding)?car(binding):binding;
                         Ref var=environment_find_variable(name,new_environment);
                         if(env_local_special_variable_p(var)){
                             if(is_true(env_local_special_variable_saved_boundp(var))){
                                 set_symbol_value(name,env_local_special_variable_saved_value(var));}
                             else{ symbol_make_unbound(name);}}}}
             } else {
                 dolist(subform,body){
                     evale(subform,environment,results);}}
             environment=environment_next_environment(environment);
             PASS_VALUES();}

        /* LET*: sequential binding. Each init form is evaluated in the
           env that already has the previous bindings, so it CANNOT be a
           macro over LET (which evaluates all inits in the outer env).
           For now we don't honour SPECIAL declarations in LET* — the
           lexical case covers the bulk of real usage. */
        if(operator==LET__Symbol){
            eval_counters.LET_++;
            check_arg_count_min(arguments,1);
            Ref bindings=car(arguments);
            Ref body=cdr(arguments);
            /* Collect SPECIAL declarations; ignore all other declare forms. */
            Ref specials_star=NIL_Symbol;
            while(consp(body) && consp(car(body)) && eql(DECLARE_Symbol,caar(body))){
                Ref decl=pop(&body);
                if(eql(SPECIAL_Symbol,caadr(decl))){
                    dolist(sym,cdadr(decl)){ push(sym,&specials_star);}}}
            Ref new_environment=environment_new(environment,NIL_Symbol);
            add_to_live_pool(new_environment); /* Object_new leaves it unrooted */
            environment=new_environment;
            bool has_dynamic_star=false;
            {dolist(binding,bindings){
                    Ref name;
                    Ref value;
                    if(consp(binding)){
                        name=car(binding);
                        value=evale(cadr(binding),environment,results);
                        add_to_live_pool(value);
                    } else {
                        name=binding;
                        value=NIL_Symbol;}
                    if(!symbolp(name)){
                        ERROR("LET*: %s is not a symbol",
                              string_cstring(prin1_to_string(name)));}
                    if(special_variable_p(name)||!nullp(memq(name,specials_star))){
                        has_dynamic_star=true;
                        bool saved_boundp=symbol_boundp(name);
                        Ref saved_value=saved_boundp?symbol_value(name):NIL_Symbol;
                        Ref var=env_make_local_special_variable(name,as_boolean(saved_boundp),saved_value);
                        environment_add_variable(var,environment);
                        set_symbol_value(name,value);
                    } else {
                        Ref var=env_make_lexical_variable(name,value);
                        environment_add_variable(var,environment);}}}
            if(has_dynamic_star){
                Frame frame;
                frame.type=frame_protect;
                int result=setjmp(frame.non_local_exit);
                UNWIND(result){
                    UWBODY{ dolist(subform,body){ evale(subform,environment,results);}}
                    UWCLEANUP{
                        {dolist(binding,bindings){
                                Ref name=consp(binding)?car(binding):binding;
                                Ref var=environment_find_variable(name,new_environment);
                                if(env_local_special_variable_p(var)){
                                    if(is_true(env_local_special_variable_saved_boundp(var))){
                                        set_symbol_value(name,env_local_special_variable_saved_value(var));}
                                    else{ symbol_make_unbound(name);}}}}}}
                /* Normal-return restore (see LET comment). */
                {dolist(binding,bindings){
                        Ref name=consp(binding)?car(binding):binding;
                        Ref var=environment_find_variable(name,new_environment);
                        if(env_local_special_variable_p(var)){
                            if(is_true(env_local_special_variable_saved_boundp(var))){
                                set_symbol_value(name,env_local_special_variable_saved_value(var));}
                            else{ symbol_make_unbound(name);}}}}
            } else {
                dolist(subform,body){ evale(subform,environment,results);}}
            environment=environment_next_environment(environment);
            PASS_VALUES();}

        if(operator==PROGV_Symbol){
            eval_counters.PROGV++;
            check_arg_count_min(arguments,2);
            Ref symbols=evale(car(arguments),environment,results);
            Ref values=evale(cadr(arguments),environment,results);
            Ref body=cddr(arguments);
            /* bind the symbols to the values */
            Ref new_environment=environment_new(environment,false);
            add_to_live_pool(new_environment); /* Object_new leaves it unrooted */
            {dolist(name,symbols){
                    bool boundp=symbol_boundp(name);
                    Ref value=boundp?symbol_value(name):NIL_Symbol;
                    Ref var=env_make_local_special_variable(name,as_boolean(boundp),value);
                    environment_add_variable(var,new_environment);
                    if(consp(values)){
                        value=pop(&values);
                        set_symbol_value(name,value);}
                    else{
                        symbol_make_unbound(name);}}}
             environment=new_environment;
             /* Evaluate body; restore specials on both normal and non-local exit. */
             Frame frame;
             frame.type=frame_protect;
             int result=setjmp(frame.non_local_exit);
             UNWIND(result){
                 UWBODY{
                     {dolist(subform,body){
                             evale(subform,environment,results);}}}
                 UWCLEANUP{
                     /* restore on non-local exit */
                     {dolist(name,symbols){
                             Ref var=environment_find_variable(name,environment);
                             if(!env_local_special_variable_p(var)){
                                 FATAL("Oops unwinding dynamic binding of a non local dynamic variable! %s",
                                       string_cstring(symbol_name(name)));}
                             if(is_true(env_local_special_variable_saved_boundp(var))){
                                 set_symbol_value(name,env_local_special_variable_saved_value(var));}
                             else{
                                 symbol_make_unbound(name);}}}}}
             /* Normal-return restore. */
             {dolist(name,symbols){
                     Ref var=environment_find_variable(name,new_environment);
                     if(env_local_special_variable_p(var)){
                         if(is_true(env_local_special_variable_saved_boundp(var))){
                             set_symbol_value(name,env_local_special_variable_saved_value(var));}
                         else{ symbol_make_unbound(name);}}}}
             environment=environment_next_environment(environment);
             PASS_VALUES();}

        if(operator==FLET_Symbol){
            eval_counters.FLET++;
            check_arg_count_min(arguments,1);
            Ref bindings=car(arguments);
            Ref body=cdr(arguments);
            /* Skip DECLAREs at the head of body (only SPECIAL recognised so far). */
            while(consp(body) && consp(car(body)) && eql(DECLARE_Symbol,caar(body))){
                pop(&body);}
            /* Build the new env. Local-function bodies see the OUTER env (parent of new). */
            Ref new_environment=environment_new(environment,NIL_Symbol);
            add_to_live_pool(new_environment); /* Object_new leaves it unrooted */
            {dolist(binding,bindings){
                    if(!consp(binding)){
                        ERROR("FLET: %s","each binding must be (name lambda-list . body)");}
                    Ref name=car(binding);
                    Ref lambda_list=cadr(binding);
                    Ref fn_body=cddr(binding);
                    Ref fn_wrapper=env_make_local_function(name,lambda_list,fn_body);
                    environment_add_function(fn_wrapper,new_environment);}}
             environment=new_environment;
             {dolist(subform,body){
                     evale(subform,environment,results);}}
             environment=environment_next_environment(environment);
             PASS_VALUES();}

        if(operator==LABELS_Symbol){
            eval_counters.LABELS++;
            check_arg_count_min(arguments,1);
            Ref bindings=car(arguments);
            Ref body=cdr(arguments);
            while(consp(body) && consp(car(body)) && eql(DECLARE_Symbol,caar(body))){
                pop(&body);}
            /* Build the new env FIRST, then add wrappers — each fn body will resolve
               its sibling references via the same env (because the wrapper's body is
               evaluated only when the function is called, by which point all siblings
               are bound). */
            Ref new_environment=environment_new(environment,NIL_Symbol);
            add_to_live_pool(new_environment); /* Object_new leaves it unrooted */
            {dolist(binding,bindings){
                    if(!consp(binding)){
                        ERROR("LABELS: %s","each binding must be (name lambda-list . body)");}
                    Ref name=car(binding);
                    Ref lambda_list=cadr(binding);
                    Ref fn_body=cddr(binding);
                    Ref fn_wrapper=env_make_local_function(name,lambda_list,fn_body);
                    environment_add_function(fn_wrapper,new_environment);}}
             environment=new_environment;
             {dolist(subform,body){
                     evale(subform,environment,results);}}
             environment=environment_next_environment(environment);
             PASS_VALUES();}

        /* MACROLET — lexical macro bindings. Same shape as FLET but the
           wrappers go in the function namespace as local macros, so the
           usual call dispatch (environment_find_function → check macro
           path) picks them up. */
        if(operator==MACROLET_Symbol){
            eval_counters.MACROLET++;
            check_arg_count_min(arguments,1);
            Ref bindings=car(arguments);
            Ref body=cdr(arguments);
            while(consp(body) && consp(car(body)) && eql(DECLARE_Symbol,caar(body))){
                pop(&body);}
            Ref new_environment=environment_new(environment,NIL_Symbol);
            add_to_live_pool(new_environment); /* Object_new leaves it unrooted */
            {dolist(binding,bindings){
                    if(!consp(binding)){
                        ERROR("MACROLET: %s","each binding must be (name lambda-list . body)");}
                    Ref name=car(binding);
                    Ref lambda_list=cadr(binding);
                    Ref macro_body=cddr(binding);
                    Ref wrapper=env_make_local_macro(name,lambda_list,macro_body);
                    environment_add_function(wrapper,new_environment);}}
             environment=new_environment;
             {dolist(subform,body){
                     evale(subform,environment,results);}}
             environment=environment_next_environment(environment);
             PASS_VALUES();}

        /* SYMBOL-MACROLET — lexical symbol macros. The wrapper goes in
           the VARIABLE namespace; evale's symbol-lookup path (above)
           checks env_local_symbol_macro_p before treating the symbol as
           an ordinary variable. */
        if(operator==SYMBOL_MACROLET_Symbol){
            eval_counters.SYMBOL_MACROLET++;
            check_arg_count_min(arguments,1);
            Ref bindings=car(arguments);
            Ref body=cdr(arguments);
            while(consp(body) && consp(car(body)) && eql(DECLARE_Symbol,caar(body))){
                pop(&body);}
            Ref new_environment=environment_new(environment,NIL_Symbol);
            add_to_live_pool(new_environment); /* Object_new leaves it unrooted */
            {dolist(binding,bindings){
                    if(!consp(binding) || !consp(cdr(binding))){
                        ERROR("SYMBOL-MACROLET: %s","each binding must be (name expansion)");}
                    Ref name=car(binding);
                    Ref expansion=cadr(binding);
                    if(!symbolp(name)){
                        ERROR("SYMBOL-MACROLET: %s is not a symbol",
                              string_cstring(prin1_to_string(name)));}
                    Ref wrapper=env_make_local_symbol_macro(name,expansion);
                    environment_add_variable(wrapper,new_environment);}}
             environment=new_environment;
             {dolist(subform,body){
                     evale(subform,environment,results);}}
             environment=environment_next_environment(environment);
             PASS_VALUES();}

        /* LOAD-TIME-VALUE — without a compiler, just evaluate the form
           once and return its value. (CL allows multiple evaluations as
           "permissible but not required"; we evaluate it now.) */
        if(operator==LOAD_TIME_VALUE_Symbol){
            eval_counters.LOAD_TIME_VALUE++;
            check_arg_count(arguments,1,2);   /* form [read-only-p] — ignore read-only-p */
            VALUES1(evale(car(arguments),environment,results));}

        if(operator==MULTIPLE_VALUE_CALL_Symbol){
            eval_counters.MULTIPLE_VALUE_CALL++;
            check_arg_count_min(arguments,1);
            Ref function=evale(car(arguments),environment,results);
            add_to_live_pool(function); /* protect across subsequent evale calls */
            Ref forms=cdr(arguments);
            {uword mvc_pool=open_live_pool();
             COLLECTING(args);
             dolist(subform,forms){
                 add_to_live_pool(args); /* protect accumulated list across evale */
                 results->count=0;
                 evale(subform,environment,results);
                 for(uword i=0;i<results->count;i++){
                     add_to_live_pool(results->values[i]); /* protect value before cons */
                     COLLECT(args,results->values[i]);}}
             add_to_live_pool(args); /* protect before kernel_apply */
             kernel_apply(function,args,results);
             close_live_pool(mvc_pool);}
            PASS_VALUES();}

        if(operator==MULTIPLE_VALUE_PROG1_Symbol){
            eval_counters.MULTIPLE_VALUE_PROG1++;
            check_arg_count_min(arguments,1);
            Ref first_form=car(arguments);
            Ref other_forms=cdr(arguments);
            /* Evaluate first form and save multiple results */
            MultipleValues results1;
            results1.count=0;
            evale(first_form,environment,&results1);
            /* Evaluate the other forms */
            {dolist(subform,other_forms){
                    evale(subform,environment,results);}}
             /* Return the first results */
             results->count=results1.count;
             for(uword i=0;i<results1.count;i++){
                 results->values[i]=results1.values[i];}
             PASS_VALUES();}

        if(operator==THE_Symbol){
            eval_counters.THE++;
            check_arg_count(arguments,2,2);
            evale(cadr(arguments),environment,results);
            PASS_VALUES();}

        if(operator==EVAL_WHEN_Symbol){
            eval_counters.EVAL_WHEN++;
            check_arg_count_min(arguments,1);
            Ref situations=car(arguments);
            Ref body=cdr(arguments);
            if(nullp(situations)){
                VALUES1(NIL_Symbol);}
            bool doit=false;
            {dolist(situation,situations){
                    if(eql(situation,K(COMPILE-TOPLEVEL))
                       ||eql(situation,COMPILE_Symbol)){
                        if(environment_is_toplevel(environment)
                           && !nullp(variable_value(S(*COMPILE-FILE-PATHNAME*)))){
                            doit=1;
                            break;}}
                    else if(eql(situation,K(LOAD-TOPLEVEL))
                            ||eql(situation,LOAD_Symbol)){
                        if(environment_is_toplevel(environment)
                           && !nullp(variable_value(S(*LOAD-PATHNAME*)))
                           && !nullp(variable_value(S(*LOADING-FASL-FILE*)))){
                            doit=1;
                            break;}}
                    else if(eql(situation,K(EXECUTE))
                            ||eql(situation,EVAL_Symbol)){
                        if(nullp(variable_value(S(*LOADING-FASL-FILE*)))){
                            doit=1;
                            break;}}
                    else{
                        ERROR("Invalid EVAL-WHEN situation: %s",string_cstring(prin1_to_string(situation)));}}}
             if(doit){
                 {dolist(subform,body){
                         evale(subform,environment,results);}}
                  PASS_VALUES();}
             else{
                 VALUES1(NIL_Symbol);}}

        if(operator==UNWIND_PROTECT_Symbol){
            eval_counters.UNWIND_PROTECT++;
            Ref proform=car(arguments);
            Ref cleanup=cdr(arguments);
            Frame frame;
            frame.type=frame_protect;
            int result=setjmp(frame.non_local_exit);
            MultipleValues protect_results = {0};
            UNWIND(result){
                UWBODY{
                    evale(proform,environment,&protect_results);}
                UWCLEANUP{
                    MultipleValues tmp_results = {0};
                    dolist(subform,cleanup){
                        evale(subform,environment,&tmp_results);}}}
            /* The shared unwind state machine runs UWCLEANUP only when UNWINDING
               through this frame (select 1/2); its normal-entry row {1,2,4,9}
               has no cleanup step.  CLHS requires UNWIND-PROTECT's cleanup forms
               to run on NORMAL completion too, so run them here for that path.
               (On a non-local exit, unwind_next_frame has already longjmp'd away
               above, so this point is never reached — no double execution.)
               This is why e.g. WITH-OPEN-FILE used to leave its output unflushed:
               its CLOSE never ran on the normal path. */
            if(check_non_local_exit(result)==0){
                for(uword _i=0;_i<protect_results.count;_i++){
                    add_to_live_pool(protect_results.values[_i]); }
                MultipleValues tmp_results = {0};
                dolist(subform,cleanup){
                    evale(subform,environment,&tmp_results);}}
            *results = protect_results;
            PASS_VALUES();}

        /* (HANDLER-CASE form clause...) where each clause is
              (type-spec (var) body...)
           type-spec is currently restricted to a single class designator
           (a symbol naming a built-in class, or the class object itself).
           Listed compound types (or, and, etc.) wait for the type system. */
        if(operator==S(HANDLER-CASE)){
            eval_counters.UNWIND_PROTECT++;  /* reuse counter; no dedicated one */
            check_arg_count_min(arguments,1);
            Ref form=car(arguments);
            Ref raw_clauses=cdr(arguments);
            /* Normalise each clause: (type (var) body...) → (type-class var body-form). */
            Ref clauses=NIL_Symbol;
            Ref no_error_clause=NIL_Symbol;  /* (:no-error lambda-list body...) */
            {dolist(clause,raw_clauses){
                if(!consp(clause) || !consp(cdr(clause))){
                    ERROR("HANDLER-CASE: malformed clause %s",
                          string_cstring(prin1_to_string(clause)));}
                Ref type_spec=car(clause);
                Ref varlist  =cadr(clause);
                Ref body     =cddr(clause);
                /* :no-error clause — not a condition type.  It is run (with the
                   handlers deactivated) when the protected form returns normally,
                   with its lambda-list bound to the values the form produced. */
                if(symbolp(type_spec) && type_spec==keyword_intern_cstring("NO-ERROR")){
                    no_error_clause = list(varlist, cons(PROGN_Symbol, skip_declares(body)), NULL);
                } else {
                /* Resolve type_spec to a class object.
                 * Kernel classes take priority; if not found, try Lisp find-class
                 * (which searches CLOS *%classes* for user-defined conditions). */
                Ref type_class;
                if(symbolp(type_spec)){
                    type_class = find_class(type_spec);   /* kernel lookup */
                    if(type_class == NULL){
                        /* Not a kernel class — try the CLOS class registry. */
                        Ref lisp_fc = symbol_function(S(FIND-CLASS));
                        if(lisp_fc != NULL && lisp_fc != UNBOUND_Object &&
                           (closurep(lisp_fc)||functionp(lisp_fc)||built_in_function_p(lisp_fc))){
                            MultipleValues fc_res = {0};
                            kernel_apply(lisp_fc, list(type_spec, NULL), &fc_res);
                            type_class = (fc_res.count > 0) ? fc_res.values[0] : NULL;}}}
                else {
                    type_class = type_spec;}
                if((type_class == NULL || type_class == NIL_Symbol) ||
                   (!typep(type_class,T_Class) && !clos_class_p(type_class))){
                    ERROR("HANDLER-CASE: unknown type %s",
                          string_cstring(prin1_to_string(type_spec)));}
                /* varlist must be () or (var). */
                Ref var = nullp(varlist) ? NIL_Symbol : car(varlist);
                /* body becomes a PROGN form for uniform eval. */
                Ref body_form = cons(PROGN_Symbol, skip_declares(body));
                push(list(type_class, var, body_form, NULL), &clauses);}}}
            clauses = nreverse(clauses);
            add_to_live_pool(no_error_clause);
            /* Protect the clause list from GC for the entire HANDLER-CASE
               dynamic extent.  It lives only in a C-stack Frame (not a GC
               root), so any allocation triggered while evaluating the
               protected form (including those in type_error/signal_condition)
               could otherwise sweep it.  Mirrors HANDLER-BIND's pattern. */
            add_to_live_pool(clauses);

            Frame frame;
            frame.type = frame_handler;
            frame.parameters.handler.clauses        = clauses;
            frame.parameters.handler.matched_clause = NIL_Symbol;
            frame.parameters.handler.condition      = NIL_Symbol;
            frame.results = results;
            int code = setjmp(frame.non_local_exit);
            if(code == 0){
                push_frame(&frame, environment);
                evale(form, environment, results);
                pop_frame(&frame, environment);
                /* Normal return: if there is a :no-error clause, run it (handlers
                   now inactive) with its lambda-list bound to the form's values. */
                if(!nullp(no_error_clause)){
                    Ref vals=NIL_Symbol;
                    for(uword _vi=results->count; _vi-->0;){ vals=cons(results->values[_vi],vals); }
                    add_to_live_pool(vals);
                    Ref ne_env=environment_new(environment,NIL_Symbol);
                    add_to_live_pool(ne_env);
                    bind_required_parameters(NIL_Symbol,car(no_error_clause),vals,ne_env);
                    evale(cadr(no_error_clause),ne_env,results);}
                PASS_VALUES();
            } else {
                /* Non-local entry.  Two cases:
                   - signal_condition matched a clause here (code 2): matched_clause
                     is set — run the clause below.
                   - a non-local exit (THROW/RETURN-FROM/GO/BOCL:QUIT, or a signal
                     destined for an OUTER handler) is unwinding THROUGH us (code 1):
                     matched_clause is nil — continue the unwind to the next frame
                     on the way to its real target, exactly like CATCH/BLOCK do. */
                pop_frame(&frame, environment);
                Ref clause = frame.parameters.handler.matched_clause;
                Ref cond   = frame.parameters.handler.condition;
                if(nullp(clause)){
                    unwind_next_frame(&frame, environment); /* _Noreturn */ }
                Ref var       = cadr(clause);
                Ref body_form = caddr(clause);
                /* Bind var to the condition in a fresh env and eval body. */
                Ref new_env = environment_new(environment, NIL_Symbol);
                add_to_live_pool(new_env); /* Object_new leaves it unrooted */
                if(!nullp(var)){
                    Ref wrap = env_make_lexical_variable(var, cond);
                    environment_add_variable(wrap, new_env);}
                evale(body_form, new_env, results);
                PASS_VALUES();}}

        /* (HANDLER-BIND ((type handler-fn-form) ...) body...)
           Unlike HANDLER-CASE, HANDLER-BIND does NOT unwind the stack when
           a matching condition is signaled.  Instead it calls the handler
           function in the dynamic context of the signal point (via
           signal_condition / kernel_apply).  If the handler returns normally
           signaling continues to outer handlers.  If it performs a non-local
           exit that exit takes effect normally. */
        if(operator==S(HANDLER-BIND)){
            check_arg_count_min(arguments,1);
            Ref raw_bindings = car(arguments);
            Ref body         = cdr(arguments);
            /* Evaluate each handler form and build list of (type-class . fn).
               Use a pool frame to protect the accumulating list and each
               evaluated value across allocations. */
            Ref hb_bindings = NIL_Symbol;
            {uword hb_pool=open_live_pool();
             Ref hb_tail = NIL_Symbol;
             dolist(clause, raw_bindings){
                 add_to_live_pool(hb_bindings); /* protect accumulated list */
                 if(!consp(clause) || !consp(cdr(clause))){
                     ERROR("HANDLER-BIND: malformed binding %s",
                           string_cstring(prin1_to_string(clause)));}
                 Ref type_spec  = car(clause);
                 Ref fn_form    = cadr(clause);
                 Ref type_class = symbolp(type_spec) ? find_class(type_spec) : type_spec;
                 add_to_live_pool(type_class); /* protect across evale */
                 if(!typep(type_class,T_Class)){
                     ERROR("HANDLER-BIND: unknown type %s",
                           string_cstring(prin1_to_string(type_spec)));}
                 MultipleValues fn_results;
                 evale(fn_form, environment, &fn_results);
                 Ref fn = fn_results.values[0];
                 add_to_live_pool(fn);        /* protect fn before cons */
                 add_to_live_pool(type_class);/* re-protect type_class before cons */
                 Ref cell = cons(cons(type_class, fn), NIL_Symbol);
                 add_to_live_pool(cell);
                 if(nullp(hb_bindings)){ hb_bindings = cell; hb_tail = cell; }
                 else{ rplacd(hb_tail, cell); hb_tail = cell; }}
             close_live_pool(hb_pool);}
            add_to_live_pool(hb_bindings);
            /* Push a frame_handler_bind frame, eval body, pop frame.
               The UNWIND/UWBODY macro handles push (step 1) and pop (step 4)
               automatically.  No manual push_frame/pop_frame here. */
            Frame frame;
            frame.type = frame_handler_bind;
            frame.parameters.handler_bind.bindings = hb_bindings;
            frame.results = results;
            /* An empty body must yield NIL (CLHS HANDLER-BIND).  Without this the
               dolist below never runs and `results` keeps a stale value — e.g. via
               the EVAL function (handler-bind nil) wrongly returned its own form. */
            results->count = 1;
            results->values[0] = NIL_Symbol;
            int code = setjmp(frame.non_local_exit);
            UNWIND(code){
                UWBODY{
                    dolist(subform, body){
                        evale(subform, environment, results);}}
                UWCLEANUP{}}
            PASS_VALUES();}

        /* (RESTART-BIND ((name function [keyword form]...) ...) body...)
           Establishes named restarts whose FUNCTION is called (in the dynamic
           context of the invocation, NOT by unwinding to a clause — that is
           RESTART-CASE) when INVOKE-RESTART names them.  The frame is a registry
           + an unwind pass-through; INVOKE-RESTART detects a restart-bind restart
           (its 2nd element is a function) and applies it directly. */
        if(operator==S(RESTART-BIND)){
            check_arg_count_min(arguments,1);
            Ref raw_bindings = car(arguments);
            Ref body         = cdr(arguments);
            COLLECTING(restarts);
            dolist(binding, raw_bindings){
                if(!consp(binding) || !consp(cdr(binding))){
                    ERROR("RESTART-BIND: malformed binding %s",
                          string_cstring(prin1_to_string(binding)));}
                Ref rname   = car(binding);
                MultipleValues fr={0};
                Ref fn      = evale(cadr(binding), environment, &fr);
                /* :interactive-function (used by INVOKE-RESTART-INTERACTIVELY to
                   compute the args); :report-function / :test-function ignored. */
                Ref interactive_fn = NIL_Symbol;
                {   Ref opts=cddr(binding);
                    while(consp(opts) && consp(cdr(opts))){
                        if(car(opts)==keyword_intern_cstring("INTERACTIVE-FUNCTION")){
                            MultipleValues ir={0};
                            interactive_fn = evale(cadr(opts), environment, &ir);
                            break;}
                        opts=cddr(opts);}}
                COLLECT(restarts, list(rname, fn, interactive_fn, NULL));
                add_to_live_pool(restarts);}   /* root partial list across the next evale */
            /* COLLECT already builds source order; do NOT nreverse (leftmost of
               same-named restarts must win). */
            add_to_live_pool(restarts);

            Frame frame;
            frame.type = frame_restart;
            frame.parameters.restart.restarts      = restarts;
            frame.parameters.restart.matched_name  = NIL_Symbol;
            frame.parameters.restart.matched_args  = NIL_Symbol;
            frame.results = results;
            results->count = 0;   /* empty body -> NIL */
            int code = setjmp(frame.non_local_exit);
            UNWIND(code){
                UWBODY{
                    dolist(subform, body){
                        evale(subform, environment, results);}}
                UWCLEANUP{}}
            PASS_VALUES();}

        /* (RESTART-CASE form (name lambda-list body...) ...)
           Establishes named restarts visible to FIND-RESTART / INVOKE-RESTART
           during the evaluation of `form`.  When a restart is invoked,
           INVOKE-RESTART longjmps here (code = non_local_exit_target) with
           matched_name + matched_args set on the frame; we then run the
           restart's body and return its value.
           Keyword options (:report :interactive :test) are parsed but ignored.
           NOTE: unlike HANDLER-CASE this uses the UNWIND state machine so
           non-local exits that merely pass THROUGH this frame work correctly. */
        if(operator==S(RESTART-CASE)){
            check_arg_count_min(arguments,1);
            Ref form         = car(arguments);
            Ref raw_clauses  = cdr(arguments);
            /* Parse each clause: (name lambda-list [keyword form]... body...) */
            COLLECTING(restarts);
            dolist(clause, raw_clauses){
                if(!consp(clause)){
                    ERROR("RESTART-CASE: malformed clause %s",
                          string_cstring(prin1_to_string(clause)));}
                Ref rname   = car(clause);
                Ref rest    = cdr(clause);
                Ref varlist = consp(rest) ? car(rest) : NIL_Symbol;
                rest = consp(rest) ? cdr(rest) : NIL_Symbol;
                /* Skip known keyword options (:report, :interactive, :test)
                   and their associated value forms.  We must NOT skip
                   arbitrary keywords because a restart body may legitimately
                   begin with a keyword literal (e.g. `(my-restart () :ok)`). */
                while(consp(rest) && consp(cdr(rest)) &&
                      (car(rest)==K(REPORT) ||
                       car(rest)==K(INTERACTIVE) ||
                       car(rest)==K(TEST))){
                    rest = cddr(rest);}
                Ref body_form = cons(PROGN_Symbol, skip_declares(rest));
                COLLECT(restarts, list(rname, varlist, body_form, NULL));}
            /* COLLECT already builds source order [first..last]; do NOT nreverse —
               when several clauses share a name the FIRST (leftmost) must win
               (CLHS: INVOKE-RESTART / FIND-RESTART select the most recently bound,
               which for one RESTART-CASE is the textually first). */

            Frame frame;
            frame.type = frame_restart;
            frame.parameters.restart.restarts      = restarts;
            frame.parameters.restart.matched_name  = NIL_Symbol;
            frame.parameters.restart.matched_args  = NIL_Symbol;
            frame.results = results;

            int code = setjmp(frame.non_local_exit);
            UNWIND(code){
                UWBODY{
                    evale(form, environment, results);}
                UWCLEANUP{}}

            /* Check code: 0 = form completed normally; 2 = restart invoked. */
            if(code == non_local_exit_initial){
                PASS_VALUES();}
            /* Restart was invoked (non_local_exit_target).
               frame.parameters.restart.matched_name / matched_args are set. */
            {
                Ref matched_name = frame.parameters.restart.matched_name;
                Ref matched_args = frame.parameters.restart.matched_args;
                /* Find matching restart spec. */
                Ref restart_spec = NIL_Symbol;
                dolist(r, frame.parameters.restart.restarts){
                    if(car(r)==matched_name){ restart_spec=r; break;}}
                if(nullp(restart_spec)){
                    FATAL("RESTART-CASE: no restart named %s",
                          string_cstring(prin1_to_string(matched_name)));}
                Ref varlist   = cadr(restart_spec);
                Ref body_form = caddr(restart_spec);
                /* Bind the restart arguments to the clause's lambda-list — a full
                   ordinary lambda list, so &OPTIONAL / &REST / &KEY are honoured
                   (the naive positional bind treated &OPTIONAL as a variable). */
                Ref new_env = environment_new(environment, NIL_Symbol);
                add_to_live_pool(new_env); /* Object_new leaves it unrooted */
                bind_required_parameters(NIL_Symbol, varlist, matched_args, new_env);
                evale(body_form, new_env, results);
                PASS_VALUES();}}

        else if(operator==BLOCK_Symbol){
            eval_counters.BLOCK++;
            check_arg_count_min(arguments,1);
            Ref name=car(arguments);
            Ref body=cdr(arguments);
            Frame frame;
            frame.type=frame_block;
            frame.parameters.block.name=name;
            frame.results=results;
            int result=setjmp(frame.non_local_exit);
            UNWIND(result){
                UWBODY{
                    dolist(subform,body){evale(subform,environment,results);}}
                UWCLEANUP{}}
            FIRST_VALUE();}

        else if(operator==RETURN_FROM_Symbol){
            eval_counters.RETURN_FROM++;
            check_arg_count(arguments,1,2);
            Ref name=car(arguments);
            Ref result=cadr(arguments);
            Frame* target_frame=find_block_frame(name,environment);
            if(NULL==target_frame){
                ERROR("No block named %s",string_cstring(prin1_to_string(name)));}
            evale(result,environment,target_frame->results);
            NON_LOCAL_EXIT(target_frame);}


        else if(operator==CATCH_Symbol){
            eval_counters.CATCH++;
            check_arg_count_min(arguments,1);
            /* CATCH evaluates its tag. */
            Ref tag=evale(car(arguments),environment,results);
            Ref body=cdr(arguments);
            Frame frame;
            frame.type=frame_catch;
            frame.parameters.catch.object=tag;
            frame.results=results;
            int result=setjmp(frame.non_local_exit);
            UNWIND(result){
                UWBODY{
                    dolist(subform,body){
                        evale(subform,environment,results);}}
                UWCLEANUP{}}
            return (results->count>0)?results->values[0]:NIL_Symbol;}

        else if(operator==THROW_Symbol){
            eval_counters.THROW++;
            check_arg_count(arguments,2,2);
            /* THROW evaluates its tag form and its result form. */
            Ref object=evale(car(arguments),environment,results);
            Ref result_form=cadr(arguments);
            Frame* target_frame=find_catch_frame(object,environment);
            if(NULL==target_frame){
                ERROR("No active catch for tag %s",string_cstring(prin1_to_string(object)));}
            evale(result_form,environment,target_frame->results);
            NON_LOCAL_EXIT(target_frame);}

        else if(operator==TAGBODY_Symbol){
            eval_counters.TAGBODY++;
            if(nullp(arguments)){
                VALUES1(NIL_Symbol);}
            Ref tags=tagbody_collect_tags(arguments);
            Ref body=arguments;
            Frame frame;
            frame.type=frame_tagbody;
            frame.parameters.tagbody.tags=tags;
            frame.results=results;
            while(1){
                frame.parameters.tagbody.target_tag=NULL;
                int result=setjmp(frame.non_local_exit);
                UNWIND(result){
                    UWBODY{
                        /* Tags (top-level symbols or integers) are labels —
                           they are NOT evaluated. Only proper forms (conses)
                           are evaluated. */
                        dolist(subform,body){
                            if(consp(subform)){
                                evale(subform,environment,results);}}}
                    UWCLEANUP{
                    }}
                if(frame.parameters.tagbody.target_tag==NULL){
                    /* TAGBODY always returns NIL (CLHS), never the value of its
                       last evaluated form — so e.g. (do (...) (test) body) with no
                       result forms yields NIL, not the end-test value. */
                    VALUES1(NIL_Symbol);}
                body=assq(frame.parameters.tagbody.target_tag,frame.parameters.tagbody.tags);
                if(!body){
                    ERROR("No catch for tag %p",(void*)frame.parameters.tagbody.target_tag);}}}

        else if(operator==GO_Symbol){
            eval_counters.GO++;
            check_arg_count(arguments,1,1);
            Ref tag=car(arguments);
            Frame* target_frame=find_tagbody_frame(tag,environment);
            target_frame->parameters.tagbody.target_tag=tag;
            if(NULL==target_frame){
                /* TODO: save the environment so error() and handlers can find it! */
                ERROR("No active catch for tag %p",(void*)tag);}
            NON_LOCAL_EXIT(target_frame);}

        /* operator is not a special operator. Lookup order matters here
           for lexical-vs-global shadowing:
             1. Lexical function namespace (FLET / LABELS / MACROLET) —
                a local macro must beat a global macro of the same name.
             2. Global DEFMACRO ($MACRO on the symbol's plist).
             3. Global function (symbol's function cell). */

        Ref function = NIL_Symbol;
        Ref macro    = NIL_Symbol;            /* env_local_macro / env_global_macro wrapper */

        /* 1. Lexical function namespace. */
        Ref local = environment_find_function(operator,environment);
        if(!nullp(local)){
            if(env_local_function_p(local)){
                eval_counters.local_function++;
                /* Build a transient Function from the wrapper and close it over
                   the env in which it was bound. */
                Ref fn=function_new(env_local_function_name(local),
                                    env_local_function_lambda_list(local),
                                    env_local_function_body(local));
                function=closure_new(fn,environment);}
            else if(env_local_macro_p(local) || env_global_macro_p(local)){
                eval_counters.local_macro++;
                macro=local;}
            else if(env_global_function_p(local)){
                eval_counters.global_function++;
                Ref fn=function_new(env_global_function_name(local),
                                    env_global_function_lambda_list(local),
                                    env_global_function_body(local));
                function=closure_new(fn,environment);}
            else{
                FATAL("Function-namespace wrapper of unexpected type %p",(void*)local);}}

        /* 2. Global DEFMACRO (stored on the symbol's plist). */
        if(nullp(function) && nullp(macro)){
            Ref macro_fn = getf(symbol_plist(operator), S($MACRO), NIL_Symbol);
            if(!nullp(macro_fn)){
                eval_counters.global_macro++;
                MultipleValues mac_results = {0};
                /* If the macro lambda-list begins with &WHOLE, prepend the whole
                   form (a FRESH copy of its top cons, so the macro-displacement
                   below — which mutates FORM in place — cannot corrupt the value
                   captured by the &WHOLE variable) for bind_required_parameters. */
                Ref mll=functionp(macro_fn)?macro_fn->slots->function.lambda_list:NIL_Symbol;
                Ref margs=(consp(mll)&&car(mll)==S(&WHOLE))
                          ?cons(cons(car(form),arguments),arguments):arguments;
                kernel_apply(macro_fn, margs, &mac_results);
                Ref expansion = (mac_results.count > 0) ? mac_results.values[0] : NIL_Symbol;
                /* Minimal compilation by macro displacement: cache the expansion
                   in place so this global-macro call is expanded ONCE rather than
                   on every evaluation.  This is the dominant cost in repeatedly-run
                   code: WHEN/INCF/SETF/DOTIMES etc. otherwise rebuild their
                   expansion (backquote conses + setf-expander gethash + gensym) on
                   every iteration.  CLHS 3.2.2.2 permits caching macro expansions.
                   Skip the degenerate expansion==form case (would loop forever). */
                add_to_live_pool(expansion);   /* root across BOTH the displaced
                    evale(form) below and the fall-through evale(expansion) — the
                    fall-through previously ran with EXPANSION unrooted. */
                if(expansion!=form && consp(form)){
                    if(consp(expansion)){
                        cons_rplaca(form,cons_car(expansion));
                        cons_rplacd(form,cons_cdr(expansion));
                    }else{
                        cons_rplaca(form,PROGN_Symbol);
                        cons_rplacd(form,cons(expansion,NIL_Symbol));}
                    evale(form, environment, results);   /* run displaced form */
                    PASS_VALUES();}
                evale(expansion, environment, results);
                PASS_VALUES();}
        }

        /* 3. Global function namespace (symbol's function cell). */
        if(nullp(function) && nullp(macro)){
            if(symbol_fboundp(operator)){
                Ref f=symbol_function(operator);
                if(f!=UNBOUND_Object){
                    eval_counters.global_function++;
                    function=f;}}}

        if(nullp(function) && nullp(macro)){
            ERROR("Undefined function: %s",string_cstring(symbol_name(operator)));}

        if(!nullp(macro)){
            /* Macro call: kernel_apply the macro function to the
               UNEVALUATED argument list (same convention as DEFMACRO),
               then evaluate the expansion in the same env. */
            Ref lambda_list=env_local_macro_p(macro)
                            ?env_local_macro_lambda_list(macro)
                            :env_global_macro_lambda_list(macro);
            Ref body       =env_local_macro_p(macro)
                            ?env_local_macro_body(macro)
                            :env_global_macro_body(macro);
            Ref name       =env_local_macro_p(macro)
                            ?env_local_macro_name(macro)
                            :env_global_macro_name(macro);
            Ref fn=function_new(name,lambda_list,body);
            MultipleValues mac_results = {0};
            /* &WHOLE: prepend a fresh copy of the whole form (see above). */
            Ref margs=(consp(lambda_list)&&car(lambda_list)==S(&WHOLE))
                      ?cons(cons(car(form),arguments),arguments):arguments;
            /* Expose the macro call's lexical environment to its &ENVIRONMENT
               parameter (bound during this kernel_apply).  Save/restore for nested
               expansions. */
            Ref saved_menv=g_macroexpand_environment;
            g_macroexpand_environment=environment;
            kernel_apply(fn,margs,&mac_results);
            g_macroexpand_environment=saved_menv;
            Ref expansion = (mac_results.count > 0) ? mac_results.values[0] : NIL_Symbol;
            /* Minimal compilation by macro displacement: cache the expansion in
               place so a global-macro call is expanded ONCE, not on every
               evaluation.  This is the dominant cost in repeatedly-run code
               (function bodies, loop bodies): without it, e.g. WHEN/INCF rebuild
               their expansion — backquote conses + GENSYM symbol creation — on
               every iteration.  CLHS 3.2.2.2 explicitly permits caching macro
               expansions.  We only displace GLOBAL macros (a local/lexical macro
               may expand differently in another scope) and skip the degenerate
               expansion==form case (would re-expand forever). */
            add_to_live_pool(expansion);   /* root across BOTH evale(form) and the
                fall-through evale(expansion) — the fall-through (e.g. a local
                macro, where the guard below is false) previously ran unrooted. */
            if(env_global_macro_p(macro) && expansion!=form && consp(form)){
                if(consp(expansion)){
                    cons_rplaca(form,cons_car(expansion));
                    cons_rplacd(form,cons_cdr(expansion));
                }else{
                    /* atom expansion X -> (progn X), keeping `form` a cons */
                    cons_rplaca(form,PROGN_Symbol);
                    cons_rplacd(form,cons(expansion,NIL_Symbol));}
                evale(form,environment,results);   /* run the displaced form */
                PASS_VALUES();}
            evale(expansion,environment,results);
            PASS_VALUES();}

        /* Function call: evaluate the arguments in order, then apply. */
        {uword fc_pool=open_live_pool();
         add_to_live_pool(function);   /* a freshly closure_new()'d FUNCTION is
            unrooted; protect it across the argument evale()s below, which
            allocate and may GC (otherwise it is freed before kernel_apply). */
         COLLECTING(args);
         {dolist(subform,arguments){
                 add_to_live_pool(args);           /* protect accumulated list across evale */
                 Ref value=evale(subform,environment,results);
                 add_to_live_pool(value);          /* protect value before cons in COLLECT */
                 COLLECT(args,value);}}
         add_to_live_pool(args);                   /* protect before kernel_apply */
         kernel_apply(function,args,results);
         close_live_pool(fc_pool);
         PASS_VALUES();}}

    else if(consp(operator) && eql(LAMBDA_Symbol,car(operator))){
        /* ((lambda (params...) body...) args...) */
        eval_counters.lambda_function++;
        Ref params=cadr(operator);
        Ref body=cddr(operator);
        Ref fn=function_new(LAMBDA_Symbol,params,body);
        Ref closure=closure_new(fn,environment);
        add_to_live_pool(closure); /* protect across evale calls */
        {uword lc_pool=open_live_pool();
         COLLECTING(args);
         {dolist(subform,arguments){
                 add_to_live_pool(args);           /* protect accumulated list across evale */
                 Ref value=evale(subform,environment,results);
                 add_to_live_pool(value);          /* protect value before cons in COLLECT */
                 COLLECT(args,value);}}
         add_to_live_pool(args);                   /* protect before kernel_apply */
         kernel_apply(closure,args,results);
         close_live_pool(lc_pool);}
        PASS_VALUES();}

    else{
        ERROR("evale: malformed form %s",string_cstring(prin1_to_string(form)));}

    close_live_pool(pool);
    return NIL_Symbol;}


/* ------------------------------------------------------------------ */
/*  Function application.                                              */
/*                                                                      */
/*  These live in eval.c (not kernel.c) because they need push_frame /  */
/*  pop_frame / UNWIND from the eval.c control-flow machinery.          */
/* ------------------------------------------------------------------ */

/* Bind the params from `lambda_list` to the values in `arguments` into
   `target_env`. Phase 1: required parameters only — no &optional / &rest /
   &key / &aux / &allow-other-keys. Any of those markers in the lambda list
   signals an error so we don't silently produce wrong bindings. */
/* Look up a keyword in a flat (:key value :key value …) list. Returns the
   value if found and sets *found to true; else returns NIL with *found
   false. Stops scanning at &ALLOW-OTHER-KEYS markers in the input list. */
static Ref lookup_key_argument(Ref keyword, Ref args, bool* found){
    Ref it = args;
    while(consp(it)){
        Ref k = cons_car(it);
        if(!consp(cons_cdr(it))){
            /* Odd-numbered tail — leave it for the caller's validation. */
            break;}
        if(eql(k, keyword)){
            (*found) = true;
            return cons_car(cons_cdr(it));}
        it = cons_cdr(cons_cdr(it));}
    (*found) = false;
    return NIL_Symbol;}

/* True when the keyword-argument list ARGS contains :ALLOW-OTHER-KEYS with a
   non-NIL value (CLHS 3.4.1.4.1 — this suppresses the unknown-keyword check
   regardless of the lambda list). */
static bool args_allow_other_keys(Ref args){
    bool found=false;
    Ref v=lookup_key_argument(keyword_intern_cstring("ALLOW-OTHER-KEYS"),args,&found);
    return found && !nullp(v);}

/* Validate the keyword-argument portion of a call to a &KEY function or macro.
   ARGS is the flat (:k v …) list; ACCEPTED is the list of declared keyword
   symbols.  Always required (CLHS 3.5.1.4 / 3.5.1.6): ARGS is a proper list of
   even length whose keys are symbols.  Unless SUPPRESS (from &ALLOW-OTHER-KEYS
   in the lambda list, or :ALLOW-OTHER-KEYS non-NIL in ARGS), each key must be
   one of the declared keywords (:ALLOW-OTHER-KEYS itself is always accepted).
   Violations signal a PROGRAM-ERROR. */
static void check_key_arguments(Ref function,Ref args,Ref accepted,bool suppress){
    Ref name=function_name(function);
    Ref it=args;
    while(consp(it)){
        Ref k=cons_car(it);
        if(!symbolp(k)){
            PROGRAM_ERROR("%s: keyword-argument key is not a symbol: %s",
                          string_cstring(prin1_to_string(name)),
                          string_cstring(prin1_to_string(k)));}
        if(!consp(cons_cdr(it))){
            PROGRAM_ERROR("%s: odd-length keyword-argument list: %s",
                          string_cstring(prin1_to_string(name)),
                          string_cstring(prin1_to_string(args)));}
        it=cons_cdr(cons_cdr(it));}
    if(!nullp(it)){
        PROGRAM_ERROR("%s: keyword arguments are not a proper list: %s",
                      string_cstring(prin1_to_string(name)),
                      string_cstring(prin1_to_string(args)));}
    if(suppress){ return; }
    Ref aok=keyword_intern_cstring("ALLOW-OTHER-KEYS");
    it=args;
    while(consp(it) && consp(cons_cdr(it))){
        Ref k=cons_car(it);
        bool ok=eql(k,aok);
        if(!ok){
            for(Ref a=accepted; consp(a); a=cons_cdr(a)){
                if(eql(k,cons_car(a))){ ok=true; break; }}}
        if(!ok){
            PROGRAM_ERROR("%s: unknown keyword argument %s (not one of the &KEY parameters)",
                          string_cstring(prin1_to_string(name)),
                          string_cstring(prin1_to_string(k)));}
        it=cons_cdr(cons_cdr(it));}}

/* Bind the lambda-list parameters to the values in `arguments` into
   target_env. Supports required, &OPTIONAL (with default-init form),
   &REST / &BODY (synonyms), &KEY (with default init forms; bindings
   to the local variable have the keyword's symbol-name without the
   leading colon, per CL convention), &ALLOW-OTHER-KEYS (suppresses the
   unknown-keyword check), &WHOLE (bound to the whole form, prepended by the
   macro-call dispatch), &ENVIRONMENT (bound to NIL — bocl has no first-class
   macro environments), and &AUX (sequential local bindings). */
static void bind_required_parameters(Ref function,Ref lambda_list,Ref arguments,Ref target_env){
    Ref ll=lambda_list;
    enum { in_required, in_optional, in_rest_done, in_key } state = in_required;
    Ref rest_value = NIL_Symbol;  /* if &REST was bound, the kw-args list */
    bool allow_other_keys = false;
    bool saw_rest = false;
    bool saw_key = false;             /* lambda list contains &KEY */
    Ref accepted_keys = NIL_Symbol;   /* declared &KEY keyword symbols, rooted */
    while(consp(ll)){
        Ref param=car(ll);
        /* &WHOLE var — the whole form, prepended to ARGUMENTS by the macro-call
           dispatch when the lambda-list begins with &WHOLE; bind and consume it. */
        if(symbolp(param) && param==S(&WHOLE)){
            ll=cdr(ll);
            if(!consp(ll) || !symbolp(car(ll))){
                ERROR("&WHOLE: expected a variable%s","");}
            Ref whole_var=car(ll);
            Ref whole_val=consp(arguments)?car(arguments):NIL_Symbol;
            environment_add_variable(env_make_lexical_variable(whole_var,whole_val),target_env);
            if(consp(arguments)){ arguments=cdr(arguments); }
            ll=cdr(ll);
            continue;}
        /* &ENVIRONMENT var — bound to the lexical environment of the macro call
           being expanded, so the macro can pass it to MACRO-FUNCTION / MACROEXPAND
           and see lexically-local macros.  Consumes no argument (may appear
           anywhere in the lambda list). */
        if(symbolp(param) && param==S(&ENVIRONMENT)){
            ll=cdr(ll);
            if(!consp(ll) || !symbolp(car(ll))){
                ERROR("&ENVIRONMENT: expected a variable%s","");}
            environment_add_variable(env_make_lexical_variable(car(ll),current_macroexpand_environment()),target_env);
            ll=cdr(ll);
            continue;}
        /* &AUX (var) or (var init-form) — bind local variables in order. */
        if(symbolp(param) && param==S(&AUX)){
            /* &AUX is always last and returns early, so validate any preceding
               &KEY arguments here before binding the &AUX locals. */
            if(saw_key){
                check_key_arguments(function, rest_value, accepted_keys,
                                    allow_other_keys || args_allow_other_keys(rest_value));}
            ll=cdr(ll);
            while(consp(ll)){
                Ref aux_spec=car(ll);
                Ref aux_var, aux_init;
                if(symbolp(aux_spec)){
                    aux_var=aux_spec; aux_init=NIL_Symbol;
                } else if(consp(aux_spec)){
                    aux_var=car(aux_spec);
                    aux_init=nullp(cdr(aux_spec))?NIL_Symbol:cadr(aux_spec);
                } else {
                    ERROR("&AUX: bad spec %s",string_cstring(prin1_to_string(aux_spec)));}
                MultipleValues aux_res={0};
                Ref val=(nullp(aux_init)?NIL_Symbol:evale(aux_init,target_env,&aux_res));
                Ref wrap=env_make_lexical_variable(aux_var,val);
                environment_add_variable(wrap,target_env);
                ll=cdr(ll);}
            return;  /* &AUX is always last */
        }
        if(symbolp(param) && param==S(&OPTIONAL)){
            state = in_optional;
            ll = cdr(ll);
            continue;}
        if(symbolp(param) && (param==S(&REST) || param==S(&BODY))){
            ll = cdr(ll);
            if(!consp(ll) || !symbolp(car(ll))){
                ERROR("%s: expected a symbol after &REST",
                      string_cstring(symbol_name(function_name(function))));}
            Ref rest_param = car(ll);
            rest_value = arguments;     /* keep for &KEY lookups too */
            saw_rest = true;
            Ref wrap = env_make_lexical_variable(rest_param, arguments);
            environment_add_variable(wrap, target_env);
            state = in_rest_done;
            ll = cdr(ll);
            continue;}
        if(symbolp(param) && param==S(&KEY)){
            /* All remaining positional args (or the captured &REST) form
               the flat keyword-value list. */
            if(!saw_rest){ rest_value = arguments; }
            state = in_key;
            saw_key = true;
            ll = cdr(ll);
            continue;}
        if(symbolp(param) && param==S(&ALLOW-OTHER-KEYS)){
            allow_other_keys = true;
            ll = cdr(ll);
            continue;}
        if(state == in_rest_done){
            ERROR("Lambda-list parameter after &REST without &KEY: %s",
                  string_cstring(prin1_to_string(param)));}
        if(state == in_optional){
            /* &OPTIONAL element: symbol, or (var init), or (var init svar). */
            Ref opt_name, opt_init;
            Ref opt_svar = NIL_Symbol;
            if(symbolp(param)){
                opt_name = param;
                opt_init = NIL_Symbol;
            } else if(consp(param)){
                opt_name = car(param);
                opt_init = consp(cdr(param)) ? cadr(param) : NIL_Symbol;
                if(consp(cdr(param)) && consp(cddr(param))){
                    opt_svar = caddr(param);}
            } else {
                ERROR("Bad &OPTIONAL parameter: %s",
                      string_cstring(prin1_to_string(param)));}
            Ref value;
            bool supplied;
            if(consp(arguments)){
                value = pop(&arguments);
                supplied = true;
            } else {
                MultipleValues init_results = {0};
                value = evale(opt_init, target_env, &init_results);
                supplied = false;}
            Ref var = env_make_lexical_variable(opt_name, value);
            environment_add_variable(var, target_env);
            if(!nullp(opt_svar)){
                Ref sw = env_make_lexical_variable(opt_svar,
                                                   supplied ? T_Symbol : NIL_Symbol);
                environment_add_variable(sw, target_env);}
            ll = cdr(ll);
            continue;}
        if(state == in_key){
            /* &KEY param: symbol  | (var init)  | (var init svar)
               | ((:kw var) init [svar])
               Spec is the keyword the user uses; var is the local name;
               svar (if present) is bound to T when the keyword was
               supplied by the caller, NIL otherwise. */
            Ref var_name;
            Ref keyword;
            Ref init_form  = NIL_Symbol;
            Ref svar       = NIL_Symbol;          /* supplied-p var, or NIL */
            if(symbolp(param)){
                var_name = param;
                char* nm = string_cstring(symbol_name(param));
                keyword = keyword_intern_cstring(nm);
                free(nm);
            } else if(consp(param)){
                Ref first = car(param);
                if(consp(first)){
                    keyword  = car(first);
                    var_name = cadr(first);
                } else {
                    var_name = first;
                    char* nm = string_cstring(symbol_name(first));
                    keyword = keyword_intern_cstring(nm);
                    free(nm);}
                if(consp(cdr(param))){ init_form = cadr(param); }
                if(consp(cdr(param)) && consp(cddr(param))){
                    svar = caddr(param);}
            } else {
                ERROR("Bad &KEY parameter: %s",
                      string_cstring(prin1_to_string(param)));}
            /* Record the declared keyword for the unknown-keyword check; root
               it across the init-form evale below (which may GC). */
            push(keyword, &accepted_keys);
            add_to_live_pool(accepted_keys);
            bool found = false;
            Ref value = lookup_key_argument(keyword, rest_value, &found);
            if(!found){
                MultipleValues init_r = {0};
                value = evale(init_form, target_env, &init_r);}
            Ref var = env_make_lexical_variable(var_name, value);
            environment_add_variable(var, target_env);
            if(!nullp(svar)){
                Ref sw = env_make_lexical_variable(svar,
                                                   found ? T_Symbol : NIL_Symbol);
                environment_add_variable(sw, target_env);}
            ll = cdr(ll);
            continue;}
        /* Required parameter. */
        if(!symbolp(param)){
            ERROR("Lambda-list parameter is not a symbol: %s",
                  string_cstring(prin1_to_string(param)));}
        Ref value=pop_mandatory_argument(function,&arguments);
        Ref var=env_make_lexical_variable(param,value);
        environment_add_variable(var,target_env);
        ll=cdr(ll);}
    if(state == in_required || state == in_optional){
        check_no_more_arguments(function,arguments);}
    /* Validate the keyword-argument plist for any &KEY lambda list (shape +
       unknown-keyword check, unless other keys are allowed). */
    if(saw_key){
        check_key_arguments(function, rest_value, accepted_keys,
                            allow_other_keys || args_allow_other_keys(rest_value));}}

/* Run `body` in `environment` with a block frame named `block_name` so
   RETURN-FROM works inside user code. (Parameter named `environment` because
   the UWBODY macro hardcodes that name.) */
static void eval_body_in_block(Ref function,Ref block_name,Ref body,Ref environment,MultipleValues* results){
    Frame frame;
    frame.type=frame_block;
    frame.parameters.block.name=block_name;
    frame.results=results;
    int code=setjmp(frame.non_local_exit);
    UNWIND(code){
        UWBODY{
            {dolist(form,body){
                    /* Skip (declare ...) forms — declarations, not code. */
                    if(consp(form) && eql(DECLARE_Symbol,cons_car(form))) continue;
                    evale(form,environment,results);}}}
        UWCLEANUP{}}
    (void)function;}

void kernel_apply_function(Ref function,Function* slots,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    /* A bare Function with no captured lexical env: parent is the global env so
       references to globals still resolve. */
    Ref env=environment_new(eval_global_environment(),NIL_Symbol);
    add_to_live_pool(env);
    bind_required_parameters(function,slots->lambda_list,arguments,env);
    eval_body_in_block(function,slots->lisp_name,slots->body,env,results);
    close_live_pool(pool);}

void kernel_apply_closure(Ref closure,Closure* slots,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(closure);
    add_to_live_pool(arguments);
    check_type(slots->function,FUNCTION_Class);
    Function* fslots=&(slots->function->slots->function);
    Ref env=environment_new(slots->environment,NIL_Symbol);
    add_to_live_pool(env);
    bind_required_parameters(slots->function,fslots->lambda_list,arguments,env);
    eval_body_in_block(slots->function,fslots->lisp_name,fslots->body,env,results);
    close_live_pool(pool);}
