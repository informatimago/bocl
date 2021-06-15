#include "structure_functions.h"
#include "kernel_structure_functions.h"
#include "kernel.h"
#include "cons.h"
#include "variable.h"
#include "stream.h"
#include "reader.h"
#include "printer.h"

void kernel_ENV_LEXICAL_VARIABLE_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(env_lexical_variable_p(par_object));
    close_live_pool(pool);
}

void kernel_ENV_MAKE_LEXICAL_VARIABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_name = variable_value(S(NIL));
    Ref par_name = pop_key_argument(function,&current,K(NAME),def_name);
    Ref def_value = variable_value(S(NIL));
    Ref par_value = pop_key_argument(function,&current,K(VALUE),def_value);
    results->count=1;
    results->values[0] = env_make_lexical_variable(par_name, par_value);
    close_live_pool(pool);
}

void kernel_ENV_LEXICAL_VARIABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_lexical_variable(par_name);
    close_live_pool(pool);
}

void kernel_COPY_LEXICAL_VARIABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_lexical_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = copy_lexical_variable(par_lexical_variable);
    close_live_pool(pool);
}

void kernel_ENV_LEXICAL_VARIABLE_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_lexical_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_lexical_variable_name(par_lexical_variable);
    close_live_pool(pool);
}

void kernel_SET_ENV_LEXICAL_VARIABLE_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_name = pop_mandatory_argument(function,&current);
    Ref par_lexical_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_lexical_variable_name(par_new_name, par_lexical_variable);
    close_live_pool(pool);
}

void kernel_ENV_LEXICAL_VARIABLE_VALUE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_lexical_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_lexical_variable_value(par_lexical_variable);
    close_live_pool(pool);
}

void kernel_SET_ENV_LEXICAL_VARIABLE_VALUE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_value = pop_mandatory_argument(function,&current);
    Ref par_lexical_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_lexical_variable_value(par_new_value, par_lexical_variable);
    close_live_pool(pool);
}

void kernel_ENV_CONSTANT_VARIABLE_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(env_constant_variable_p(par_object));
    close_live_pool(pool);
}

void kernel_ENV_MAKE_CONSTANT_VARIABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_name = variable_value(S(NIL));
    Ref par_name = pop_key_argument(function,&current,K(NAME),def_name);
    Ref def_value = variable_value(S(NIL));
    Ref par_value = pop_key_argument(function,&current,K(VALUE),def_value);
    results->count=1;
    results->values[0] = env_make_constant_variable(par_name, par_value);
    close_live_pool(pool);
}

void kernel_ENV_CONSTANT_VARIABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_constant_variable(par_name);
    close_live_pool(pool);
}

void kernel_COPY_CONSTANT_VARIABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_constant_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = copy_constant_variable(par_constant_variable);
    close_live_pool(pool);
}

void kernel_ENV_CONSTANT_VARIABLE_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_constant_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_constant_variable_name(par_constant_variable);
    close_live_pool(pool);
}

void kernel_SET_ENV_CONSTANT_VARIABLE_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_name = pop_mandatory_argument(function,&current);
    Ref par_constant_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_constant_variable_name(par_new_name, par_constant_variable);
    close_live_pool(pool);
}

void kernel_ENV_CONSTANT_VARIABLE_VALUE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_constant_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_constant_variable_value(par_constant_variable);
    close_live_pool(pool);
}

void kernel_SET_ENV_CONSTANT_VARIABLE_VALUE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_value = pop_mandatory_argument(function,&current);
    Ref par_constant_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_constant_variable_value(par_new_value, par_constant_variable);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_SPECIAL_VARIABLE_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(env_global_special_variable_p(par_object));
    close_live_pool(pool);
}

void kernel_ENV_MAKE_GLOBAL_SPECIAL_VARIABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_name = variable_value(S(NIL));
    Ref par_name = pop_key_argument(function,&current,K(NAME),def_name);
    Ref def_value = variable_value(S(NIL));
    Ref par_value = pop_key_argument(function,&current,K(VALUE),def_value);
    results->count=1;
    results->values[0] = env_make_global_special_variable(par_name, par_value);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_SPECIAL_VARIABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_special_variable(par_name);
    close_live_pool(pool);
}

