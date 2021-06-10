#include <setjmp.h>
#include "eval.h"
#include "kernel.h"
#include "boerror.h"
#include "cons.h"

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
    while(frame && (frame->type!=frame_block) && (frame->parameters.block.name!=block_name)){
        frame=frame->next;}
    return frame;}

static Frame* find_catch_frame(Ref catch_object,Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    Frame* frame=env->frames;
    while(frame && (frame->type!=frame_catch) && (frame->parameters.catch.object!=catch_object)){
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

//#define VALUES0()          do{close_live_pool(pool);results->count=0;return NIL_Symbol;}while(0)
#define VALUES1(v1)        do{close_live_pool(pool);results->count=1;return ((results->values[0]=(v1)));}while(0)
//#define VALUES2(v1,v2)     do{close_live_pool(pool);results->count=2;results->values[0]=(v1);return ((results->values[1]=(v2)));}while(0)
//#define VALUES3(v1,v2,v3)  do{close_live_pool(pool);results->count=3;results->values[0]=(v1);results->values[1]=(v2);return ((results->values[2]=(v3)));}while(0)
#define FIRST_VALUE()      do{close_live_pool(pool);return (results->count>0)?results->values[0]:NIL_Symbol;}while(0)

Ref evale(Ref form,Ref environment,MultipleValues* results){
    uword pool=open_live_pool();

    if(symbolp(form)){
        /* Symbol macros must have been expanded by the minimal compilation,
        so we only have variables. */
        Ref variable = environment_find_variable(form,environment);
        if(nullp(variable)){
            /* TODO: restart to change the variable or to bind it? */
            error("NO SUCH VARIABLE %p",(void*)form);}
        VALUES1(env_variable_value(variable));}

    else if(!consp(form)){
        /* The other atoms are unchanged literals: */
        VALUES1(form);}

    Ref operator=car(form);
    Ref arguments=cdr(form);
    
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

#define NON_LOCAL_EXIT()                                                \
    do {                                                                \
        Frame* frame=pop_frame(NULL,environment);                       \
        frame->target=target_frame;                                     \
        unwind_next_frame(frame); } while(0)
    
    if(operator==UNWIND_PROTECT_Symbol){
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
        check_arg_count(arguments,1,2);
        Ref name=car(arguments);
        Ref result=cadr(arguments);
        Frame* target_frame=find_block_frame(name,environment);
        if(NULL==target_frame){
            error("No block named ~p",(void*)name);}
        evale(result,environment,target_frame->results);
        NON_LOCAL_EXIT();}


    else if(operator==CATCH_Symbol){
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
                dolist(subform,body){evale(subform,environment,results);}}
            UWCLEANUP{}}
        return (results->count>0)?results->values[0]:NIL_Symbol;}

    else if(operator==THROW_Symbol){
        check_arg_count(arguments,2,2);
        Ref object=car(arguments);
        Ref result=cadr(arguments);
        Frame* target_frame=find_catch_frame(object,environment);
        if(NULL==target_frame){
            error("No active catch for tag %p",(void*)object);}
        evale(result,environment,target_frame->results);
        NON_LOCAL_EXIT();}

    close_live_pool(pool);
    return NIL_Symbol;}
