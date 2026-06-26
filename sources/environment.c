#include "kernel.h"
#include "environment.h"
#include "boerror.h"
#include "cons.h"
#include "memory.h"
#include "kernel_structure_functions.h"


bool environment_is_toplevel(Ref environment){
    check_class(environment,ENVIRONMENT_Class);
    Environment* env=&(environment->slots->environment);
    return env->is_toplevel;}

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

static Ref environment_find_frame_environment(Frame* frame,Ref environment){
    (void)frame;   /* stub: not implemented yet */
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
          case frame_tagbody:
              push(list(TAGBODY_Symbol,frame_env,firsts(frame->parameters.tagbody.tags),NULL),&results);
              break;
          case frame_handler:
              push(list(S(HANDLER),frame_env,frame->parameters.handler.clauses,NULL),&results);
              break;
          case frame_handler_bind:
          case frame_handler_bind_inactive:
              push(list(S(HANDLER-BIND),frame_env,frame->parameters.handler_bind.bindings,NULL),&results);
              break;
          case frame_restart:
              push(list(S(RESTART),frame_env,frame->parameters.restart.restarts,NULL),&results);
              break;
          default: break;}
        frame=frame->next;}
    results=nreverse(results);
    close_live_pool(pool);
    add_to_live_pool(results);   /* re-root: close_live_pool trimmed the fresh list */
    return results;}

 
/* In each of these, the item being pushed must be rooted across push's cons
   (push X = X-list = cons(X, X-list)); X is a fresh object the caller just made
   and has not otherwise rooted. */
Ref environment_add_variable(Ref variable, Ref environment){
    uword pool=open_live_pool();
    add_to_live_pool(environment);
    add_to_live_pool(variable);
    check_class(environment,ENVIRONMENT_Class);
    push(variable,&environment->slots->environment.variables);
    gc_write_barrier(environment->slots);   /* the var-list head lives in the env slots block */
    close_live_pool(pool);
    return environment;}

Ref environment_add_function(Ref function, Ref environment){
    uword pool=open_live_pool();
    add_to_live_pool(environment);
    add_to_live_pool(function);
    check_class(environment,ENVIRONMENT_Class);
    push(function,&environment->slots->environment.functions);
    gc_write_barrier(environment->slots);
    close_live_pool(pool);
    return environment;}

Ref environment_add_block(Ref name, Ref environment){
    uword pool=open_live_pool();
    add_to_live_pool(environment);
    add_to_live_pool(name);
    check_class(environment,ENVIRONMENT_Class);
    push(name,&environment->slots->environment.blocks);
    gc_write_barrier(environment->slots);
    close_live_pool(pool);
    return environment;}

Ref environment_add_catch(Ref tag, Ref environment){
    uword pool=open_live_pool();
    add_to_live_pool(environment);
    add_to_live_pool(tag);
    check_class(environment,ENVIRONMENT_Class);
    push(tag,&environment->slots->environment.catchs);
    gc_write_barrier(environment->slots);
    close_live_pool(pool);
    return environment;}

Ref environment_add_declaration(Ref declaration, Ref environment){
    uword pool=open_live_pool();
    add_to_live_pool(environment);
    add_to_live_pool(declaration);
    check_class(environment,ENVIRONMENT_Class);
    push(declaration,&environment->slots->environment.declarations);
    gc_write_barrier(environment->slots);
    close_live_pool(pool);
    return environment;}

/* Read the name slot of any env variable wrapper. Symbol-macro wrappers
   (SYMBOL-MACROLET / DEFINE-SYMBOL-MACRO) also live in the variable
   namespace — evale checks them before treating a symbol as an ordinary
   variable lookup. */
static Ref env_variable_wrapper_name(Ref variable){
    if(env_lexical_variable_p(variable))       { return env_lexical_variable_name(variable); }
    if(env_local_special_variable_p(variable)) { return env_local_special_variable_name(variable); }
    if(env_global_special_variable_p(variable)){ return env_global_special_variable_name(variable); }
    if(env_constant_variable_p(variable))      { return env_constant_variable_name(variable); }
    if(env_local_symbol_macro_p(variable))     { return env_local_symbol_macro_name(variable); }
    if(env_global_symbol_macro_p(variable))    { return env_global_symbol_macro_name(variable); }
    TYPE_ERROR(variable,LEXICAL_VARIABLE_Class);}

/* Read the name slot of any env function wrapper (global or local fn, global or local macro). */
static Ref env_function_wrapper_name(Ref function){
    if(env_local_function_p(function))      { return env_local_function_name(function); }
    if(env_global_function_p(function))     { return env_global_function_name(function); }
    if(env_local_macro_p(function))         { return env_local_macro_name(function); }
    if(env_global_macro_p(function))        { return env_global_macro_name(function); }
    if(env_local_symbol_macro_p(function))  { return env_local_symbol_macro_name(function); }
    if(env_global_symbol_macro_p(function)) { return env_global_symbol_macro_name(function); }
    TYPE_ERROR(function,LOCAL_FUNCTION_Class);}

