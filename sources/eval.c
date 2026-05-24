#include <setjmp.h>
#include "eval.h"
#include "kernel.h"
#include "boerror.h"
#include "cons.h"
#include "environment.h"
#include "variable.h"
#include "bocl.h"

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

typedef enum {
    non_local_exit_initial=0,
    non_local_exit_unwind=1,
    non_local_exit_target=2,
} non_local_exit_t;


static void push_frame(Frame* frame,Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    frame->next=env->frames;
    env->frames=frame;}

static Frame* pop_frame(Frame* frame,Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    /*
    if frame!=NULL, then check if frame==current_frame(environment)
    (if not, then signal a fatal error).
    pop the current frame and return it.
    */
    Environment* env=&(environment->slots->environment);
    Frame* result=env->frames;
    if(result==NULL){
        FATAL("Frame stack is %s!","empty");}
    env->frames=result->next;
    if((frame!=NULL) && (frame!=result)){
        FATAL("Popped frame is not the %s frame.","expected");}
    return result;}

static Frame* find_block_frame(Ref block_name,Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    Frame* frame=env->frames;
    while(frame && ((frame->type!=frame_block) || (frame->parameters.block.name!=block_name))){
        frame=frame->next;}
    return frame;}

static Frame* find_catch_frame(Ref catch_object,Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    Frame* frame=env->frames;
    while(frame && ((frame->type!=frame_catch) || (frame->parameters.catch.object!=catch_object))){
        frame=frame->next;}
    return frame;}

static Frame* find_tagbody_frame(Ref tag,Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    Frame* frame=env->frames;
    while(frame && ((frame->type!=frame_tagbody) || nullp(assq(tag,frame->parameters.tagbody.tags)))){
        frame=frame->next;}
    return frame;}

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

unused static Frame* find_restart_frame(unused Ref restart,unused Ref environment){
    NOT_IMPLEMENTED_YET();
    return NULL;}

static _Noreturn void unwind_next_frame(Frame* frame){
    /* Unwind one frame */
    non_local_exit_t exit=((frame->next==frame->target)
                           ?non_local_exit_target
                           :non_local_exit_unwind);
    frame->next->target=frame->target;
    longjmp(frame->next->non_local_exit,(int)exit);}

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
    Ref tags=NIL_Symbol;
    while(consp(body)){
        if(symbolp(car(body))
           || integerp(car(body))){
            push(body,&tags);}
        body=cdr(body);}
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

#define UWBODY                                                          \
    switch(unwind_index){                                               \
      case 1: push_frame(&frame,environment); break;                    \
      case 4: pop_frame(&frame,environment);  break;                    \
      case 5: unwind_next_frame(&frame);      break;                    \
      default:                                break;}                   \
    if(unwind_index==2)

#define UWCLEANUP                                                       \
    if(unwind_index==3)

/* Trigger the unwind that takes us to target_frame. We do NOT pop the current
   top frame here — every UNWIND state machine pops *itself* in its UWBODY
   case 4 on re-entry via setjmp. Popping here would have torn the target
   frame off the stack when it was the only frame on it. */
#define NON_LOCAL_EXIT(target_frame)                                    \
    do {                                                                \
        Frame* current_top=environment->slots->environment.frames;      \
        if(current_top==NULL){                                          \
            FATAL("NON_LOCAL_EXIT: %s","no active frame");}             \
        current_top->target=(target_frame);                             \
        non_local_exit_t exit_code=(current_top==(target_frame))        \
            ?non_local_exit_target                                      \
            :non_local_exit_unwind;                                     \
        longjmp(current_top->non_local_exit,(int)exit_code);            \
    } while(0)