void kernel_COPY_GLOBAL_SPECIAL_VARIABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_special_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = copy_global_special_variable(par_global_special_variable);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_SPECIAL_VARIABLE_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_special_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_special_variable_name(par_global_special_variable);
    close_live_pool(pool);
}

void kernel_SET_ENV_GLOBAL_SPECIAL_VARIABLE_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_name = pop_mandatory_argument(function,&current);
    Ref par_global_special_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_global_special_variable_name(par_new_name, par_global_special_variable);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_SPECIAL_VARIABLE_VALUE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_special_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_special_variable_value(par_global_special_variable);
    close_live_pool(pool);
}

void kernel_SET_ENV_GLOBAL_SPECIAL_VARIABLE_VALUE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_value = pop_mandatory_argument(function,&current);
    Ref par_global_special_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_global_special_variable_value(par_new_value, par_global_special_variable);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_SPECIAL_VARIABLE_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(env_local_special_variable_p(par_object));
    close_live_pool(pool);
}

void kernel_ENV_MAKE_LOCAL_SPECIAL_VARIABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_name = variable_value(S(NIL));
    Ref par_name = pop_key_argument(function,&current,K(NAME),def_name);
    Ref def_saved_boundp = variable_value(S(NIL));
    Ref par_saved_boundp = pop_key_argument(function,&current,K(SAVED-BOUNDP),def_saved_boundp);
    Ref def_saved_value = variable_value(S(NIL));
    Ref par_saved_value = pop_key_argument(function,&current,K(SAVED-VALUE),def_saved_value);
    results->count=1;
    results->values[0] = env_make_local_special_variable(par_name, par_saved_boundp, par_saved_value);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_SPECIAL_VARIABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_special_variable(par_name);
    close_live_pool(pool);
}

void kernel_COPY_LOCAL_SPECIAL_VARIABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_special_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = copy_local_special_variable(par_local_special_variable);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_SPECIAL_VARIABLE_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_special_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_special_variable_name(par_local_special_variable);
    close_live_pool(pool);
}

void kernel_SET_ENV_LOCAL_SPECIAL_VARIABLE_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_name = pop_mandatory_argument(function,&current);
    Ref par_local_special_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_local_special_variable_name(par_new_name, par_local_special_variable);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_SPECIAL_VARIABLE_SAVED_BOUNDP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_special_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_special_variable_saved_boundp(par_local_special_variable);
    close_live_pool(pool);
}

void kernel_SET_ENV_LOCAL_SPECIAL_VARIABLE_SAVED_BOUNDP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_saved_boundp = pop_mandatory_argument(function,&current);
    Ref par_local_special_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_local_special_variable_saved_boundp(par_new_saved_boundp, par_local_special_variable);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_SPECIAL_VARIABLE_SAVED_VALUE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_special_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_special_variable_saved_value(par_local_special_variable);
    close_live_pool(pool);
}

void kernel_SET_ENV_LOCAL_SPECIAL_VARIABLE_SAVED_VALUE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_saved_value = pop_mandatory_argument(function,&current);
    Ref par_local_special_variable = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_local_special_variable_saved_value(par_new_saved_value, par_local_special_variable);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_FUNCTION_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(env_global_function_p(par_object));
    close_live_pool(pool);
}

void kernel_ENV_MAKE_GLOBAL_FUNCTION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_name = variable_value(S(NIL));
    Ref par_name = pop_key_argument(function,&current,K(NAME),def_name);
    Ref def_lambda_list = variable_value(S(NIL));
    Ref par_lambda_list = pop_key_argument(function,&current,K(LAMBDA-LIST),def_lambda_list);
    Ref def_body = variable_value(S(NIL));
    Ref par_body = pop_key_argument(function,&current,K(BODY),def_body);
    Ref def_compiler_macro = variable_value(S(NIL));
    Ref par_compiler_macro = pop_key_argument(function,&current,K(COMPILER-MACRO),def_compiler_macro);
    results->count=1;
    results->values[0] = env_make_global_function(par_name, par_lambda_list, par_body, par_compiler_macro);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_FUNCTION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_function(par_name);
    close_live_pool(pool);
}

