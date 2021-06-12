#include "kernel.h"
#include "environment.h"
#include "boerror.h"
#include "cons.h"
#include "memory.h"
#include "kernel_structure_functions.h"

Ref environment_variables(Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    return env->variables;}

Ref environment_functions(Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    return env->functions;}

Ref environment_blocks(Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    return env->blocks;}

Ref environment_catchs(Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    return env->catchs;}

Ref environment_declarations(Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    return env->declarations;}

Ref environment_next_environment(Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    return env->next_environment;}

Ref environment_find_frame_environment(Frame* frame,Ref environment){
    NOT_IMPLEMENTED_YET();
    return environment;}


Ref environment_frames(Ref environment){
    uword pool=open_live_pool();
    add_to_live_pool(environment);
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    Ref results=NIL_Symbol;
    Frame* frame=env->frames;
    /*
    Build a list of frames: 
    ((unwind-protect environment)
    (dynamic-bindings environment)
    (block environment name)
    (catch environment object)
    (handler environment conditions)
    (restart environment restart))
    */
    while(frame){
        Ref frame_env=environment_find_frame_environment(frame,environment);
        switch(frame->type){
          case frame_protect:
              push(list(UNWIND_PROTECT_Symbol,frame_env,NULL),&results);
              break;
          case frame_dynamic_binding:
              push(list(S(DYNAMIC-BINDING),frame_env,NULL),&results);
              break;
          case frame_block:
              push(list(BLOCK_Symbol,frame_env,frame->parameters.block.name,NULL),&results);
              break;
          case frame_catch:
              push(list(CATCH_Symbol,frame_env,frame->parameters.catch.object,NULL),&results);
              break;
          case frame_handler:
              push(list(S(HANDLER),frame_env,frame->parameters.handler.conditions,NULL),&results);
              break;
          case frame_restart:
              push(list(S(RESTART),frame_env,frame->parameters.restart.restart,NULL),&results);
              break;}
        frame=frame->next;}
    results=nreverse(results);
    close_live_pool(pool);
    return results;}

 
Ref environment_add_variable(Ref variable, Ref environment){
    uword pool=open_live_pool();    
    add_to_live_pool(environment);
    check_class(environment,ENVIRONMENT_Class);
    push(variable,&environment->slots->environment.variables);
    close_live_pool(pool);
    return environment;}

Ref environment_add_function(Ref function, Ref environment){
    uword pool=open_live_pool();    
    add_to_live_pool(environment);
    check_class(environment,ENVIRONMENT_Class);
    push(function,&environment->slots->environment.functions);
    close_live_pool(pool);
    return environment;}

Ref environment_add_block(Ref name, Ref environment){
    uword pool=open_live_pool();    
    add_to_live_pool(environment);
    check_class(environment,ENVIRONMENT_Class);
    push(name,&environment->slots->environment.blocks);
    close_live_pool(pool);
    return environment;}

Ref environment_add_catch(Ref tag, Ref environment){
    uword pool=open_live_pool();    
    add_to_live_pool(environment);
    check_class(environment,ENVIRONMENT_Class);
    push(tag,&environment->slots->environment.catchs);
    close_live_pool(pool);
    return environment;}

Ref environment_add_declaration(Ref declaration, Ref environment){
    uword pool=open_live_pool();    
    add_to_live_pool(environment);
    check_class(environment,ENVIRONMENT_Class);
    push(declaration,&environment->slots->environment.declarations);
    close_live_pool(pool);
    return environment;}

Ref environment_find_variable(Ref name, Ref environment){
}
Ref environment_find_function(Ref name, Ref environment){
}
Ref environment_find_block(Ref name, Ref environment){
}
Ref environment_find_catch(Ref tag, Ref environment){
}


bool env_special_variable_p(Ref object){
    return env_global_special_variable_p(object)
            || env_local_special_variable_p(object);}

Ref env_variable_value(Ref variable){
    if(env_lexical_variable_p(variable)){
        return env_lexical_variable_name(variable);}
    if(env_local_special_variable_p(variable)){
        return env_local_special_variable_name(variable);}
    if(env_global_special_variable_p(variable)){
        return env_global_special_variable_name(variable);}
    if(env_constant_variable_p(variable)){
        return env_constant_variable_name(variable);}
    TYPE_ERROR(variable,LEXICAL_VARIABLE_Class);}

/**** THE END ****/