#define PASS_VALUES()      do{close_live_pool(pool);return ((results->count==0)?NIL_Symbol:results->values[0]);}while(0)
//#define VALUES0()          do{close_live_pool(pool);results->count=0;return NIL_Symbol;}while(0)
#define VALUES1(v1)        do{close_live_pool(pool);results->count=1;return ((results->values[0]=(v1)));}while(0)
//#define VALUES2(v1,v2)     do{close_live_pool(pool);results->count=2;results->values[0]=(v1);return ((results->values[1]=(v2)));}while(0)
//#define VALUES3(v1,v2,v3)  do{close_live_pool(pool);results->count=3;results->values[0]=(v1);results->values[1]=(v2);return ((results->values[2]=(v3)));}while(0)
#define FIRST_VALUE()      do{close_live_pool(pool);return (results->count>0)?results->values[0]:NIL_Symbol;}while(0)


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
        /* Symbol macros must have been expanded by the minimal compilation,
        so we only have variables. */
        if(form==NIL_Symbol){
            VALUES1(NIL_Symbol);}
        if(form==T_Symbol){
            VALUES1(T_Symbol);}
        Ref variable = environment_find_variable(form,environment);
        if(!nullp(variable)){
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
                /* (SETF NAME) — no SETF support yet. */
                ERROR("FUNCTION: (SETF %s) not yet supported",
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
            if(!symbolp(name)){
                ERROR("DEFUN: %s is not a symbol",string_cstring(prin1_to_string(name)));}
            Ref fn=function_new(name,params,body);
            set_symbol_function(name,fn);
            VALUES1(name);}

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
                Ref value=evale(value_form,environment,results);
                Ref variable=environment_find_variable(name,environment);
                if(!nullp(variable)){
                    env_set_variable_value(value,variable);}
                else{
                    /* Not lexically bound — assign the symbol's global value cell.
                       Mirrors most CL implementations' implicit-global behaviour
                       for a bare SETQ at top level. */
                    set_symbol_value(name,value);}
                last_value=value;
                tail=cddr(tail);}
            VALUES1(last_value);}

        if(operator==PROGN_Symbol){
            eval_counters.PROGN++;
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
            bool has_dynamic_binding=false;
            {dolist(binding,bindings){
                    Ref name;
                    Ref value;
                    Ref var;
                    if(consp(binding)){
                        name=car(binding);
                        value=evale(cadr(binding),environment,results);}
                    else{
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
                 /* unwind-protect the body evaluation */
                 Frame frame;
                 frame.type=frame_protect;
                 int result=setjmp(frame.non_local_exit);
                 UNWIND(result){
                     UWBODY{
                         /* evaluate the body */
                         dolist(subform,body){
                             evale(subform,environment,results);}}
                     UWCLEANUP{
                         /* restore the dynamic bindings */
                         {dolist(binding,bindings){
                                 Ref name=consp(binding)?car(binding):binding;
                                 Ref var=environment_find_variable(name,environment);
                                 if(!env_local_special_variable_p(var)){
                                     FATAL("Oops unwinding dynamic binding of a nonn local special variable! %s",
                                           string_cstring(symbol_name(name)));}
                                 if(is_true(env_local_special_variable_saved_boundp(var))){
                                     set_symbol_value(name,env_local_special_variable_saved_value(var));}
                                 else{
                                     symbol_make_unbound(name);}}}}}}
             else{
                 dolist(subform,body){
                     evale(subform,environment,results);}}
             environment=environment_next_environment(environment);
             PASS_VALUES();}

        /* NO LET*, since it's minimal-compiled out into LET */

        if(operator==PROGV_Symbol){
            eval_counters.PROGV++;
            check_arg_count_min(arguments,2);
            Ref symbols=evale(car(arguments),environment,results);
            Ref values=evale(cadr(arguments),environment,results);
            Ref body=cddr(arguments);
            /* bind the symbols to the values */
            Ref new_environment=environment_new(environment,false);
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
             /* unwind-protect the body evaluation */
             Frame frame;
             frame.type=frame_protect;
             int result=setjmp(frame.non_local_exit);
             UNWIND(result){
                 UWBODY{
                     /* evaluate the body */
                     {dolist(subform,body){
                             evale(subform,environment,results);}}}
                 UWCLEANUP{
                     /* restore the dynamic bindings */
                     {dolist(name,symbols){
                             Ref var=environment_find_variable(name,environment);
                             if(!env_local_special_variable_p(var)){
                                 FATAL("Oops unwinding dynamic binding of a non local dynamic variable! %s",
                                       string_cstring(symbol_name(name)));}
                             if(is_true(env_local_special_variable_saved_boundp(var))){
                                 set_symbol_value(name,env_local_special_variable_saved_value(var));}
                             else{
                                 symbol_make_unbound(name);}}}}}
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

        if(operator==SYMBOL_MACROLET_Symbol){
            NOT_IMPLEMENTED_YET();
        }
        if(operator==MACROLET_Symbol){
            NOT_IMPLEMENTED_YET();
        }

        if(operator==MULTIPLE_VALUE_CALL_Symbol){
            eval_counters.MULTIPLE_VALUE_CALL++;
            check_arg_count_min(arguments,1);
            Ref function=evale(car(arguments),environment,results);
            Ref forms=cdr(arguments);
            COLLECTING(args);
            dolist(subform,forms){
                results->count=0;
                evale(subform,environment,results);
                for(uword i=0;i<results->count;i++){
                    COLLECT(args,results->values[i]);}}
            kernel_apply(function,args,results);
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
            UNWIND(result){
                UWBODY{
                    evale(proform,environment,results);}
                UWCLEANUP{
                    dolist(subform,cleanup){
                        evale(subform,environment,results);}}}}

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
            {dolist(clause,raw_clauses){
                if(!consp(clause) || !consp(cdr(clause))){
                    ERROR("HANDLER-CASE: malformed clause %s",
                          string_cstring(prin1_to_string(clause)));}
                Ref type_spec=car(clause);
                Ref varlist  =cadr(clause);
                Ref body     =cddr(clause);
                /* Resolve type_spec to a class object. */
                Ref type_class = symbolp(type_spec) ? find_class(type_spec) : type_spec;
                if(!typep(type_class,T_Class)){
                    ERROR("HANDLER-CASE: unknown type %s",
                          string_cstring(prin1_to_string(type_spec)));}
                /* varlist must be () or (var). */
                Ref var = nullp(varlist) ? NIL_Symbol : car(varlist);
                /* body becomes a PROGN form for uniform eval. */
                Ref body_form = cons(PROGN_Symbol, body);
                push(list(type_class, var, body_form, NULL), &clauses);}}
            clauses = nreverse(clauses);

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
                PASS_VALUES();
            } else {
                /* signal_condition longjmps us here; clause + cond on the frame. */
                pop_frame(&frame, environment);
                Ref clause = frame.parameters.handler.matched_clause;
                Ref cond   = frame.parameters.handler.condition;
                if(nullp(clause)){
                    /* unwind-only: re-raise to outer handler. Not currently used. */
                    FATAL("HANDLER-CASE unwound without a matched clause%s","");}
                Ref var       = cadr(clause);
                Ref body_form = caddr(clause);
                /* Bind var to the condition in a fresh env and eval body. */
                Ref new_env = environment_new(environment, NIL_Symbol);
                if(!nullp(var)){
                    Ref wrap = env_make_lexical_variable(var, cond);
                    environment_add_variable(wrap, new_env);}
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
                        dolist(subform,body){evale(subform,environment,results);}}
                    UWCLEANUP{
                    }}
                if(frame.parameters.tagbody.target_tag==NULL){
                    PASS_VALUES();}
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

        /* operator is not a special operator, let's try functions and macros. */
        Ref function = NIL_Symbol;
        Ref macro    = NIL_Symbol;            /* env_local_macro / env_global_macro wrapper */

        /* 1. Lexical function namespace (FLET/LABELS, future MACROLET). */
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

        /* 2. Global function namespace (symbol's function cell). */
        if(nullp(function) && nullp(macro)){
            if(symbol_fboundp(operator)){
                Ref f=symbol_function(operator);
                if(f!=UNBOUND_Object){
                    eval_counters.global_function++;
                    function=f;}}}

        if(nullp(function) && nullp(macro)){
            ERROR("Undefined function: %s",string_cstring(symbol_name(operator)));}

        if(!nullp(macro)){
            /* Macro call: build (form env) as the argument list, kernel_apply the
               macro function, then evaluate the expansion in the same env. */
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
            Ref args=NIL_Symbol;
            push(environment,&args);
            push(form,&args);
            kernel_apply(fn,args,results);
            evale(results->values[0],environment,results);
            PASS_VALUES();}

        /* Function call: evaluate the arguments in order, then apply. */
        {COLLECTING(args);
         {dolist(subform,arguments){
                 Ref value=evale(subform,environment,results);
                 COLLECT(args,value);}}
         kernel_apply(function,args,results);
         PASS_VALUES();}}

    else if(consp(operator) && eql(LAMBDA_Symbol,car(operator))){
        /* ((lambda (params...) body...) args...) */
        eval_counters.lambda_function++;
        Ref params=cadr(operator);
        Ref body=cddr(operator);
        Ref fn=function_new(LAMBDA_Symbol,params,body);
        Ref closure=closure_new(fn,environment);
        COLLECTING(args);
        {dolist(subform,arguments){
                Ref value=evale(subform,environment,results);
                COLLECT(args,value);}}
        kernel_apply(closure,args,results);
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
static void bind_required_parameters(Ref function,Ref lambda_list,Ref arguments,Ref target_env){
    Ref ll=lambda_list;
    while(consp(ll)){
        Ref param=car(ll);
        if(symbolp(param)
           && (param==S(&OPTIONAL) || param==S(&REST)
               || param==S(&KEY)   || param==S(&AUX)
               || param==S(&ALLOW-OTHER-KEYS) || param==S(&BODY)
               || param==S(&WHOLE) || param==S(&ENVIRONMENT))){
            ERROR("Lambda-list keyword %s not yet implemented",
                  string_cstring(symbol_name(param)));}
        if(!symbolp(param)){
            ERROR("Lambda-list parameter is not a symbol: %s",
                  string_cstring(prin1_to_string(param)));}
        Ref value=pop_mandatory_argument(function,&arguments);
        Ref var=env_make_lexical_variable(param,value);
        environment_add_variable(var,target_env);
        ll=cdr(ll);}
    check_no_more_arguments(function,arguments);}

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