void kernel_COPY_GLOBAL_FUNCTION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = copy_global_function(par_global_function);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_FUNCTION_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_function_name(par_global_function);
    close_live_pool(pool);
}

void kernel_SET_ENV_GLOBAL_FUNCTION_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_name = pop_mandatory_argument(function,&current);
    Ref par_global_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_global_function_name(par_new_name, par_global_function);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_FUNCTION_LAMBDA_LIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_function_lambda_list(par_global_function);
    close_live_pool(pool);
}

void kernel_SET_ENV_GLOBAL_FUNCTION_LAMBDA_LIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_lambda_list = pop_mandatory_argument(function,&current);
    Ref par_global_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_global_function_lambda_list(par_new_lambda_list, par_global_function);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_FUNCTION_BODY(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_function_body(par_global_function);
    close_live_pool(pool);
}

void kernel_SET_ENV_GLOBAL_FUNCTION_BODY(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_body = pop_mandatory_argument(function,&current);
    Ref par_global_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_global_function_body(par_new_body, par_global_function);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_FUNCTION_COMPILER_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_function_compiler_macro(par_global_function);
    close_live_pool(pool);
}

void kernel_SET_ENV_GLOBAL_FUNCTION_COMPILER_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_compiler_macro = pop_mandatory_argument(function,&current);
    Ref par_global_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_global_function_compiler_macro(par_new_compiler_macro, par_global_function);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_FUNCTION_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(env_local_function_p(par_object));
    close_live_pool(pool);
}

void kernel_ENV_MAKE_LOCAL_FUNCTION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_name = variable_value(S(NIL));
    Ref par_name = pop_key_argument(function,&current,K(NAME),def_name);
    Ref def_lambda_list = variable_value(S(NIL));
    Ref par_lambda_list = pop_key_argument(function,&current,K(LAMBDA-LIST),def_lambda_list);
    Ref def_body = variable_value(S(NIL));
    Ref par_body = pop_key_argument(function,&current,K(BODY),def_body);
    results->count=1;
    results->values[0] = env_make_local_function(par_name, par_lambda_list, par_body);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_FUNCTION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_function(par_name);
    close_live_pool(pool);
}

void kernel_COPY_LOCAL_FUNCTION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = copy_local_function(par_local_function);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_FUNCTION_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_function_name(par_local_function);
    close_live_pool(pool);
}

void kernel_SET_ENV_LOCAL_FUNCTION_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_name = pop_mandatory_argument(function,&current);
    Ref par_local_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_local_function_name(par_new_name, par_local_function);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_FUNCTION_LAMBDA_LIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_function_lambda_list(par_local_function);
    close_live_pool(pool);
}

void kernel_SET_ENV_LOCAL_FUNCTION_LAMBDA_LIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_lambda_list = pop_mandatory_argument(function,&current);
    Ref par_local_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_local_function_lambda_list(par_new_lambda_list, par_local_function);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_FUNCTION_BODY(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_function_body(par_local_function);
    close_live_pool(pool);
}

void kernel_SET_ENV_LOCAL_FUNCTION_BODY(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_body = pop_mandatory_argument(function,&current);
    Ref par_local_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_local_function_body(par_new_body, par_local_function);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_SYMBOL_MACRO_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(env_global_symbol_macro_p(par_object));
    close_live_pool(pool);
}

void kernel_ENV_MAKE_GLOBAL_SYMBOL_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_name = variable_value(S(NIL));
    Ref par_name = pop_key_argument(function,&current,K(NAME),def_name);
    Ref def_expansion = variable_value(S(NIL));
    Ref par_expansion = pop_key_argument(function,&current,K(EXPANSION),def_expansion);
    results->count=1;
    results->values[0] = env_make_global_symbol_macro(par_name, par_expansion);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_SYMBOL_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_symbol_macro(par_name);
    close_live_pool(pool);
}

void kernel_COPY_GLOBAL_SYMBOL_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_symbol_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = copy_global_symbol_macro(par_global_symbol_macro);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_SYMBOL_MACRO_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_symbol_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_symbol_macro_name(par_global_symbol_macro);
    close_live_pool(pool);
}

