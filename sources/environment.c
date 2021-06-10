#include "kernel.h"
#include "environment.h"
#include "boerror.h"

Ref environment_dynamic_variables(Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    return env->dynamic_variables;}

Ref environment_lexical_variables(Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    return env->lexical_variables;}

Ref environment_lexical_functions(Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    return env->lexical_functions;}

Ref environment_lexical_blocks(Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    return env->lexical_blocks;}

Ref environment_lexical_catchs(Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    return env->lexical_catchs;}

Ref environment_lexical_declarations(Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    return env->lexical_declarations;}

Ref environment_next_environment(Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    return env->next_environment;}

Ref environment_find_frame_environment(Frame* frame,Ref environment){
    NOT_IMPLEMENTED_YET();
    return environment;}


Ref environment_frames(Ref environment){
    uword pool=open_live_pool();
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

    
Ref environment_add_dynamic_variable(Ref variable, Ref environment);
Ref environment_add_lexical_variable(Ref variable, Ref environment);
Ref environment_add_lexical_function(Ref function, Ref environment);
Ref environment_add_lexical_block(Ref name, Ref environment);
Ref environment_add_lexical_catch(Ref tag, Ref environment);
Ref environment_add_lexical_declaration(Ref declaration, Ref environment);
Ref environment_find_variable(Ref name, Ref environment);
Ref environment_find_function(Ref name, Ref environment);
Ref environment_find_block(Ref name, Ref environment);
Ref environment_find_catch(Ref tag, Ref environment);

Ref env_variable_value(Ref variable){
    
}


/* Ref env_make_constant_variable(Ref name); */
/* Ref env_make_global_special_variable(Ref name); */
/* Ref env_make_local_special_variable(Ref name); */
/* Ref env_make_lexical_variable(Ref name); */
/* Ref env_make_global_symbol_macro(Ref name); */
/* Ref env_make_local_symbol_macro(Ref name); */
/* Ref env_make_global_function(Ref name); */
/* Ref env_make_local_function(Ref name); */
/* Ref env_make_global_macro(Ref name); */
/* Ref env_make_local_macro(Ref name); */
/* bool env_variablep(Ref object); */
/* bool env_contant_variable_p(Ref object); */
/* bool env_special_variable_p(Ref object); */
/* Ref env_variable_name(Ref variable); */
/* Ref env_variable_value(Ref variable); */
/* Ref env_variable_set_value(Ref new_value, Ref variable); */
/* bool env_symbol_macro_p(Ref object); */
/* Ref env_symbol_macro_name(Ref symbol_macro); */
/* Ref env_symbol_macro_expansion(Ref symbol_macro); */
/* Ref env_symbol_macro_set_expansion(Ref new_expansion, Ref symbol_macro); */
/* bool env_functionp(Ref object); */
/* Ref env_function_name(Ref function); */
/* Ref env_function_lambda_list(Ref function); */
/* Ref env_function_body(Ref function); */
/* Ref env_function_compiler_macro(Ref function); */
/* Ref env_function_set_lambda_list(Ref new_lambda_list, Ref function); */
/* Ref env_function_set_body(Ref new_body, Ref function); */
/* Ref env_function_set_compiler_macro(Ref new_compiler_macro, Ref function); */
/* bool env_macro_function_p(Ref object); */
/* Ref env_macro_name(Ref macro); */
/* Ref env_macro_lambda_list(Ref macro); */
/* Ref env_macro_body(Ref macro); */
/* Ref env_macro_set_lambda_list(Ref new_lambda_list, Ref macro); */
/* Ref env_macro_set_body(Ref new_body, Ref macro); */

/**** THE END ****/
