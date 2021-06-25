#include <setjmp.h>
#include "eval.h"
#include "kernel.h"
#include "boerror.h"
#include "cons.h"
#include "environment.h"
#include "variable.h"

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

static Frame* find_handler_frame(Ref condition,Ref environment){
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

static Frame* find_restart_frame(Ref restart,Ref environment){
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

static void check_arg_count_even(Ref arguments){
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

#define NON_LOCAL_EXIT(target_frame)                                    \
    do {                                                                \
        Frame* frame=pop_frame(NULL,environment);                       \
        frame->target=target_frame;                                     \
        unwind_next_frame(frame); } while(0)

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
        Ref variable = environment_find_variable(form,environment);
        if(nullp(variable)){
            /* TODO: restart to change the variable or to bind it? */
            error("NO SUCH VARIABLE %p",(void*)form);}
        VALUES1(env_variable_value(variable));}

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
            if(symbolp(fname)
               || (consp(fname)
                   && eql(SETF_Symbol,car(fname))
                   && consp(cdr(fname))
                   && symbolp(cadr(fname))
                   && nullp(cddr(fname)))){
                Ref fun = environment_find_function(fname,environment);
                if(fun==0){
                    ERROR("No such function %s",string_cstring(prin1_to_string(fname)));}
                if(env_global_function_p(fun)){
                    /* return the global function */
                }
                else{
                    /* Build a local closure */
                }
            }else{ /* lambda-expression? */
            }
            VALUES1(NIL_Symbol);}

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
            NOT_IMPLEMENTED_YET();}

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

        if(operator==LABELS_Symbol){
            NOT_IMPLEMENTED_YET();
        }

        if(operator==FLET_Symbol){
            NOT_IMPLEMENTED_YET();
            eval_counters.FLET++;
            check_arg_count_min(arguments,1);
            Ref bindings=car(arguments);
            Ref specials=NIL_Symbol;
            Ref body=cdr(arguments);
            /* find the special declarations */
            while(consp(body)
                  && consp(car(body))
                  && eql(DECLARE_Symbol,caar(body))){
                Ref decl=pop(&body);
                if(eql(SPECIAL_Symbol,car(decl))){
                    dolist(symbol,cdr(decl)){
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
                error("No block named ~p",(void*)name);}
            evale(result,environment,target_frame->results);
            NON_LOCAL_EXIT(target_frame);}


        else if(operator==CATCH_Symbol){
            eval_counters.CATCH++;
            check_arg_count_min(arguments,1);
            Ref tag=car(arguments);
            Ref body=cdr(arguments);
            Frame frame;
            frame.type=frame_catch;
            frame.parameters.catch.object=tag;
            frame.results=results;
            int result=setjmp(frame.non_local_exit);
            UNWIND(result){
                UWBODY{
                    dolist(subform,body){
                        /* skip tags, since they're not variables… */
                        if(!(symbolp(subform)||integerp(subform))){
                            evale(subform,environment,results);}}}
                UWCLEANUP{}}
            return (results->count>0)?results->values[0]:NIL_Symbol;}

        else if(operator==THROW_Symbol){
            eval_counters.THROW++;
            check_arg_count(arguments,2,2);
            Ref object=car(arguments);
            Ref result=cadr(arguments);
            Frame* target_frame=find_catch_frame(object,environment);
            if(NULL==target_frame){
                error("No active catch for tag %p",(void*)object);}
            evale(result,environment,target_frame->results);
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

        /* operator is not a special operator, let's try functions and macros:  */
        Ref function=environment_find_function(operator,environment);
        if(nullp(function)){
            ERROR("No such function %s",string_cstring(symbol_name(operator)));}

        Ref fun=NIL_Symbol;
        Ref mac=NIL_Symbol;
        if(env_global_function_p(function)){
            Ref compiler_macro=env_global_function_compiler_macro(function);
            if(nullp(compiler_macro)){
                eval_counters.global_function++;
                /* call the function */
                fun = f();}
            else{
                eval_counters.compiler_macro++;
                /* expand the compiler macro and evaluate the result */
                mac = f(compiler_macro);}}
        else if(env_local_function_p(function)){
            eval_counters.local_function++;
            /* call the function */
            fun = f();}
        else if(env_global_macro_p(function)){
            eval_counters.global_macro++;
            /* expand the macro and evaluate the result */
            mac = f();}
        else if(env_local_macro_p(function)){
            eval_counters.local_macro++;
            /* expand the macro and evaluate the result */
            mac = f();}
        else{
            FATAL("Got a function that is none of the expected types %p",(void*)function);}

        if(!nullp(fun)){
            /* Call a function */
            COLLECTING(args);
            {dolist(subform,arguments){
                    Ref value=evale(subform,environment,results);
                    COLLECT(args,value);}}
             kernel_apply(fun,args,results);
             PASS_VALUES();}
        else if(!nullp(mac)){
            /* Expand a macro */
            Ref args = NIL_Symbol;
            push(environment,&args);
            push(form,&args);
            kernel_apply(mac,args,results);
            evale(results->values[0],environment,results);
            PASS_VALUES();}}
    else{
        /* operator = lambda expression?  ((lambda …) …)*/
        NOT_IMPLEMENTED_YET();
        eval_counters.lambda_function++;
    }

    close_live_pool(pool);
    return NIL_Symbol;}