void kernel_SET_ENV_GLOBAL_SYMBOL_MACRO_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_name = pop_mandatory_argument(function,&current);
    Ref par_global_symbol_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_global_symbol_macro_name(par_new_name, par_global_symbol_macro);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_SYMBOL_MACRO_EXPANSION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_symbol_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_symbol_macro_expansion(par_global_symbol_macro);
    close_live_pool(pool);
}

void kernel_SET_ENV_GLOBAL_SYMBOL_MACRO_EXPANSION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_expansion = pop_mandatory_argument(function,&current);
    Ref par_global_symbol_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_global_symbol_macro_expansion(par_new_expansion, par_global_symbol_macro);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_SYMBOL_MACRO_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(env_local_symbol_macro_p(par_object));
    close_live_pool(pool);
}

void kernel_ENV_MAKE_LOCAL_SYMBOL_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_name = variable_value(S(NIL));
    Ref par_name = pop_key_argument(function,&current,K(NAME),def_name);
    Ref def_expansion = variable_value(S(NIL));
    Ref par_expansion = pop_key_argument(function,&current,K(EXPANSION),def_expansion);
    results->count=1;
    results->values[0] = env_make_local_symbol_macro(par_name, par_expansion);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_SYMBOL_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_symbol_macro(par_name);
    close_live_pool(pool);
}

void kernel_COPY_LOCAL_SYMBOL_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_symbol_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = copy_local_symbol_macro(par_local_symbol_macro);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_SYMBOL_MACRO_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_symbol_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_symbol_macro_name(par_local_symbol_macro);
    close_live_pool(pool);
}

void kernel_SET_ENV_LOCAL_SYMBOL_MACRO_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_name = pop_mandatory_argument(function,&current);
    Ref par_local_symbol_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_local_symbol_macro_name(par_new_name, par_local_symbol_macro);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_SYMBOL_MACRO_EXPANSION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_symbol_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_symbol_macro_expansion(par_local_symbol_macro);
    close_live_pool(pool);
}

void kernel_SET_ENV_LOCAL_SYMBOL_MACRO_EXPANSION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_expansion = pop_mandatory_argument(function,&current);
    Ref par_local_symbol_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_local_symbol_macro_expansion(par_new_expansion, par_local_symbol_macro);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_MACRO_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(env_global_macro_p(par_object));
    close_live_pool(pool);
}

void kernel_ENV_MAKE_GLOBAL_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_name = variable_value(S(NIL));
    Ref par_name = pop_key_argument(function,&current,K(NAME),def_name);
    Ref def_lambda_list = variable_value(S(NIL));
    Ref par_lambda_list = pop_key_argument(function,&current,K(LAMBDA-LIST),def_lambda_list);
    Ref def_body = variable_value(S(NIL));
    Ref par_body = pop_key_argument(function,&current,K(BODY),def_body);
    results->count=1;
    results->values[0] = env_make_global_macro(par_name, par_lambda_list, par_body);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_macro(par_name);
    close_live_pool(pool);
}

void kernel_COPY_GLOBAL_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = copy_global_macro(par_global_macro);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_MACRO_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_macro_name(par_global_macro);
    close_live_pool(pool);
}

void kernel_SET_ENV_GLOBAL_MACRO_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_name = pop_mandatory_argument(function,&current);
    Ref par_global_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_global_macro_name(par_new_name, par_global_macro);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_MACRO_LAMBDA_LIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_macro_lambda_list(par_global_macro);
    close_live_pool(pool);
}

void kernel_SET_ENV_GLOBAL_MACRO_LAMBDA_LIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_lambda_list = pop_mandatory_argument(function,&current);
    Ref par_global_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_global_macro_lambda_list(par_new_lambda_list, par_global_macro);
    close_live_pool(pool);
}

void kernel_ENV_GLOBAL_MACRO_BODY(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_global_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_global_macro_body(par_global_macro);
    close_live_pool(pool);
}

void kernel_SET_ENV_GLOBAL_MACRO_BODY(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_body = pop_mandatory_argument(function,&current);
    Ref par_global_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_global_macro_body(par_new_body, par_global_macro);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_MACRO_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(env_local_macro_p(par_object));
    close_live_pool(pool);
}

