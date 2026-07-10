#include "kernel_structure_functions.h"
#include "structure_functions.h"
#include "kernel.h"

Ref LEXICAL_VARIABLE_Class;
bool env_lexical_variable_p(Ref par_object){
    return typep(par_object,LEXICAL_VARIABLE_Class);}

/* hot-2: LEXICAL-VARIABLE is the dominant wrapper on the variable hot path, and
   its NAME/VALUE slot positions are CONSTANT once the class is defined.  The
   generic accessors resolve them by `assq` (over the class slot list) plus a
   live-pool dance on EVERY read/write — pure waste hot-1 measured as ~96% eval.
   Cache the two slot indices as plain C words (NOT boxed-integer Refs, which in a
   C static would be invisible to the collector — a rooting hazard) and read/write
   the slot vector directly.  Populated lazily on first use; structure_class_
   instance_slots may allocate the first time, so callers holding a fresh unrooted
   object must root it before calling ensure (env_make_lexical_variable does). */
static word lexvar_name_idx  = -1;
static word lexvar_value_idx = -1;
static void ensure_lexvar_indices(void){
    if(lexvar_name_idx < 0){
        Ref slots = structure_class_instance_slots(LEXICAL_VARIABLE_Class);
        Ref ne = assq(S(NAME), slots);
        Ref ve = assq(S(VALUE),slots);
        if(nullp(ne) || nullp(ve)){ FATAL("%s","NAME/VALUE not slots of LEXICAL-VARIABLE"); }
        lexvar_name_idx  = integer_value(cdr(ne));
        lexvar_value_idx = integer_value(cdr(ve));}}