Ref environment_find_variable(Ref name, Ref environment){
    while(!nullp(environment) && typep(environment,ENVIRONMENT_Class)){
        Environment* env=&(environment->slots->environment);
        Ref vars=env->variables;
        while(consp(vars)){
            Ref var=cons_car(vars);
            if(eql(name,env_variable_wrapper_name(var))){
                return var;}
            vars=cons_cdr(vars);}
        environment=env->next_environment;}
    return NIL_Symbol;}

Ref environment_find_function(Ref name, Ref environment){
    while(!nullp(environment) && typep(environment,ENVIRONMENT_Class)){
        Environment* env=&(environment->slots->environment);
        Ref funs=env->functions;
        while(consp(funs)){
            Ref fun=cons_car(funs);
            if(eql(name,env_function_wrapper_name(fun))){
                return fun;}
            funs=cons_cdr(funs);}
        environment=env->next_environment;}
    return NIL_Symbol;}

Ref environment_find_block(Ref name, Ref environment){
    while(!nullp(environment) && typep(environment,ENVIRONMENT_Class)){
        Environment* env=&(environment->slots->environment);
        if(!nullp(memq(name,env->blocks))){
            return name;}
        environment=env->next_environment;}
    return NIL_Symbol;}

Ref environment_find_catch(Ref tag, Ref environment){
    while(!nullp(environment) && typep(environment,ENVIRONMENT_Class)){
        Environment* env=&(environment->slots->environment);
        if(!nullp(memq(tag,env->catchs))){
            return tag;}
        environment=env->next_environment;}
    return NIL_Symbol;}


bool env_special_variable_p(Ref object){
    return env_global_special_variable_p(object)
            || env_local_special_variable_p(object);}

Ref env_variable_value(Ref variable){
    if(env_lexical_variable_p(variable))       { return env_lexical_variable_value(variable); }
    if(env_local_special_variable_p(variable)) {
        /* Local-special is a dynamic binding: its current value is in the symbol's value cell. */
        return symbol_value(env_local_special_variable_name(variable)); }
    if(env_global_special_variable_p(variable)){ return env_global_special_variable_value(variable); }
    if(env_constant_variable_p(variable))      { return env_constant_variable_value(variable); }
    TYPE_ERROR(variable,LEXICAL_VARIABLE_Class);}

/* (name . value) alist of ENVIRONMENT's OWN bound variables (its direct level
   only — i.e. a function's parameters), in source/left-to-right order.  Symbol-macro
   wrappers are skipped (they have no value cell).  Used by the debugger backtrace to
   show a call frame's parameters and their current values. */
Ref environment_local_bindings(Ref environment){
    if(!typep(environment,ENVIRONMENT_Class)){ return NIL_Symbol; }
    uword pool=open_live_pool();
    add_to_live_pool(environment);
    Ref out=NIL_Symbol;
    Ref vars=environment->slots->environment.variables;
    while(consp(vars)){
        Ref var=cons_car(vars);
        if(env_lexical_variable_p(var)
           || env_local_special_variable_p(var)
           || env_global_special_variable_p(var)
           || env_constant_variable_p(var)){
            Ref pair=cons(env_variable_wrapper_name(var), env_variable_value(var));
            add_to_live_pool(pair);
            push(pair,&out);
            add_to_live_pool(out);}
        vars=cons_cdr(vars);}
    /* variables are prepended as bound, so OUT is already in source order. */
    close_live_pool(pool);
    add_to_live_pool(out);
    return out;}

Ref env_set_variable_value(Ref new_value, Ref variable){
    if(env_lexical_variable_p(variable)){
        set_env_lexical_variable_value(new_value,variable);
        return new_value;}
    if(env_local_special_variable_p(variable)){
        set_symbol_value(env_local_special_variable_name(variable),new_value);
        return new_value;}
    if(env_global_special_variable_p(variable)){
        set_env_global_special_variable_value(new_value,variable);
        /* Also update the symbol's value cell so dynamic-binding lookups stay consistent. */
        set_symbol_value(env_global_special_variable_name(variable),new_value);
        return new_value;}
    if(env_constant_variable_p(variable)){
        ERROR("Cannot assign to constant variable %s",
              string_cstring(symbol_name(env_constant_variable_name(variable))));}
    TYPE_ERROR(variable,LEXICAL_VARIABLE_Class);}

/**** THE END ****/