void kernel_ENV_MAKE_LOCAL_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_name = variable_value(S(NIL));
    Ref par_name = pop_key_argument(function,&current,K(NAME),def_name);
    Ref def_lambda_list = variable_value(S(NIL));
    Ref par_lambda_list = pop_key_argument(function,&current,K(LAMBDA-LIST),def_lambda_list);
    Ref def_body = variable_value(S(NIL));
    Ref par_body = pop_key_argument(function,&current,K(BODY),def_body);
    results->count=1;
    results->values[0] = env_make_local_macro(par_name, par_lambda_list, par_body);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_macro(par_name);
    close_live_pool(pool);
}

void kernel_COPY_LOCAL_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = copy_local_macro(par_local_macro);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_MACRO_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_macro_name(par_local_macro);
    close_live_pool(pool);
}

void kernel_SET_ENV_LOCAL_MACRO_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_name = pop_mandatory_argument(function,&current);
    Ref par_local_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_local_macro_name(par_new_name, par_local_macro);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_MACRO_LAMBDA_LIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_macro_lambda_list(par_local_macro);
    close_live_pool(pool);
}

void kernel_SET_ENV_LOCAL_MACRO_LAMBDA_LIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_lambda_list = pop_mandatory_argument(function,&current);
    Ref par_local_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_local_macro_lambda_list(par_new_lambda_list, par_local_macro);
    close_live_pool(pool);
}

void kernel_ENV_LOCAL_MACRO_BODY(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_local_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = env_local_macro_body(par_local_macro);
    close_live_pool(pool);
}

void kernel_SET_ENV_LOCAL_MACRO_BODY(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_body = pop_mandatory_argument(function,&current);
    Ref par_local_macro = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_env_local_macro_body(par_new_body, par_local_macro);
    close_live_pool(pool);
}


void kernel_initialize_structure_functions(void){
    LEXICAL_VARIABLE_Class           = make_structure_class(S(LEXICAL_VARIABLE),STRUCTURE_OBJECT_Class,integer_from_uword(0),list(S(NAME),S(VALUE),NULL));
    CONSTANT_VARIABLE_Class          = make_structure_class(S(CONSTANT_VARIABLE),STRUCTURE_OBJECT_Class,integer_from_uword(0),list(S(NAME),S(VALUE),NULL));
    GLOBAL_SPECIAL_VARIABLE_Class    = make_structure_class(S(GLOBAL_SPECIAL_VARIABLE),STRUCTURE_OBJECT_Class,integer_from_uword(0),list(S(NAME),S(VALUE),NULL));
    LOCAL_SPECIAL_VARIABLE_Class     = make_structure_class(S(LOCAL_SPECIAL_VARIABLE),STRUCTURE_OBJECT_Class,integer_from_uword(0),list(S(NAME),S(SAVED-BOUNDP),S(SAVED-VALUE),NULL));
    GLOBAL_FUNCTION_Class            = make_structure_class(S(GLOBAL_FUNCTION),STRUCTURE_OBJECT_Class,integer_from_uword(0),list(S(NAME),S(LAMBDA-LIST),S(BODY),S(COMPILER-MACRO),NULL));
    LOCAL_FUNCTION_Class             = make_structure_class(S(LOCAL_FUNCTION),STRUCTURE_OBJECT_Class,integer_from_uword(0),list(S(NAME),S(LAMBDA-LIST),S(BODY),NULL));
    GLOBAL_SYMBOL_MACRO_Class        = make_structure_class(S(GLOBAL_SYMBOL_MACRO),STRUCTURE_OBJECT_Class,integer_from_uword(0),list(S(NAME),S(EXPANSION),NULL));
    LOCAL_SYMBOL_MACRO_Class         = make_structure_class(S(LOCAL_SYMBOL_MACRO),STRUCTURE_OBJECT_Class,integer_from_uword(0),list(S(NAME),S(EXPANSION),NULL));
    GLOBAL_MACRO_Class               = make_structure_class(S(GLOBAL_MACRO),STRUCTURE_OBJECT_Class,integer_from_uword(0),list(S(NAME),S(LAMBDA-LIST),S(BODY),NULL));
    LOCAL_MACRO_Class                = make_structure_class(S(LOCAL_MACRO),STRUCTURE_OBJECT_Class,integer_from_uword(0),list(S(NAME),S(LAMBDA-LIST),S(BODY),NULL));
}
/**** THE END ****/