Ref env_make_lexical_variable(Ref par_name, Ref par_value){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    add_to_live_pool(par_value);
    ensure_lexvar_indices();      /* inputs rooted first: ensure may allocate on its first call */
    Ref instance = make_structure_instance(LEXICAL_VARIABLE_Class);
    instance->slots->vector_of_object.elements[lexvar_name_idx]  = par_name;
    instance->slots->vector_of_object.elements[lexvar_value_idx] = par_value;
    gc_write_barrier(instance->slots);
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_lexical_variable(Ref par_name){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    Ref instance = make_structure_instance(LEXICAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(LEXICAL_VARIABLE_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LEXICAL-VARIABLE"); }
        structure_set(par_name,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref copy_lexical_variable(Ref par_lexical_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_lexical_variable_instance);
    Ref instance = make_structure_instance(LEXICAL_VARIABLE_Class);
    Ref size = structure_class_instance_size(LEXICAL_VARIABLE_Class);
    for(word i = integer_value(size)-1;0<=i;i--){
        Ref index = integer_from_word(i);
        structure_set(structure_get(par_lexical_variable_instance,index),instance,index);}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_lexical_variable_name(Ref par_lexical_variable_instance){
    check_type(par_lexical_variable_instance,LEXICAL_VARIABLE_Class);
    ensure_lexvar_indices();
    return par_lexical_variable_instance->slots->vector_of_object.elements[lexvar_name_idx];}
Ref set_env_lexical_variable_name(Ref par_new_name, Ref par_lexical_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_name);
    add_to_live_pool(par_lexical_variable_instance);
    check_type(par_lexical_variable_instance,LEXICAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(LEXICAL_VARIABLE_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LEXICAL-VARIABLE"); }
    structure_set(par_new_name,par_lexical_variable_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_name;}
Ref env_lexical_variable_value(Ref par_lexical_variable_instance){
    check_type(par_lexical_variable_instance,LEXICAL_VARIABLE_Class);
    ensure_lexvar_indices();
    return par_lexical_variable_instance->slots->vector_of_object.elements[lexvar_value_idx];}
/* Unchecked fast readers: caller guarantees env_lexical_variable_p is true.  The
   indices are always populated here because a variable is made (which runs
   ensure) before it can be read. */
Ref env_lexical_variable_name_fast(Ref inst){
    return inst->slots->vector_of_object.elements[lexvar_name_idx];}
Ref env_lexical_variable_value_fast(Ref inst){
    return inst->slots->vector_of_object.elements[lexvar_value_idx];}
Ref set_env_lexical_variable_value(Ref par_new_value, Ref par_lexical_variable_instance){
    check_type(par_lexical_variable_instance,LEXICAL_VARIABLE_Class);
    ensure_lexvar_indices();
    par_lexical_variable_instance->slots->vector_of_object.elements[lexvar_value_idx]=par_new_value;
    gc_write_barrier(par_lexical_variable_instance->slots);
    return par_new_value;}
Ref CONSTANT_VARIABLE_Class;
bool env_constant_variable_p(Ref par_object){
    return typep(par_object,CONSTANT_VARIABLE_Class);}
Ref env_make_constant_variable(Ref par_name, Ref par_value){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    add_to_live_pool(par_value);
    Ref instance = make_structure_instance(CONSTANT_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(CONSTANT_VARIABLE_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of CONSTANT-VARIABLE"); }
        structure_set(par_name,instance,cdr(entry));}
    {   Ref entry = assq(S(VALUE),pos);
        if(nullp(entry)){ FATAL("%s","VALUE is not a slot-name of CONSTANT-VARIABLE"); }
        structure_set(par_value,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_constant_variable(Ref par_name){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    Ref instance = make_structure_instance(CONSTANT_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(CONSTANT_VARIABLE_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of CONSTANT-VARIABLE"); }
        structure_set(par_name,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref copy_constant_variable(Ref par_constant_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_constant_variable_instance);
    Ref instance = make_structure_instance(CONSTANT_VARIABLE_Class);
    Ref size = structure_class_instance_size(CONSTANT_VARIABLE_Class);
    for(word i = integer_value(size)-1;0<=i;i--){
        Ref index = integer_from_word(i);
        structure_set(structure_get(par_constant_variable_instance,index),instance,index);}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_constant_variable_name(Ref par_constant_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_constant_variable_instance);
    check_type(par_constant_variable_instance,CONSTANT_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(CONSTANT_VARIABLE_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of CONSTANT-VARIABLE"); }
    Ref result=structure_get(par_constant_variable_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_constant_variable_name(Ref par_new_name, Ref par_constant_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_name);
    add_to_live_pool(par_constant_variable_instance);
    check_type(par_constant_variable_instance,CONSTANT_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(CONSTANT_VARIABLE_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of CONSTANT-VARIABLE"); }
    structure_set(par_new_name,par_constant_variable_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_name;}
Ref env_constant_variable_value(Ref par_constant_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_constant_variable_instance);
    check_type(par_constant_variable_instance,CONSTANT_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(CONSTANT_VARIABLE_Class);
    Ref entry = assq(S(VALUE),pos);
    if(nullp(entry)){ FATAL("%s","VALUE is not a slot-name of CONSTANT-VARIABLE"); }
    Ref result=structure_get(par_constant_variable_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_constant_variable_value(Ref par_new_value, Ref par_constant_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_value);
    add_to_live_pool(par_constant_variable_instance);
    check_type(par_constant_variable_instance,CONSTANT_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(CONSTANT_VARIABLE_Class);
    Ref entry = assq(S(VALUE),pos);
    if(nullp(entry)){ FATAL("%s","VALUE is not a slot-name of CONSTANT-VARIABLE"); }
    structure_set(par_new_value,par_constant_variable_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_value;}
Ref GLOBAL_SPECIAL_VARIABLE_Class;
bool env_global_special_variable_p(Ref par_object){
    return typep(par_object,GLOBAL_SPECIAL_VARIABLE_Class);}
Ref env_make_global_special_variable(Ref par_name, Ref par_value){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    add_to_live_pool(par_value);
    Ref instance = make_structure_instance(GLOBAL_SPECIAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_SPECIAL_VARIABLE_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-SPECIAL-VARIABLE"); }
        structure_set(par_name,instance,cdr(entry));}
    {   Ref entry = assq(S(VALUE),pos);
        if(nullp(entry)){ FATAL("%s","VALUE is not a slot-name of GLOBAL-SPECIAL-VARIABLE"); }
        structure_set(par_value,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_global_special_variable(Ref par_name){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    Ref instance = make_structure_instance(GLOBAL_SPECIAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_SPECIAL_VARIABLE_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-SPECIAL-VARIABLE"); }
        structure_set(par_name,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref copy_global_special_variable(Ref par_global_special_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_special_variable_instance);
    Ref instance = make_structure_instance(GLOBAL_SPECIAL_VARIABLE_Class);
    Ref size = structure_class_instance_size(GLOBAL_SPECIAL_VARIABLE_Class);
    for(word i = integer_value(size)-1;0<=i;i--){
        Ref index = integer_from_word(i);
        structure_set(structure_get(par_global_special_variable_instance,index),instance,index);}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_global_special_variable_name(Ref par_global_special_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_special_variable_instance);
    check_type(par_global_special_variable_instance,GLOBAL_SPECIAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_SPECIAL_VARIABLE_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-SPECIAL-VARIABLE"); }
    Ref result=structure_get(par_global_special_variable_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_global_special_variable_name(Ref par_new_name, Ref par_global_special_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_name);
    add_to_live_pool(par_global_special_variable_instance);
    check_type(par_global_special_variable_instance,GLOBAL_SPECIAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_SPECIAL_VARIABLE_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-SPECIAL-VARIABLE"); }
    structure_set(par_new_name,par_global_special_variable_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_name;}
Ref env_global_special_variable_value(Ref par_global_special_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_special_variable_instance);
    check_type(par_global_special_variable_instance,GLOBAL_SPECIAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_SPECIAL_VARIABLE_Class);
    Ref entry = assq(S(VALUE),pos);
    if(nullp(entry)){ FATAL("%s","VALUE is not a slot-name of GLOBAL-SPECIAL-VARIABLE"); }
    Ref result=structure_get(par_global_special_variable_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_global_special_variable_value(Ref par_new_value, Ref par_global_special_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_value);
    add_to_live_pool(par_global_special_variable_instance);
    check_type(par_global_special_variable_instance,GLOBAL_SPECIAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_SPECIAL_VARIABLE_Class);
    Ref entry = assq(S(VALUE),pos);
    if(nullp(entry)){ FATAL("%s","VALUE is not a slot-name of GLOBAL-SPECIAL-VARIABLE"); }
    structure_set(par_new_value,par_global_special_variable_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_value;}
Ref LOCAL_SPECIAL_VARIABLE_Class;
bool env_local_special_variable_p(Ref par_object){
    return typep(par_object,LOCAL_SPECIAL_VARIABLE_Class);}
Ref env_make_local_special_variable(Ref par_name, Ref par_saved_boundp, Ref par_saved_value){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    add_to_live_pool(par_saved_boundp);
    add_to_live_pool(par_saved_value);
    Ref instance = make_structure_instance(LOCAL_SPECIAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(LOCAL_SPECIAL_VARIABLE_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-SPECIAL-VARIABLE"); }
        structure_set(par_name,instance,cdr(entry));}
    {   Ref entry = assq(S(SAVED-BOUNDP),pos);
        if(nullp(entry)){ FATAL("%s","SAVED-BOUNDP is not a slot-name of LOCAL-SPECIAL-VARIABLE"); }
        structure_set(par_saved_boundp,instance,cdr(entry));}
    {   Ref entry = assq(S(SAVED-VALUE),pos);
        if(nullp(entry)){ FATAL("%s","SAVED-VALUE is not a slot-name of LOCAL-SPECIAL-VARIABLE"); }
        structure_set(par_saved_value,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_local_special_variable(Ref par_name){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    Ref instance = make_structure_instance(LOCAL_SPECIAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(LOCAL_SPECIAL_VARIABLE_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-SPECIAL-VARIABLE"); }
        structure_set(par_name,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref copy_local_special_variable(Ref par_local_special_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_special_variable_instance);
    Ref instance = make_structure_instance(LOCAL_SPECIAL_VARIABLE_Class);
    Ref size = structure_class_instance_size(LOCAL_SPECIAL_VARIABLE_Class);
    for(word i = integer_value(size)-1;0<=i;i--){
        Ref index = integer_from_word(i);
        structure_set(structure_get(par_local_special_variable_instance,index),instance,index);}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_local_special_variable_name(Ref par_local_special_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_special_variable_instance);
    check_type(par_local_special_variable_instance,LOCAL_SPECIAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(LOCAL_SPECIAL_VARIABLE_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-SPECIAL-VARIABLE"); }
    Ref result=structure_get(par_local_special_variable_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_local_special_variable_name(Ref par_new_name, Ref par_local_special_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_name);
    add_to_live_pool(par_local_special_variable_instance);
    check_type(par_local_special_variable_instance,LOCAL_SPECIAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(LOCAL_SPECIAL_VARIABLE_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-SPECIAL-VARIABLE"); }
    structure_set(par_new_name,par_local_special_variable_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_name;}
Ref env_local_special_variable_saved_boundp(Ref par_local_special_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_special_variable_instance);
    check_type(par_local_special_variable_instance,LOCAL_SPECIAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(LOCAL_SPECIAL_VARIABLE_Class);
    Ref entry = assq(S(SAVED-BOUNDP),pos);
    if(nullp(entry)){ FATAL("%s","SAVED-BOUNDP is not a slot-name of LOCAL-SPECIAL-VARIABLE"); }
    Ref result=structure_get(par_local_special_variable_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_local_special_variable_saved_boundp(Ref par_new_saved_boundp, Ref par_local_special_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_saved_boundp);
    add_to_live_pool(par_local_special_variable_instance);
    check_type(par_local_special_variable_instance,LOCAL_SPECIAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(LOCAL_SPECIAL_VARIABLE_Class);
    Ref entry = assq(S(SAVED-BOUNDP),pos);
    if(nullp(entry)){ FATAL("%s","SAVED-BOUNDP is not a slot-name of LOCAL-SPECIAL-VARIABLE"); }
    structure_set(par_new_saved_boundp,par_local_special_variable_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_saved_boundp;}
Ref env_local_special_variable_saved_value(Ref par_local_special_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_special_variable_instance);
    check_type(par_local_special_variable_instance,LOCAL_SPECIAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(LOCAL_SPECIAL_VARIABLE_Class);
    Ref entry = assq(S(SAVED-VALUE),pos);
    if(nullp(entry)){ FATAL("%s","SAVED-VALUE is not a slot-name of LOCAL-SPECIAL-VARIABLE"); }
    Ref result=structure_get(par_local_special_variable_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_local_special_variable_saved_value(Ref par_new_saved_value, Ref par_local_special_variable_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_saved_value);
    add_to_live_pool(par_local_special_variable_instance);
    check_type(par_local_special_variable_instance,LOCAL_SPECIAL_VARIABLE_Class);
    Ref pos = structure_class_instance_slots(LOCAL_SPECIAL_VARIABLE_Class);
    Ref entry = assq(S(SAVED-VALUE),pos);
    if(nullp(entry)){ FATAL("%s","SAVED-VALUE is not a slot-name of LOCAL-SPECIAL-VARIABLE"); }
    structure_set(par_new_saved_value,par_local_special_variable_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_saved_value;}
Ref GLOBAL_FUNCTION_Class;
bool env_global_function_p(Ref par_object){
    return typep(par_object,GLOBAL_FUNCTION_Class);}
Ref env_make_global_function(Ref par_name, Ref par_lambda_list, Ref par_body, Ref par_compiler_macro){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    add_to_live_pool(par_lambda_list);
    add_to_live_pool(par_body);
    add_to_live_pool(par_compiler_macro);
    Ref instance = make_structure_instance(GLOBAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_FUNCTION_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-FUNCTION"); }
        structure_set(par_name,instance,cdr(entry));}
    {   Ref entry = assq(S(LAMBDA-LIST),pos);
        if(nullp(entry)){ FATAL("%s","LAMBDA-LIST is not a slot-name of GLOBAL-FUNCTION"); }
        structure_set(par_lambda_list,instance,cdr(entry));}
    {   Ref entry = assq(S(BODY),pos);
        if(nullp(entry)){ FATAL("%s","BODY is not a slot-name of GLOBAL-FUNCTION"); }
        structure_set(par_body,instance,cdr(entry));}
    {   Ref entry = assq(S(COMPILER-MACRO),pos);
        if(nullp(entry)){ FATAL("%s","COMPILER-MACRO is not a slot-name of GLOBAL-FUNCTION"); }
        structure_set(par_compiler_macro,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_global_function(Ref par_name){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    Ref instance = make_structure_instance(GLOBAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_FUNCTION_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-FUNCTION"); }
        structure_set(par_name,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref copy_global_function(Ref par_global_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_function_instance);
    Ref instance = make_structure_instance(GLOBAL_FUNCTION_Class);
    Ref size = structure_class_instance_size(GLOBAL_FUNCTION_Class);
    for(word i = integer_value(size)-1;0<=i;i--){
        Ref index = integer_from_word(i);
        structure_set(structure_get(par_global_function_instance,index),instance,index);}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_global_function_name(Ref par_global_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_function_instance);
    check_type(par_global_function_instance,GLOBAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_FUNCTION_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-FUNCTION"); }
    Ref result=structure_get(par_global_function_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_global_function_name(Ref par_new_name, Ref par_global_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_name);
    add_to_live_pool(par_global_function_instance);
    check_type(par_global_function_instance,GLOBAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_FUNCTION_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-FUNCTION"); }
    structure_set(par_new_name,par_global_function_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_name;}
Ref env_global_function_lambda_list(Ref par_global_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_function_instance);
    check_type(par_global_function_instance,GLOBAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_FUNCTION_Class);
    Ref entry = assq(S(LAMBDA-LIST),pos);
    if(nullp(entry)){ FATAL("%s","LAMBDA-LIST is not a slot-name of GLOBAL-FUNCTION"); }
    Ref result=structure_get(par_global_function_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_global_function_lambda_list(Ref par_new_lambda_list, Ref par_global_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_lambda_list);
    add_to_live_pool(par_global_function_instance);
    check_type(par_global_function_instance,GLOBAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_FUNCTION_Class);
    Ref entry = assq(S(LAMBDA-LIST),pos);
    if(nullp(entry)){ FATAL("%s","LAMBDA-LIST is not a slot-name of GLOBAL-FUNCTION"); }
    structure_set(par_new_lambda_list,par_global_function_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_lambda_list;}
Ref env_global_function_body(Ref par_global_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_function_instance);
    check_type(par_global_function_instance,GLOBAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_FUNCTION_Class);
    Ref entry = assq(S(BODY),pos);
    if(nullp(entry)){ FATAL("%s","BODY is not a slot-name of GLOBAL-FUNCTION"); }
    Ref result=structure_get(par_global_function_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_global_function_body(Ref par_new_body, Ref par_global_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_body);
    add_to_live_pool(par_global_function_instance);
    check_type(par_global_function_instance,GLOBAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_FUNCTION_Class);
    Ref entry = assq(S(BODY),pos);
    if(nullp(entry)){ FATAL("%s","BODY is not a slot-name of GLOBAL-FUNCTION"); }
    structure_set(par_new_body,par_global_function_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_body;}
Ref env_global_function_compiler_macro(Ref par_global_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_function_instance);
    check_type(par_global_function_instance,GLOBAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_FUNCTION_Class);
    Ref entry = assq(S(COMPILER-MACRO),pos);
    if(nullp(entry)){ FATAL("%s","COMPILER-MACRO is not a slot-name of GLOBAL-FUNCTION"); }
    Ref result=structure_get(par_global_function_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_global_function_compiler_macro(Ref par_new_compiler_macro, Ref par_global_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_compiler_macro);
    add_to_live_pool(par_global_function_instance);
    check_type(par_global_function_instance,GLOBAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_FUNCTION_Class);
    Ref entry = assq(S(COMPILER-MACRO),pos);
    if(nullp(entry)){ FATAL("%s","COMPILER-MACRO is not a slot-name of GLOBAL-FUNCTION"); }
    structure_set(par_new_compiler_macro,par_global_function_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_compiler_macro;}
Ref LOCAL_FUNCTION_Class;
bool env_local_function_p(Ref par_object){
    return typep(par_object,LOCAL_FUNCTION_Class);}
Ref env_make_local_function(Ref par_name, Ref par_lambda_list, Ref par_body){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    add_to_live_pool(par_lambda_list);
    add_to_live_pool(par_body);
    Ref instance = make_structure_instance(LOCAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(LOCAL_FUNCTION_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-FUNCTION"); }
        structure_set(par_name,instance,cdr(entry));}
    {   Ref entry = assq(S(LAMBDA-LIST),pos);
        if(nullp(entry)){ FATAL("%s","LAMBDA-LIST is not a slot-name of LOCAL-FUNCTION"); }
        structure_set(par_lambda_list,instance,cdr(entry));}
    {   Ref entry = assq(S(BODY),pos);
        if(nullp(entry)){ FATAL("%s","BODY is not a slot-name of LOCAL-FUNCTION"); }
        structure_set(par_body,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_local_function(Ref par_name){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    Ref instance = make_structure_instance(LOCAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(LOCAL_FUNCTION_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-FUNCTION"); }
        structure_set(par_name,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref copy_local_function(Ref par_local_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_function_instance);
    Ref instance = make_structure_instance(LOCAL_FUNCTION_Class);
    Ref size = structure_class_instance_size(LOCAL_FUNCTION_Class);
    for(word i = integer_value(size)-1;0<=i;i--){
        Ref index = integer_from_word(i);
        structure_set(structure_get(par_local_function_instance,index),instance,index);}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_local_function_name(Ref par_local_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_function_instance);
    check_type(par_local_function_instance,LOCAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(LOCAL_FUNCTION_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-FUNCTION"); }
    Ref result=structure_get(par_local_function_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_local_function_name(Ref par_new_name, Ref par_local_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_name);
    add_to_live_pool(par_local_function_instance);
    check_type(par_local_function_instance,LOCAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(LOCAL_FUNCTION_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-FUNCTION"); }
    structure_set(par_new_name,par_local_function_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_name;}
Ref env_local_function_lambda_list(Ref par_local_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_function_instance);
    check_type(par_local_function_instance,LOCAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(LOCAL_FUNCTION_Class);
    Ref entry = assq(S(LAMBDA-LIST),pos);
    if(nullp(entry)){ FATAL("%s","LAMBDA-LIST is not a slot-name of LOCAL-FUNCTION"); }
    Ref result=structure_get(par_local_function_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_local_function_lambda_list(Ref par_new_lambda_list, Ref par_local_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_lambda_list);
    add_to_live_pool(par_local_function_instance);
    check_type(par_local_function_instance,LOCAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(LOCAL_FUNCTION_Class);
    Ref entry = assq(S(LAMBDA-LIST),pos);
    if(nullp(entry)){ FATAL("%s","LAMBDA-LIST is not a slot-name of LOCAL-FUNCTION"); }
    structure_set(par_new_lambda_list,par_local_function_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_lambda_list;}
Ref env_local_function_body(Ref par_local_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_function_instance);
    check_type(par_local_function_instance,LOCAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(LOCAL_FUNCTION_Class);
    Ref entry = assq(S(BODY),pos);
    if(nullp(entry)){ FATAL("%s","BODY is not a slot-name of LOCAL-FUNCTION"); }
    Ref result=structure_get(par_local_function_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_local_function_body(Ref par_new_body, Ref par_local_function_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_body);
    add_to_live_pool(par_local_function_instance);
    check_type(par_local_function_instance,LOCAL_FUNCTION_Class);
    Ref pos = structure_class_instance_slots(LOCAL_FUNCTION_Class);
    Ref entry = assq(S(BODY),pos);
    if(nullp(entry)){ FATAL("%s","BODY is not a slot-name of LOCAL-FUNCTION"); }
    structure_set(par_new_body,par_local_function_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_body;}
Ref GLOBAL_SYMBOL_MACRO_Class;
bool env_global_symbol_macro_p(Ref par_object){
    return typep(par_object,GLOBAL_SYMBOL_MACRO_Class);}
Ref env_make_global_symbol_macro(Ref par_name, Ref par_expansion){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    add_to_live_pool(par_expansion);
    Ref instance = make_structure_instance(GLOBAL_SYMBOL_MACRO_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_SYMBOL_MACRO_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-SYMBOL-MACRO"); }
        structure_set(par_name,instance,cdr(entry));}
    {   Ref entry = assq(S(EXPANSION),pos);
        if(nullp(entry)){ FATAL("%s","EXPANSION is not a slot-name of GLOBAL-SYMBOL-MACRO"); }
        structure_set(par_expansion,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_global_symbol_macro(Ref par_name){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    Ref instance = make_structure_instance(GLOBAL_SYMBOL_MACRO_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_SYMBOL_MACRO_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-SYMBOL-MACRO"); }
        structure_set(par_name,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref copy_global_symbol_macro(Ref par_global_symbol_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_symbol_macro_instance);
    Ref instance = make_structure_instance(GLOBAL_SYMBOL_MACRO_Class);
    Ref size = structure_class_instance_size(GLOBAL_SYMBOL_MACRO_Class);
    for(word i = integer_value(size)-1;0<=i;i--){
        Ref index = integer_from_word(i);
        structure_set(structure_get(par_global_symbol_macro_instance,index),instance,index);}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_global_symbol_macro_name(Ref par_global_symbol_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_symbol_macro_instance);
    check_type(par_global_symbol_macro_instance,GLOBAL_SYMBOL_MACRO_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_SYMBOL_MACRO_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-SYMBOL-MACRO"); }
    Ref result=structure_get(par_global_symbol_macro_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_global_symbol_macro_name(Ref par_new_name, Ref par_global_symbol_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_name);
    add_to_live_pool(par_global_symbol_macro_instance);
    check_type(par_global_symbol_macro_instance,GLOBAL_SYMBOL_MACRO_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_SYMBOL_MACRO_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-SYMBOL-MACRO"); }
    structure_set(par_new_name,par_global_symbol_macro_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_name;}
Ref env_global_symbol_macro_expansion(Ref par_global_symbol_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_symbol_macro_instance);
    check_type(par_global_symbol_macro_instance,GLOBAL_SYMBOL_MACRO_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_SYMBOL_MACRO_Class);
    Ref entry = assq(S(EXPANSION),pos);
    if(nullp(entry)){ FATAL("%s","EXPANSION is not a slot-name of GLOBAL-SYMBOL-MACRO"); }
    Ref result=structure_get(par_global_symbol_macro_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_global_symbol_macro_expansion(Ref par_new_expansion, Ref par_global_symbol_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_expansion);
    add_to_live_pool(par_global_symbol_macro_instance);
    check_type(par_global_symbol_macro_instance,GLOBAL_SYMBOL_MACRO_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_SYMBOL_MACRO_Class);
    Ref entry = assq(S(EXPANSION),pos);
    if(nullp(entry)){ FATAL("%s","EXPANSION is not a slot-name of GLOBAL-SYMBOL-MACRO"); }
    structure_set(par_new_expansion,par_global_symbol_macro_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_expansion;}
Ref LOCAL_SYMBOL_MACRO_Class;
bool env_local_symbol_macro_p(Ref par_object){
    return typep(par_object,LOCAL_SYMBOL_MACRO_Class);}
Ref env_make_local_symbol_macro(Ref par_name, Ref par_expansion){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    add_to_live_pool(par_expansion);
    Ref instance = make_structure_instance(LOCAL_SYMBOL_MACRO_Class);
    Ref pos = structure_class_instance_slots(LOCAL_SYMBOL_MACRO_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-SYMBOL-MACRO"); }
        structure_set(par_name,instance,cdr(entry));}
    {   Ref entry = assq(S(EXPANSION),pos);
        if(nullp(entry)){ FATAL("%s","EXPANSION is not a slot-name of LOCAL-SYMBOL-MACRO"); }
        structure_set(par_expansion,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_local_symbol_macro(Ref par_name){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    Ref instance = make_structure_instance(LOCAL_SYMBOL_MACRO_Class);
    Ref pos = structure_class_instance_slots(LOCAL_SYMBOL_MACRO_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-SYMBOL-MACRO"); }
        structure_set(par_name,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref copy_local_symbol_macro(Ref par_local_symbol_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_symbol_macro_instance);
    Ref instance = make_structure_instance(LOCAL_SYMBOL_MACRO_Class);
    Ref size = structure_class_instance_size(LOCAL_SYMBOL_MACRO_Class);
    for(word i = integer_value(size)-1;0<=i;i--){
        Ref index = integer_from_word(i);
        structure_set(structure_get(par_local_symbol_macro_instance,index),instance,index);}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_local_symbol_macro_name(Ref par_local_symbol_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_symbol_macro_instance);
    check_type(par_local_symbol_macro_instance,LOCAL_SYMBOL_MACRO_Class);
    Ref pos = structure_class_instance_slots(LOCAL_SYMBOL_MACRO_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-SYMBOL-MACRO"); }
    Ref result=structure_get(par_local_symbol_macro_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_local_symbol_macro_name(Ref par_new_name, Ref par_local_symbol_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_name);
    add_to_live_pool(par_local_symbol_macro_instance);
    check_type(par_local_symbol_macro_instance,LOCAL_SYMBOL_MACRO_Class);
    Ref pos = structure_class_instance_slots(LOCAL_SYMBOL_MACRO_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-SYMBOL-MACRO"); }
    structure_set(par_new_name,par_local_symbol_macro_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_name;}
Ref env_local_symbol_macro_expansion(Ref par_local_symbol_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_symbol_macro_instance);
    check_type(par_local_symbol_macro_instance,LOCAL_SYMBOL_MACRO_Class);
    Ref pos = structure_class_instance_slots(LOCAL_SYMBOL_MACRO_Class);
    Ref entry = assq(S(EXPANSION),pos);
    if(nullp(entry)){ FATAL("%s","EXPANSION is not a slot-name of LOCAL-SYMBOL-MACRO"); }
    Ref result=structure_get(par_local_symbol_macro_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_local_symbol_macro_expansion(Ref par_new_expansion, Ref par_local_symbol_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_expansion);
    add_to_live_pool(par_local_symbol_macro_instance);
    check_type(par_local_symbol_macro_instance,LOCAL_SYMBOL_MACRO_Class);
    Ref pos = structure_class_instance_slots(LOCAL_SYMBOL_MACRO_Class);
    Ref entry = assq(S(EXPANSION),pos);
    if(nullp(entry)){ FATAL("%s","EXPANSION is not a slot-name of LOCAL-SYMBOL-MACRO"); }
    structure_set(par_new_expansion,par_local_symbol_macro_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_expansion;}
Ref GLOBAL_MACRO_Class;
bool env_global_macro_p(Ref par_object){
    return typep(par_object,GLOBAL_MACRO_Class);}
Ref env_make_global_macro(Ref par_name, Ref par_lambda_list, Ref par_body){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    add_to_live_pool(par_lambda_list);
    add_to_live_pool(par_body);
    Ref instance = make_structure_instance(GLOBAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_MACRO_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-MACRO"); }
        structure_set(par_name,instance,cdr(entry));}
    {   Ref entry = assq(S(LAMBDA-LIST),pos);
        if(nullp(entry)){ FATAL("%s","LAMBDA-LIST is not a slot-name of GLOBAL-MACRO"); }
        structure_set(par_lambda_list,instance,cdr(entry));}
    {   Ref entry = assq(S(BODY),pos);
        if(nullp(entry)){ FATAL("%s","BODY is not a slot-name of GLOBAL-MACRO"); }
        structure_set(par_body,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_global_macro(Ref par_name){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    Ref instance = make_structure_instance(GLOBAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_MACRO_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-MACRO"); }
        structure_set(par_name,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref copy_global_macro(Ref par_global_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_macro_instance);
    Ref instance = make_structure_instance(GLOBAL_MACRO_Class);
    Ref size = structure_class_instance_size(GLOBAL_MACRO_Class);
    for(word i = integer_value(size)-1;0<=i;i--){
        Ref index = integer_from_word(i);
        structure_set(structure_get(par_global_macro_instance,index),instance,index);}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_global_macro_name(Ref par_global_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_macro_instance);
    check_type(par_global_macro_instance,GLOBAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_MACRO_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-MACRO"); }
    Ref result=structure_get(par_global_macro_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_global_macro_name(Ref par_new_name, Ref par_global_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_name);
    add_to_live_pool(par_global_macro_instance);
    check_type(par_global_macro_instance,GLOBAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_MACRO_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of GLOBAL-MACRO"); }
    structure_set(par_new_name,par_global_macro_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_name;}
Ref env_global_macro_lambda_list(Ref par_global_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_macro_instance);
    check_type(par_global_macro_instance,GLOBAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_MACRO_Class);
    Ref entry = assq(S(LAMBDA-LIST),pos);
    if(nullp(entry)){ FATAL("%s","LAMBDA-LIST is not a slot-name of GLOBAL-MACRO"); }
    Ref result=structure_get(par_global_macro_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_global_macro_lambda_list(Ref par_new_lambda_list, Ref par_global_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_lambda_list);
    add_to_live_pool(par_global_macro_instance);
    check_type(par_global_macro_instance,GLOBAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_MACRO_Class);
    Ref entry = assq(S(LAMBDA-LIST),pos);
    if(nullp(entry)){ FATAL("%s","LAMBDA-LIST is not a slot-name of GLOBAL-MACRO"); }
    structure_set(par_new_lambda_list,par_global_macro_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_lambda_list;}
Ref env_global_macro_body(Ref par_global_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_global_macro_instance);
    check_type(par_global_macro_instance,GLOBAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_MACRO_Class);
    Ref entry = assq(S(BODY),pos);
    if(nullp(entry)){ FATAL("%s","BODY is not a slot-name of GLOBAL-MACRO"); }
    Ref result=structure_get(par_global_macro_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_global_macro_body(Ref par_new_body, Ref par_global_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_body);
    add_to_live_pool(par_global_macro_instance);
    check_type(par_global_macro_instance,GLOBAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(GLOBAL_MACRO_Class);
    Ref entry = assq(S(BODY),pos);
    if(nullp(entry)){ FATAL("%s","BODY is not a slot-name of GLOBAL-MACRO"); }
    structure_set(par_new_body,par_global_macro_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_body;}
Ref LOCAL_MACRO_Class;
bool env_local_macro_p(Ref par_object){
    return typep(par_object,LOCAL_MACRO_Class);}
Ref env_make_local_macro(Ref par_name, Ref par_lambda_list, Ref par_body){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    add_to_live_pool(par_lambda_list);
    add_to_live_pool(par_body);
    Ref instance = make_structure_instance(LOCAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(LOCAL_MACRO_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-MACRO"); }
        structure_set(par_name,instance,cdr(entry));}
    {   Ref entry = assq(S(LAMBDA-LIST),pos);
        if(nullp(entry)){ FATAL("%s","LAMBDA-LIST is not a slot-name of LOCAL-MACRO"); }
        structure_set(par_lambda_list,instance,cdr(entry));}
    {   Ref entry = assq(S(BODY),pos);
        if(nullp(entry)){ FATAL("%s","BODY is not a slot-name of LOCAL-MACRO"); }
        structure_set(par_body,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_local_macro(Ref par_name){
    uword pool=open_live_pool();
    add_to_live_pool(par_name);
    Ref instance = make_structure_instance(LOCAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(LOCAL_MACRO_Class);
    {   Ref entry = assq(S(NAME),pos);
        if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-MACRO"); }
        structure_set(par_name,instance,cdr(entry));}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref copy_local_macro(Ref par_local_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_macro_instance);
    Ref instance = make_structure_instance(LOCAL_MACRO_Class);
    Ref size = structure_class_instance_size(LOCAL_MACRO_Class);
    for(word i = integer_value(size)-1;0<=i;i--){
        Ref index = integer_from_word(i);
        structure_set(structure_get(par_local_macro_instance,index),instance,index);}
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}
Ref env_local_macro_name(Ref par_local_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_macro_instance);
    check_type(par_local_macro_instance,LOCAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(LOCAL_MACRO_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-MACRO"); }
    Ref result=structure_get(par_local_macro_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_local_macro_name(Ref par_new_name, Ref par_local_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_name);
    add_to_live_pool(par_local_macro_instance);
    check_type(par_local_macro_instance,LOCAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(LOCAL_MACRO_Class);
    Ref entry = assq(S(NAME),pos);
    if(nullp(entry)){ FATAL("%s","NAME is not a slot-name of LOCAL-MACRO"); }
    structure_set(par_new_name,par_local_macro_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_name;}
Ref env_local_macro_lambda_list(Ref par_local_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_macro_instance);
    check_type(par_local_macro_instance,LOCAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(LOCAL_MACRO_Class);
    Ref entry = assq(S(LAMBDA-LIST),pos);
    if(nullp(entry)){ FATAL("%s","LAMBDA-LIST is not a slot-name of LOCAL-MACRO"); }
    Ref result=structure_get(par_local_macro_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_local_macro_lambda_list(Ref par_new_lambda_list, Ref par_local_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_lambda_list);
    add_to_live_pool(par_local_macro_instance);
    check_type(par_local_macro_instance,LOCAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(LOCAL_MACRO_Class);
    Ref entry = assq(S(LAMBDA-LIST),pos);
    if(nullp(entry)){ FATAL("%s","LAMBDA-LIST is not a slot-name of LOCAL-MACRO"); }
    structure_set(par_new_lambda_list,par_local_macro_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_lambda_list;}
Ref env_local_macro_body(Ref par_local_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_local_macro_instance);
    check_type(par_local_macro_instance,LOCAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(LOCAL_MACRO_Class);
    Ref entry = assq(S(BODY),pos);
    if(nullp(entry)){ FATAL("%s","BODY is not a slot-name of LOCAL-MACRO"); }
    Ref result=structure_get(par_local_macro_instance,cdr(entry));
    close_live_pool(pool);
    return result;}
Ref set_env_local_macro_body(Ref par_new_body, Ref par_local_macro_instance){
    uword pool=open_live_pool();
    add_to_live_pool(par_new_body);
    add_to_live_pool(par_local_macro_instance);
    check_type(par_local_macro_instance,LOCAL_MACRO_Class);
    Ref pos = structure_class_instance_slots(LOCAL_MACRO_Class);
    Ref entry = assq(S(BODY),pos);
    if(nullp(entry)){ FATAL("%s","BODY is not a slot-name of LOCAL-MACRO"); }
    structure_set(par_new_body,par_local_macro_instance,cdr(entry));
    close_live_pool(pool);
    return par_new_body;}

/**** THE END ****/
