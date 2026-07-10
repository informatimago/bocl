#include "built_in_functions.h"
#include "kernel_functions.h"
#include "kernel_structure_functions.h"
#include "kernel.h"
#include "bytecode.h"
#include "cons.h"
#include "variable.h"
#include "stream.h"
#include "reader.h"
#include "printer.h"
#include "packages.h"

/* char_ref/char_set/bit/bit_set/svref/svref_set (defined in bostring.c /
   bit_vector.c / vector.c) are declared in the generated kernel_functions.h. */

void kernel_KERNEL_ROOTSET(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_rootset();
    close_live_pool(pool);
}

void kernel_GET_EVAL_COUNTERS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = get_eval_counters();
    close_live_pool(pool);
}

void kernel_CLASS_NEW(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = class_new(par_name);
    close_live_pool(pool);
}

void kernel_CLASSP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(classp(par_object));
    close_live_pool(pool);
}

void kernel_CLASS_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = class_name(par_class);
    close_live_pool(pool);
}

void kernel_CLASS_DIRECT_SUPERCLASSES(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = class_direct_superclasses(par_class);
    close_live_pool(pool);
}

void kernel_CLASS_ADD_SUPERCLASS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_class = pop_mandatory_argument(function,&current);
    Ref par_superclass = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=0;
    class_add_superclass(par_class, par_superclass);
    close_live_pool(pool);
}

void kernel_CLASS_OF(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = class_of(par_object);
    close_live_pool(pool);
}

void kernel_FIND_CLASS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_class_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = find_class(par_class_name);
    close_live_pool(pool);
}

void kernel_SUBCLASSP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_class = pop_mandatory_argument(function,&current);
    Ref par_superclass = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(subclassp(par_class, par_superclass));
    close_live_pool(pool);
}

void kernel_TYPEP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref par_type   = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    /* Resolve symbol type specifiers to class objects (e.g. 'vector → VECTOR_Class). */
    Ref par_class = symbolp(par_type) ? find_class(par_type) : par_type;
    results->values[0] = (par_class != NULL)
                         ? as_boolean(typep(par_object, par_class))
                         : NIL_Symbol;
    close_live_pool(pool);
}

void kernel_CHECK_TYPE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref par_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=0;
    check_type(par_object, par_class);
    close_live_pool(pool);
}

void kernel_CHECK_CLASS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref par_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=0;
    check_class(par_object, par_class);
    close_live_pool(pool);
}

void kernel_BUILT_IN_CLASS_NEW(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = built_in_class_new(par_name);
    close_live_pool(pool);
}

void kernel_CONS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_car = pop_mandatory_argument(function,&current);
    Ref par_cdr = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons(par_car, par_cdr);
    close_live_pool(pool);
}

void kernel_CONSP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(consp(par_object));
    close_live_pool(pool);
}

void kernel_CONS_CAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cell = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons_car(par_cell);
    close_live_pool(pool);
}

void kernel_CONS_CDR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cell = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons_cdr(par_cell);
    close_live_pool(pool);
}

void kernel_CONS_RPLACA(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cell = pop_mandatory_argument(function,&current);
    Ref par_newcar = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons_rplaca(par_cell, par_newcar);
    close_live_pool(pool);
}

void kernel_CONS_RPLACD(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cell = pop_mandatory_argument(function,&current);
    Ref par_newcdr = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons_rplacd(par_cell, par_newcdr);
    close_live_pool(pool);
}

void kernel_INTEGERP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(integerp(par_object));
    close_live_pool(pool);
}

void kernel_FLOATP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(floatp(par_object));
    close_live_pool(pool);
}

void kernel_CHARACTERP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(characterp(par_object));
    close_live_pool(pool);
}

void kernel_CHAR_CODE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_character = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = character_code(par_character);
    close_live_pool(pool);
}

void kernel_CHAR_INT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_character = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = character_code(par_character);
    close_live_pool(pool);
}

void kernel_CHAR_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_character = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = character_name(par_character);
    close_live_pool(pool);
}

void kernel_CODE_CHAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_code = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = code_character(par_code);
    close_live_pool(pool);
}

void kernel_NAME_CHAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = name_character(par_name);
    close_live_pool(pool);
}

void kernel_FIND_SYMBOL(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    Ref par_obarray = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = find_symbol(par_name, par_obarray);
    close_live_pool(pool);
}

void kernel_MAKE_SYMBOL(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_new(par_name);
    close_live_pool(pool);
}

void kernel_INTERN(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_intern(par_name);
    close_live_pool(pool);
}

void kernel_SYMBOL_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_name(par_symbol);
    close_live_pool(pool);
}

void kernel_SYMBOL_PACKAGE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_package(par_symbol);
    close_live_pool(pool);
}

void kernel_SYMBOL_VALUE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_value(par_symbol);
    close_live_pool(pool);
}

void kernel_SYMBOL_FUNCTION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_function(par_symbol);
    close_live_pool(pool);
}

void kernel_SYMBOL_PLIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_plist(par_symbol);
    close_live_pool(pool);
}

void kernel_SET_SYMBOL_PACKAGE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_package = pop_mandatory_argument(function,&current);
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_symbol_package(par_new_package, par_symbol);
    close_live_pool(pool);
}

void kernel_SET_SYMBOL_VALUE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_value = pop_mandatory_argument(function,&current);
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_symbol_value(par_symbol, par_new_value);
    close_live_pool(pool);
}

void kernel_SET_SYMBOL_FUNCTION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_function = pop_mandatory_argument(function,&current);
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_symbol_function(par_symbol, par_new_function);
    close_live_pool(pool);
}

void kernel_SET_SYMBOL_PLIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_plist = pop_mandatory_argument(function,&current);
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = set_symbol_plist(par_symbol, par_new_plist);
    close_live_pool(pool);
}

void kernel_SYMBOLP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(symbolp(par_object));
    close_live_pool(pool);
}

void kernel_BOUNDP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(symbol_boundp(par_symbol));
    close_live_pool(pool);
}

void kernel_FBOUNDP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(symbol_fboundp(par_symbol));
    close_live_pool(pool);
}

void kernel_MAKUNBOUND(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_make_unbound(par_symbol);
    close_live_pool(pool);
}

void kernel_FMAKUNBOUND(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_make_funbound(par_symbol);
    close_live_pool(pool);
}

void kernel_INTERN_KEYWORD(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = keyword_intern(par_name);
    close_live_pool(pool);
}

void kernel_KEYWORDP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(keywordp(par_object));
    close_live_pool(pool);
}

void kernel_OBARRAY_ALL_SYMBOLS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_obarray = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = obarray_all_symbols(par_obarray);
    close_live_pool(pool);
}

void kernel_NULL(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(nullp(par_object));
    close_live_pool(pool);
}

void kernel_NORMALIZE_EXTERNAL_FORMAT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_external_format = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = normalize_external_format(par_external_format);
    close_live_pool(pool);
}

void kernel_PHYSICAL_PATHNAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_namestring = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = physical_pathname(par_namestring);
    close_live_pool(pool);
}

void kernel_PHYSICAL_PATHNAME_EQUAL(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_a = pop_mandatory_argument(function,&current);
    Ref par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(physical_pathname_equal(par_a, par_b));
    close_live_pool(pool);
}

void kernel_PHYSICAL_PATHNAME_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(physical_pathname_p(par_object));
    close_live_pool(pool);
}

void kernel_PATHNAMEP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(physical_pathname_p(par_object));
    close_live_pool(pool);
}

void kernel_MAKE_FILE_STREAM(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_pathname = pop_mandatory_argument(function,&current);
    Ref par_element_type = pop_mandatory_argument(function,&current);
    Ref par_external_format = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = file_stream_new(par_pathname, par_element_type, par_external_format);
    close_live_pool(pool);
}

void kernel_STREAM_PATHNAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_pathname(par_stream);
    close_live_pool(pool);
}

void kernel_STREAM_ELEMENT_TYPE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_element_type(par_stream);
    close_live_pool(pool);
}

void kernel_STREAM_EXTERNAL_FORMAT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_external_format(par_stream);
    close_live_pool(pool);
}

void kernel_STREAM_DIRECTION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_direction(par_stream);
    close_live_pool(pool);
}

void kernel_OPEN_STREAM_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(stream_open_p(par_stream));
    close_live_pool(pool);
}

void kernel_STANDARD_INPUT_STREAM(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = standard_input_stream();
    close_live_pool(pool);
}

void kernel_STANDARD_OUTPUT_STREAM(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = standard_output_stream();
    close_live_pool(pool);
}

void kernel_STANDARD_ERROR_STREAM(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = standard_error_stream();
    close_live_pool(pool);
}

void kernel_CLOSUREP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(closurep(par_object));
    close_live_pool(pool);
}

void kernel_FUNCTIONP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    /* CL FUNCTIONP returns T for any callable: interpreted function,
       closure, or built-in function. */
    results->values[0] = as_boolean(
        functionp(par_object)         /* Function (interpreted) */
        || closurep(par_object)       /* Closure (lambda/let capture) */
        || compiled_function_p(par_object)   /* byte-code function (vm-1) */
        || compiled_closure_p(par_object)    /* byte-code closure */
        || built_in_function_p(par_object)); /* C built-in */
    close_live_pool(pool);
}

void kernel_BUILT_IN_FUNCTION_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(built_in_function_p(par_object));
    close_live_pool(pool);
}

void kernel_FUNCTION_NAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = function_name(par_function);
    close_live_pool(pool);
}

void kernel_MAKE_FUNCTION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    Ref par_lambda_list = pop_mandatory_argument(function,&current);
    Ref par_lambda_expression = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = function_new(par_name, par_lambda_list, par_lambda_expression);
    close_live_pool(pool);
}

void kernel_MAKE_CLOSURE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_function = pop_mandatory_argument(function,&current);
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = closure_new(par_function, par_environment);
    close_live_pool(pool);
}

void kernel_RPLACA(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cell = pop_mandatory_argument(function,&current);
    Ref par_new_car = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = rplaca(par_cell, par_new_car);
    close_live_pool(pool);
}

void kernel_RPLACD(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cell = pop_mandatory_argument(function,&current);
    Ref par_new_car = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = rplacd(par_cell, par_new_car);
    close_live_pool(pool);
}

void kernel_CAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = car(par_cons);
    close_live_pool(pool);
}

void kernel_CDR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdr(par_cons);
    close_live_pool(pool);
}

void kernel_CAAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caar(par_cons);
    close_live_pool(pool);
}

void kernel_CADR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cadr(par_cons);
    close_live_pool(pool);
}

void kernel_CDAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdar(par_cons);
    close_live_pool(pool);
}

void kernel_CDDR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cddr(par_cons);
    close_live_pool(pool);
}

void kernel_CAAAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caaar(par_cons);
    close_live_pool(pool);
}

void kernel_CAADR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caadr(par_cons);
    close_live_pool(pool);
}

void kernel_CADAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cadar(par_cons);
    close_live_pool(pool);
}

void kernel_CADDR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caddr(par_cons);
    close_live_pool(pool);
}

void kernel_CDAAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdaar(par_cons);
    close_live_pool(pool);
}

void kernel_CDADR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdadr(par_cons);
    close_live_pool(pool);
}

void kernel_CDDAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cddar(par_cons);
    close_live_pool(pool);
}

void kernel_CDDDR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdddr(par_cons);
    close_live_pool(pool);
}

void kernel_CAAAAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caaaar(par_cons);
    close_live_pool(pool);
}

void kernel_CAAADR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caaadr(par_cons);
    close_live_pool(pool);
}

void kernel_CAADAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caadar(par_cons);
    close_live_pool(pool);
}

void kernel_CAADDR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caaddr(par_cons);
    close_live_pool(pool);
}

void kernel_CADAAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cadaar(par_cons);
    close_live_pool(pool);
}

void kernel_CADADR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cadadr(par_cons);
    close_live_pool(pool);
}

void kernel_CADDAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caddar(par_cons);
    close_live_pool(pool);
}

void kernel_CADDDR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cadddr(par_cons);
    close_live_pool(pool);
}

void kernel_CDAAAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdaaar(par_cons);
    close_live_pool(pool);
}

void kernel_CDAADR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdaadr(par_cons);
    close_live_pool(pool);
}

void kernel_CDADAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdadar(par_cons);
    close_live_pool(pool);
}

void kernel_CDADDR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdaddr(par_cons);
    close_live_pool(pool);
}

void kernel_CDDAAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cddaar(par_cons);
    close_live_pool(pool);
}

void kernel_CDDADR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cddadr(par_cons);
    close_live_pool(pool);
}

void kernel_CDDDAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdddar(par_cons);
    close_live_pool(pool);
}

void kernel_CDDDDR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cddddr(par_cons);
    close_live_pool(pool);
}

void kernel_LIST_LENGTH(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    /* CLHS LIST-LENGTH: count via Floyd's tortoise/hare so a CIRCULAR list returns
       NIL instead of looping forever; a dotted (improper) list is a type-error. */
    Ref slow=par_list, fast=par_list;
    uword n=0;
    for(;;){
        if(nullp(fast)){ results->values[0]=integer_from_uword(n); break; }
        if(!consp(fast)){ TYPE_ERROR(par_list,LIST_Class); }
        fast=cons_cdr(fast); n++;
        if(nullp(fast)){ results->values[0]=integer_from_uword(n); break; }
        if(!consp(fast)){ TYPE_ERROR(par_list,LIST_Class); }
        fast=cons_cdr(fast); n++;
        slow=cons_cdr(slow);
        if(fast==slow){ results->values[0]=NIL_Symbol; break; }   /* circular */
    }
    close_live_pool(pool);
}

void kernel_LIST_POSITION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = list_position(par_object, par_list);
    close_live_pool(pool);
}

void kernel_NCONC(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_list = pop_mandatory_argument(function,&current);
    Ref par_tail = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = nconc(par_list, par_tail);
    close_live_pool(pool);
}

void kernel_LAST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = last(par_list);
    close_live_pool(pool);
}

void kernel_COPY_LIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = copy_list(par_list);
    close_live_pool(pool);
}

void kernel_LIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_elements = current;
    results->count=1;
    results->values[0] = copy_list(par_elements);
    close_live_pool(pool);
}

void kernel_DELETE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = delete(par_object, par_list);
    close_live_pool(pool);
}

void kernel_NREVERSE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = nreverse(par_list);
    close_live_pool(pool);
}

void kernel_REVERSE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = reverse(par_list);
    close_live_pool(pool);
}

void kernel_MEMQ(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_item = pop_mandatory_argument(function,&current);
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = memq(par_item, par_list);
    close_live_pool(pool);
}

void kernel_ASSQ(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_key = pop_mandatory_argument(function,&current);
    Ref par_alist = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = assq(par_key, par_alist);
    close_live_pool(pool);
}

void kernel_FIRSTS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_alist = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = firsts(par_alist);
    close_live_pool(pool);
}

void kernel_GETF(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_plist = pop_mandatory_argument(function,&current);
    Ref par_indicator = pop_mandatory_argument(function,&current);
    Ref def_default_value = NIL_Symbol;
    Ref par_default_value = pop_optional_argument(function,&current,def_default_value);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = getf(par_plist, par_indicator, par_default_value);
    close_live_pool(pool);
}

void kernel_PUTF(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_plist = pop_mandatory_argument(function,&current);
    Ref par_indicator = pop_mandatory_argument(function,&current);
    Ref par_new_value = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = putf(par_plist, par_indicator, par_new_value);
    close_live_pool(pool);
}

void kernel_REMF(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_plist = pop_mandatory_argument(function,&current);
    Ref par_indicator = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(remf(par_plist, par_indicator));
    close_live_pool(pool);
}

void kernel_VECTOR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_elements = current;
    results->count=1;
    results->values[0] = vector_new(par_elements);
    close_live_pool(pool);
}

void kernel_VECTORP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(vectorp(par_object));
    close_live_pool(pool);
}

void kernel_VECTOR_LENGTH(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_vector = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = integer_from_uword(vector_length(par_vector));
    close_live_pool(pool);
}

/* %ARRAY-FILL-POINTER vector  ->  the fill pointer (integer) or NIL.
   Accepts any object (LENGTH calls it on arbitrary values): non-vectors and
   vectors without a fill pointer both return NIL. */
void kernel_ARRAY_FILL_POINTER(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_vector = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = array_fill_pointer(par_vector);
    close_live_pool(pool);
}

/* %SET-ARRAY-FILL-POINTER vector fill-pointer  ->  fill-pointer.  Stores the
   fill pointer inline in the vector's slots block. */
void kernel_SET_ARRAY_FILL_POINTER(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_vector = pop_mandatory_argument(function,&current);
    Ref par_fill_pointer = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = array_set_fill_pointer(par_vector,par_fill_pointer);
    close_live_pool(pool);
}

void kernel_SVREF(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_vector = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = svref(par_vector, par_index);
    close_live_pool(pool);
}

void kernel_SVREF_SET(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_vector = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    Ref par_new_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = svref_set(par_vector, par_index, par_new_object);
    close_live_pool(pool);
}

/* ---- Slice 23: MAKE-LIST, AREF, AREF-SET -------------------------------- */

/* (MAKE-LIST n &key initial-element) — build a list of N elements. */
void kernel_MAKE_LIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref n_ref   = pop_mandatory_argument(function,&current);
    Ref init    = NIL_Symbol;
    /* Parse :initial-element keyword */
    while(consp(current)){
        Ref k=cons_car(current); current=cons_cdr(current);
        Ref v=consp(current)?cons_car(current):NIL_Symbol; if(consp(current)) current=cons_cdr(current);
        if(eql(k,keyword_intern_cstring("INITIAL-ELEMENT"))) init=v;}
    word n=integer_value(n_ref);
    if(n<0){ ERROR("MAKE-LIST: negative length %"WORD_FORMAT,n); }
    Ref result=NIL_Symbol;
    add_to_live_pool(init);
    for(word i=0;i<n;i++){
        result=cons(init,result);
        add_to_live_pool(result);}
    results->count=1;
    results->values[0]=result;
    close_live_pool(pool);}

/* (AREF array index) — generic 1-D array element access.
   Dispatches on the array type: vectors, strings, bit-vectors. */
void kernel_AREF(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref arr  = pop_mandatory_argument(function,&current);
    Ref idx  = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    Ref val;
    /* STRING and BIT-VECTOR must be tested BEFORE the general VECTOR: both are
       subtypes of VECTOR (typep "ab" 'vector => T), so checking VECTOR first would
       send a string/bit-vector through svref, which reads its octet/bit storage as
       Refs and crashes.  Order specific representations before the general one. */
    if(typep(arr,STRING_Class)){         val = char_ref(arr,idx); }
    else if(typep(arr,BIT_VECTOR_Class)){ val = bit(arr,idx); }
    else if(typep(arr,VECTOR_Class)){    val = svref(arr,idx); }
    else { TYPE_ERROR(arr,ARRAY_Class); val=NIL_Symbol; }
    results->count=1;
    results->values[0]=val;
    close_live_pool(pool);}

/* (AREF-SET value array index) — setter for AREF (used by (setf (aref …))). */
void kernel_AREF_SET(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref val  = pop_mandatory_argument(function,&current);
    Ref arr  = pop_mandatory_argument(function,&current);
    Ref idx  = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    /* Specific representations before the general VECTOR — see kernel_AREF. */
    if(typep(arr,STRING_Class)){
        /* char_set takes (string, index, character) */
        char_set(arr,idx,val);}
    else if(typep(arr,BIT_VECTOR_Class)){ bit_set(arr,idx,val); }
    else if(typep(arr,VECTOR_Class)){     svref_set(arr,idx,val); }
    else { TYPE_ERROR(arr,ARRAY_Class); }
    results->count=1;
    results->values[0]=val;
    close_live_pool(pool);}

void kernel_BIT_VECTOR_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(bit_vector_p(par_object));
    close_live_pool(pool);
}

void kernel_BIT_VECTOR_LENGTH(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_bit_vector = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = integer_from_uword(bit_vector_length(par_bit_vector));
    close_live_pool(pool);
}

void kernel_BIT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_bit_vector = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = bit(par_bit_vector, par_index);
    close_live_pool(pool);
}

void kernel_SBIT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_bit_vector = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = bit(par_bit_vector, par_index);
    close_live_pool(pool);
}

void kernel_BIT_SET(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_bit_vector = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    Ref par_new_bit = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = bit_set(par_bit_vector, par_index, par_new_bit);
    close_live_pool(pool);
}

void kernel_SBIT_SET(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_bit_vector = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    Ref par_new_bit = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = bit_set(par_bit_vector, par_index, par_new_bit);
    close_live_pool(pool);
}

void kernel_BIT_VECTOR_EQUAL(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_a = pop_mandatory_argument(function,&current);
    Ref par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(bit_vector_equal(par_a, par_b));
    close_live_pool(pool);
}

void kernel_STRING_LENGTH(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_string = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = integer_from_uword(string_length(par_string));
    close_live_pool(pool);
}

void kernel_STRINGEQ(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_a = pop_mandatory_argument(function,&current);
    Ref par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(string_eq(par_a, par_b));
    close_live_pool(pool);
}

void kernel_STRING_EQUAL(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_a = pop_mandatory_argument(function,&current);
    Ref par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(string_equal(par_a, par_b));
    close_live_pool(pool);
}

void kernel_STRINGP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(stringp(par_object));
    close_live_pool(pool);
}

void kernel_CHAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_string = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = char_ref(par_string, par_index);
    close_live_pool(pool);
}

void kernel_SCHAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_string = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = char_ref(par_string, par_index);
    close_live_pool(pool);
}

void kernel_SET_CHAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_char = pop_mandatory_argument(function,&current);
    Ref par_string = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = char_set(par_new_char, par_string, par_index);
    close_live_pool(pool);
}

void kernel_SET_SCHAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_char = pop_mandatory_argument(function,&current);
    Ref par_string = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = char_set(par_new_char, par_string, par_index);
    close_live_pool(pool);
}

void kernel_EQL(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_a = pop_mandatory_argument(function,&current);
    Ref par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(eql(par_a, par_b));
    close_live_pool(pool);
}

void kernel_EQUAL(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_a = pop_mandatory_argument(function,&current);
    Ref par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(equal(par_a, par_b));
    close_live_pool(pool);
}

void kernel_VALUES(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_values = current;
    values(par_values, results);
    close_live_pool(pool);
}

void kernel_VALUES_LIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_values = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    values(par_values, results);
    close_live_pool(pool);
}

void kernel_PATHNAME(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_pathname_designator = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = pathname(par_pathname_designator);
    close_live_pool(pool);
}

void kernel_NAMESTRING(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_pathname = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = namestring(par_pathname);
    close_live_pool(pool);
}

void kernel_PRIN1_TO_STRING(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = prin1_to_string(par_object);
    close_live_pool(pool);
}

void kernel_PRINT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Ref par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = print(par_object, par_stream);
    close_live_pool(pool);
}

void kernel_PRIN1(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Ref par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = prin1(par_object, par_stream);
    close_live_pool(pool);
}

void kernel_PRINC(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Ref par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = princ(par_object, par_stream);
    close_live_pool(pool);
}

void kernel_TERPRI(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Ref par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = terpri(par_stream);
    close_live_pool(pool);
}

void kernel_KERNEL_READ_CHAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_input_stream = variable_value(S(*STANDARD-INPUT*));
    Ref par_input_stream = pop_optional_argument(function,&current,def_input_stream);
    Ref def_eof_error_p = T_Symbol;
    Ref par_eof_error_p = pop_optional_argument(function,&current,def_eof_error_p);
    Ref def_eof_value = NIL_Symbol;
    Ref par_eof_value = pop_optional_argument(function,&current,def_eof_value);
    Ref def_recursive_p = NIL_Symbol;
    Ref par_recursive_p = pop_optional_argument(function,&current,def_recursive_p);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_read_char(par_input_stream, par_eof_error_p, par_eof_value, par_recursive_p);
    close_live_pool(pool);
}

void kernel_KERNEL_PEEK_CHAR(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_peek_type = pop_mandatory_argument(function,&current);
    Ref def_input_stream = variable_value(S(*STANDARD-INPUT*));
    Ref par_input_stream = pop_optional_argument(function,&current,def_input_stream);
    Ref def_eof_error_p = T_Symbol;
    Ref par_eof_error_p = pop_optional_argument(function,&current,def_eof_error_p);
    Ref def_eof_value = NIL_Symbol;
    Ref par_eof_value = pop_optional_argument(function,&current,def_eof_value);
    Ref def_recursive_p = NIL_Symbol;
    Ref par_recursive_p = pop_optional_argument(function,&current,def_recursive_p);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_peek_char(par_peek_type, par_input_stream, par_eof_error_p, par_eof_value, par_recursive_p);
    close_live_pool(pool);
}

void kernel_KERNEL_READ(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_input_stream = variable_value(S(*STANDARD-INPUT*));
    Ref par_input_stream = pop_optional_argument(function,&current,def_input_stream);
    Ref def_eof_error_p = T_Symbol;
    Ref par_eof_error_p = pop_optional_argument(function,&current,def_eof_error_p);
    Ref def_eof_value = NIL_Symbol;
    Ref par_eof_value = pop_optional_argument(function,&current,def_eof_value);
    Ref def_recursive_p = NIL_Symbol;
    Ref par_recursive_p = pop_optional_argument(function,&current,def_recursive_p);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_read(par_input_stream, par_eof_error_p, par_eof_value, par_recursive_p);
    close_live_pool(pool);
}

void kernel_KERNEL_READ_PRESERVING_WHITESPACE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_input_stream = variable_value(S(*STANDARD-INPUT*));
    Ref par_input_stream = pop_optional_argument(function,&current,def_input_stream);
    Ref def_eof_error_p = T_Symbol;
    Ref par_eof_error_p = pop_optional_argument(function,&current,def_eof_error_p);
    Ref def_eof_value = NIL_Symbol;
    Ref par_eof_value = pop_optional_argument(function,&current,def_eof_value);
    Ref def_recursive_p = NIL_Symbol;
    Ref par_recursive_p = pop_optional_argument(function,&current,def_recursive_p);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_read_preserving_whitespace(par_input_stream, par_eof_error_p, par_eof_value, par_recursive_p);
    close_live_pool(pool);
}

void kernel_KERNEL_READ_DELIMITED_LIST(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_character = pop_mandatory_argument(function,&current);
    Ref def_input_stream = variable_value(S(*STANDARD-INPUT*));
    Ref par_input_stream = pop_optional_argument(function,&current,def_input_stream);
    Ref def_recursive_p = NIL_Symbol;
    Ref par_recursive_p = pop_optional_argument(function,&current,def_recursive_p);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_read_delimited_list(par_character, par_input_stream, par_recursive_p);
    close_live_pool(pool);
}

void kernel_KERNEL_READ_FROM_STRING(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_input_string = pop_mandatory_argument(function,&current);
    Ref def_eof_error_p = T_Symbol;
    Ref par_eof_error_p = pop_optional_argument(function,&current,def_eof_error_p);
    Ref def_eof_value = NIL_Symbol;
    Ref par_eof_value = pop_optional_argument(function,&current,def_eof_value);
    Ref def_start = I(0);
    Ref par_start = pop_optional_argument(function,&current,def_start);
    Ref def_end = NIL_Symbol;
    Ref par_end = pop_optional_argument(function,&current,def_end);
    Ref def_preserve_whitespace = NIL_Symbol;
    Ref par_preserve_whitespace = pop_optional_argument(function,&current,def_preserve_whitespace);
    check_no_more_arguments(function,current);
    /* READ-FROM-STRING returns two values: the object and the index of the first
       unread character (CLHS). */
    Ref position = I(0);
    results->count=2;
    results->values[0] = kernel_read_from_string(par_input_string, par_eof_error_p, par_eof_value, par_start, par_end, par_preserve_whitespace, &position);
    results->values[1] = position;
    close_live_pool(pool);
}

void kernel_MAKE_STRING_INPUT_STREAM(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_string = pop_mandatory_argument(function,&current);
    Ref def_start = NIL_Symbol;
    Ref par_start = pop_optional_argument(function,&current,def_start);
    Ref def_end = NIL_Symbol;
    Ref par_end = pop_optional_argument(function,&current,def_end);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_input_string(par_string, par_start, par_end);
    close_live_pool(pool);
}

void kernel_MAKE_STRING_OUTPUT_STREAM(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_element_type = NIL_Symbol;
    Ref par_element_type = pop_key_argument(function,&current,K(ELEMENT-TYPE),def_element_type);
    results->count=1;
    results->values[0] = stream_output_string(par_element_type);
    close_live_pool(pool);
}

void kernel_GET_OUTPUT_STREAM_STRING(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_string_output_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = string_output_stream_get_string(par_string_output_stream);
    close_live_pool(pool);
}

void kernel_OPEN(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_filespec = pop_mandatory_argument(function,&current);
    Ref def_direction = K(INPUT);
    Ref par_direction = pop_key_argument(function,&current,K(DIRECTION),def_direction);
    Ref def_element_type = S(CHARACTER);
    Ref par_element_type = pop_key_argument(function,&current,K(ELEMENT-TYPE),def_element_type);
    Ref def_if_exists = K(ERROR);
    Ref par_if_exists = pop_key_argument(function,&current,K(IF-EXISTS),def_if_exists);
    Ref def_if_does_not_exist = K(CREATE);
    Ref par_if_does_not_exist = pop_key_argument(function,&current,K(IF-DOES-NOT-EXIST),def_if_does_not_exist);
    Ref def_external_format = K(DEFAULT);
    Ref par_external_format = pop_key_argument(function,&current,K(EXTERNAL-FORMAT),def_external_format);
    results->count=1;
    results->values[0] = stream_open(par_filespec, par_direction, par_element_type, par_if_exists, par_if_does_not_exist, par_external_format);
    close_live_pool(pool);
}

void kernel_FINISH_OUTPUT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Ref par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_finish_output(par_stream);
    close_live_pool(pool);
}

void kernel_FORCE_OUTPUT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Ref par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_finish_output(par_stream);
    close_live_pool(pool);
}

void kernel_EQ_I(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_a = pop_mandatory_argument(function,&current);
    Ref par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(integer_eq(par_a, par_b));
    close_live_pool(pool);
}

void kernel_EQ_F(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_a = pop_mandatory_argument(function,&current);
    Ref par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(float_eq(par_a, par_b));
    close_live_pool(pool);
}

void kernel_MAKE_STRUCTURE_CLASS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    Ref par_direct_superclass = pop_mandatory_argument(function,&current);
    Ref par_offset = pop_mandatory_argument(function,&current);
    Ref par_slots = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = make_structure_class(par_name, par_direct_superclass, par_offset, par_slots);
    close_live_pool(pool);
}

void kernel_STRUCTURE_CLASS_INSTANCE_SIZE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_structure_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = structure_class_instance_size(par_structure_class);
    close_live_pool(pool);
}

void kernel_STRUCTURE_CLASS_INSTANCE_DIRECT_SLOTS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_structure_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = structure_class_instance_direct_slots(par_structure_class);
    close_live_pool(pool);
}

void kernel_STRUCTURE_CLASS_INSTANCE_SLOTS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_structure_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = structure_class_instance_slots(par_structure_class);
    close_live_pool(pool);
}

void kernel_MAKE_STRUCTURE_INSTANCE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_structure_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = make_structure_instance(par_structure_class);
    close_live_pool(pool);
}

void kernel_STRUCTUREP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(structurep(par_object));
    close_live_pool(pool);
}

void kernel_STRUCTURE_GET(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_structure_instance = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = structure_get(par_structure_instance, par_index);
    close_live_pool(pool);
}

void kernel_STRUCTURE_SET(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_new_value = pop_mandatory_argument(function,&current);
    Ref par_structure_instance = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = structure_set(par_new_value, par_structure_instance, par_index);
    close_live_pool(pool);
}

void kernel_MAKE_ENVIRONMENT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref def_next_environment = NIL_Symbol;
    Ref par_next_environment = pop_optional_argument(function,&current,def_next_environment);
    Ref def_toplevelp = NIL_Symbol;
    Ref par_toplevelp = pop_optional_argument(function,&current,def_toplevelp);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_new(par_next_environment, par_toplevelp);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_IS_TOPLEVEL(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(environment_is_toplevel(par_environment));
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_VARIABLES(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_variables(par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_FUNCTIONS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_functions(par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_BLOCKS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_blocks(par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_CATCHS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_catchs(par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_DECLARATIONS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_declarations(par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_NEXT_ENVIRONMENT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_next_environment(par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_FRAMES(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_frames(par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_ADD_VARIABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_variable = pop_mandatory_argument(function,&current);
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_add_variable(par_variable, par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_ADD_FUNCTION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_function = pop_mandatory_argument(function,&current);
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_add_function(par_function, par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_ADD_BLOCK(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_add_block(par_name, par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_ADD_CATCH(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_tag = pop_mandatory_argument(function,&current);
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_add_catch(par_tag, par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_ADD_DECLARATION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_declaration = pop_mandatory_argument(function,&current);
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_add_declaration(par_declaration, par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_FIND_VARIABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_find_variable(par_name, par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_FIND_FUNCTION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_find_function(par_name, par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_FIND_BLOCK(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_find_block(par_name, par_environment);
    close_live_pool(pool);
}

void kernel_ENVIRONMENT_FIND_CATCH(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_tag = pop_mandatory_argument(function,&current);
    Ref par_environment = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = environment_find_catch(par_tag, par_environment);
    close_live_pool(pool);
}

void kernel_ENV_SPECIAL_VARIABLE_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(env_special_variable_p(par_object));
    close_live_pool(pool);
}


void kernel_SET_PRINT_LEVEL_DEPTH(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_depth = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    printer_set_level_depth(integer_value(par_depth));
    results->count=1;
    results->values[0] = par_depth;
    close_live_pool(pool);
}

/* (%CIRCLE-ACTIVE-P) — T while *PRINT-CIRCLE* labelling is in effect, so the
   prelude array printer knows whether to use #n=/#n# or its lexical cycle guard. */
void kernel_CIRCLE_ACTIVE_P(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    check_no_more_arguments(function,arguments);
    results->count=1;
    results->values[0] = as_boolean(printer_circle_active());
    close_live_pool(pool);
}

/* (%CIRCLE-ACTION obj) — the *PRINT-CIRCLE* label decision for OBJ: a positive
   integer N for a back-reference (the prelude prints "#N#"), a negative -N for a
   shared object's first appearance (the prelude prints "#N=" then OBJ), or NIL to
   print OBJ normally.  Mutates the label registry (assigns N) like the C printer. */
void kernel_CIRCLE_ACTION(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref obj = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    word act = printer_circle_action(obj);
    results->count=1;
    results->values[0] = (act==0) ? NIL_Symbol : integer_from_word(act);
    close_live_pool(pool);
}

/* (%FIND-LOCAL-MACRO symbol environment) — if SYMBOL names a lexically-local macro
   (MACROLET / WITH-HASH-TABLE-ITERATOR) in ENVIRONMENT, return a macro function for
   it; else NIL.  MACRO-FUNCTION consults this so it sees local macros via the
   &ENVIRONMENT it is given. */
void kernel_FIND_LOCAL_MACRO(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref sym = pop_mandatory_argument(function,&current);
    Ref env = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    Ref result = NIL_Symbol;
    if(!nullp(env)){
        Ref local = environment_find_function(sym, env);
        if(!nullp(local) && env_local_macro_p(local)){
            result = function_new(env_local_macro_name(local),
                                  env_local_macro_lambda_list(local),
                                  env_local_macro_body(local)); } }
    results->count=1; results->values[0]=result;
    close_live_pool(pool);
}

void kernel_initialize_built_in_functions(void){
    {   Ref fname=S(KERNEL-ROOTSET);
        Ref lambda_list=S(NIL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_KERNEL_ROOTSET));}
    {   Ref fname=S(%SET-PRINT-LEVEL-DEPTH);
        Ref lambda_list=list(S(DEPTH),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SET_PRINT_LEVEL_DEPTH));}
    {   Ref fname=S(%CIRCLE-ACTIVE-P);
        Ref lambda_list=S(NIL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CIRCLE_ACTIVE_P));}
    {   Ref fname=S(%CIRCLE-ACTION);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CIRCLE_ACTION));}
    {   Ref fname=S(%FIND-LOCAL-MACRO);
        Ref lambda_list=list(S(SYMBOL),S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_FIND_LOCAL_MACRO));}
    {   Ref fname=S(GET-EVAL-COUNTERS);
        Ref lambda_list=S(NIL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_GET_EVAL_COUNTERS));}
    {   Ref fname=S(CLASS-NEW);
        Ref lambda_list=list(S(NAME),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CLASS_NEW));}
    {   Ref fname=S(CLASSP);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CLASSP));}
    {   Ref fname=S(CLASS-NAME);
        Ref lambda_list=list(S(CLASS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CLASS_NAME));}
    {   Ref fname=S(CLASS-DIRECT-SUPERCLASSES);
        Ref lambda_list=list(S(CLASS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CLASS_DIRECT_SUPERCLASSES));}
    {   Ref fname=S(CLASS-ADD-SUPERCLASS);
        Ref lambda_list=list(S(CLASS),S(SUPERCLASS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CLASS_ADD_SUPERCLASS));}
    {   Ref fname=S(CLASS-OF);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CLASS_OF));}
    {   Ref fname=S(FIND-CLASS);
        Ref lambda_list=list(S(CLASS-NAME),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_FIND_CLASS));}
    {   Ref fname=S(SUBCLASSP);
        Ref lambda_list=list(S(CLASS),S(SUPERCLASS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SUBCLASSP));}
    {   Ref fname=S(TYPEP);
        Ref lambda_list=list(S(OBJECT),S(CLASS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_TYPEP));}
    {   Ref fname=S(CHECK-TYPE);
        Ref lambda_list=list(S(OBJECT),S(CLASS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CHECK_TYPE));}
    {   Ref fname=S(CHECK-CLASS);
        Ref lambda_list=list(S(OBJECT),S(CLASS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CHECK_CLASS));}
    {   Ref fname=S(BUILT-IN-CLASS-NEW);
        Ref lambda_list=list(S(NAME),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_BUILT_IN_CLASS_NEW));}
    {   Ref fname=S(CONS);
        Ref lambda_list=list(S(CAR),S(CDR),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CONS));}
    {   Ref fname=S(CONSP);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CONSP));}
    {   Ref fname=S(CONS-CAR);
        Ref lambda_list=list(S(CELL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CONS_CAR));}
    {   Ref fname=S(CONS-CDR);
        Ref lambda_list=list(S(CELL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CONS_CDR));}
    {   Ref fname=S(CONS-RPLACA);
        Ref lambda_list=list(S(CELL),S(NEWCAR),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CONS_RPLACA));}
    {   Ref fname=S(CONS-RPLACD);
        Ref lambda_list=list(S(CELL),S(NEWCDR),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CONS_RPLACD));}
    {   Ref fname=S(INTEGERP);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_INTEGERP));}
    {   Ref fname=S(FLOATP);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_FLOATP));}
    {   Ref fname=S(CHARACTERP);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CHARACTERP));}
    {   Ref fname=S(CHAR-CODE);
        Ref lambda_list=list(S(CHARACTER),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CHAR_CODE));}
    {   Ref fname=S(CHAR-INT);
        Ref lambda_list=list(S(CHARACTER),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CHAR_INT));}
    {   Ref fname=S(CHAR-NAME);
        Ref lambda_list=list(S(CHARACTER),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CHAR_NAME));}
    {   Ref fname=S(CODE-CHAR);
        Ref lambda_list=list(S(CODE),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CODE_CHAR));}
    {   Ref fname=S(NAME-CHAR);
        Ref lambda_list=list(S(NAME),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_NAME_CHAR));}
    {   Ref fname=S(FIND-SYMBOL);
        Ref lambda_list=list(S(NAME),S(OBARRAY),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_FIND_SYMBOL));}
    {   Ref fname=S(MAKE-SYMBOL);
        Ref lambda_list=list(S(NAME),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_MAKE_SYMBOL));}
    {   Ref fname=S(INTERN);
        Ref lambda_list=list(S(NAME),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_INTERN));}
    {   Ref fname=S(SYMBOL-NAME);
        Ref lambda_list=list(S(SYMBOL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SYMBOL_NAME));}
    {   Ref fname=S(SYMBOL-PACKAGE);
        Ref lambda_list=list(S(SYMBOL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SYMBOL_PACKAGE));}
    {   Ref fname=S(SYMBOL-VALUE);
        Ref lambda_list=list(S(SYMBOL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SYMBOL_VALUE));}
    {   Ref fname=S(SYMBOL-FUNCTION);
        Ref lambda_list=list(S(SYMBOL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SYMBOL_FUNCTION));}
    {   Ref fname=S(SYMBOL-PLIST);
        Ref lambda_list=list(S(SYMBOL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SYMBOL_PLIST));}
    {   Ref fname=S(SET-SYMBOL-PACKAGE);
        Ref lambda_list=list(S(NEW-PACKAGE),S(SYMBOL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SET_SYMBOL_PACKAGE));}
    {   Ref fname=S(SET-SYMBOL-VALUE);
        Ref lambda_list=list(S(NEW-VALUE),S(SYMBOL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SET_SYMBOL_VALUE));}
    {   Ref fname=S(SET-SYMBOL-FUNCTION);
        Ref lambda_list=list(S(NEW-FUNCTION),S(SYMBOL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SET_SYMBOL_FUNCTION));}
    {   Ref fname=S(SET-SYMBOL-PLIST);
        Ref lambda_list=list(S(NEW-PLIST),S(SYMBOL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SET_SYMBOL_PLIST));}
    {   Ref fname=S(SYMBOLP);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SYMBOLP));}
    {   Ref fname=S(BOUNDP);
        Ref lambda_list=list(S(SYMBOL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_BOUNDP));}
    {   Ref fname=S(FBOUNDP);
        Ref lambda_list=list(S(SYMBOL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_FBOUNDP));}
    {   Ref fname=S(MAKUNBOUND);
        Ref lambda_list=list(S(SYMBOL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_MAKUNBOUND));}
    {   Ref fname=S(FMAKUNBOUND);
        Ref lambda_list=list(S(SYMBOL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_FMAKUNBOUND));}
    {   Ref fname=S(INTERN-KEYWORD);
        Ref lambda_list=list(S(NAME),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_INTERN_KEYWORD));}
    {   Ref fname=S(KEYWORDP);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_KEYWORDP));}
    {   Ref fname=S(OBARRAY-ALL-SYMBOLS);
        Ref lambda_list=list(S(OBARRAY),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_OBARRAY_ALL_SYMBOLS));}
    {   Ref fname=S(NULL);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_NULL));}
    {   Ref fname=S(NORMALIZE-EXTERNAL-FORMAT);
        Ref lambda_list=list(S(EXTERNAL-FORMAT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_NORMALIZE_EXTERNAL_FORMAT));}
    {   Ref fname=S(PHYSICAL-PATHNAME);
        Ref lambda_list=list(S(NAMESTRING),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_PHYSICAL_PATHNAME));}
    {   Ref fname=S(PHYSICAL-PATHNAME-EQUAL);
        Ref lambda_list=list(S(A),S(B),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_PHYSICAL_PATHNAME_EQUAL));}
    {   Ref fname=S(PHYSICAL-PATHNAME-P);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_PHYSICAL_PATHNAME_P));}
    {   Ref fname=S(PATHNAMEP);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_PATHNAMEP));}
    {   Ref fname=S(MAKE-FILE-STREAM);
        Ref lambda_list=list(S(PATHNAME),S(ELEMENT-TYPE),S(EXTERNAL-FORMAT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_MAKE_FILE_STREAM));}
    {   Ref fname=S(STREAM-PATHNAME);
        Ref lambda_list=list(S(STREAM),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STREAM_PATHNAME));}
    {   Ref fname=S(STREAM-ELEMENT-TYPE);
        Ref lambda_list=list(S(STREAM),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STREAM_ELEMENT_TYPE));}
    {   Ref fname=S(STREAM-EXTERNAL-FORMAT);
        Ref lambda_list=list(S(STREAM),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STREAM_EXTERNAL_FORMAT));}
    {   Ref fname=S(STREAM-DIRECTION);
        Ref lambda_list=list(S(STREAM),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STREAM_DIRECTION));}
    {   Ref fname=S(OPEN-STREAM-P);
        Ref lambda_list=list(S(STREAM),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_OPEN_STREAM_P));}
    {   Ref fname=S(STANDARD-INPUT-STREAM);
        Ref lambda_list=S(NIL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STANDARD_INPUT_STREAM));}
    {   Ref fname=S(STANDARD-OUTPUT-STREAM);
        Ref lambda_list=S(NIL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STANDARD_OUTPUT_STREAM));}
    {   Ref fname=S(STANDARD-ERROR-STREAM);
        Ref lambda_list=S(NIL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STANDARD_ERROR_STREAM));}
    {   Ref fname=S(CLOSUREP);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CLOSUREP));}
    {   Ref fname=S(FUNCTIONP);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_FUNCTIONP));}
    {   Ref fname=S(BUILT-IN-FUNCTION-P);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_BUILT_IN_FUNCTION_P));}
    {   Ref fname=S(FUNCTION-NAME);
        Ref lambda_list=list(S(FUNCTION),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_FUNCTION_NAME));}
    {   Ref fname=S(MAKE-FUNCTION);
        Ref lambda_list=list(S(NAME),S(LAMBDA-LIST),S(LAMBDA-EXPRESSION),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_MAKE_FUNCTION));}
    {   Ref fname=S(MAKE-CLOSURE);
        Ref lambda_list=list(S(FUNCTION),S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_MAKE_CLOSURE));}
    {   Ref fname=S(RPLACA);
        Ref lambda_list=list(S(CELL),S(NEW-CAR),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_RPLACA));}
    {   Ref fname=S(RPLACD);
        Ref lambda_list=list(S(CELL),S(NEW-CAR),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_RPLACD));}
    {   Ref fname=S(CAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CAR));}
    {   Ref fname=S(CDR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDR));}
    {   Ref fname=S(CAAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CAAR));}
    {   Ref fname=S(CADR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CADR));}
    {   Ref fname=S(CDAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDAR));}
    {   Ref fname=S(CDDR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDDR));}
    {   Ref fname=S(CAAAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CAAAR));}
    {   Ref fname=S(CAADR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CAADR));}
    {   Ref fname=S(CADAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CADAR));}
    {   Ref fname=S(CADDR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CADDR));}
    {   Ref fname=S(CDAAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDAAR));}
    {   Ref fname=S(CDADR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDADR));}
    {   Ref fname=S(CDDAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDDAR));}
    {   Ref fname=S(CDDDR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDDDR));}
    {   Ref fname=S(CAAAAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CAAAAR));}
    {   Ref fname=S(CAAADR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CAAADR));}
    {   Ref fname=S(CAADAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CAADAR));}
    {   Ref fname=S(CAADDR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CAADDR));}
    {   Ref fname=S(CADAAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CADAAR));}
    {   Ref fname=S(CADADR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CADADR));}
    {   Ref fname=S(CADDAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CADDAR));}
    {   Ref fname=S(CADDDR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CADDDR));}
    {   Ref fname=S(CDAAAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDAAAR));}
    {   Ref fname=S(CDAADR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDAADR));}
    {   Ref fname=S(CDADAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDADAR));}
    {   Ref fname=S(CDADDR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDADDR));}
    {   Ref fname=S(CDDAAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDDAAR));}
    {   Ref fname=S(CDDADR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDDADR));}
    {   Ref fname=S(CDDDAR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDDDAR));}
    {   Ref fname=S(CDDDDR);
        Ref lambda_list=list(S(CONS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CDDDDR));}
    {   Ref fname=S(LIST-LENGTH);
        Ref lambda_list=list(S(LIST),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_LIST_LENGTH));}
    {   Ref fname=S(LIST-POSITION);
        Ref lambda_list=list(S(OBJECT),S(LIST),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_LIST_POSITION));}
    {   Ref fname=S(NCONC);
        Ref lambda_list=list(S(LIST),S(TAIL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_NCONC));}
    {   Ref fname=S(LAST);
        Ref lambda_list=list(S(LIST),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_LAST));}
    {   Ref fname=S(COPY-LIST);
        Ref lambda_list=list(S(LIST),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_COPY_LIST));}
    {   Ref fname=S(LIST);
        Ref lambda_list=list(S(&REST),S(ELEMENTS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_LIST));}
    {   Ref fname=S(DELETE);
        Ref lambda_list=list(S(OBJECT),S(LIST),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_DELETE));}
    {   Ref fname=S(NREVERSE);
        Ref lambda_list=list(S(LIST),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_NREVERSE));}
    {   Ref fname=S(REVERSE);
        Ref lambda_list=list(S(LIST),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_REVERSE));}
    {   Ref fname=S(MEMQ);
        Ref lambda_list=list(S(ITEM),S(LIST),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_MEMQ));}
    {   Ref fname=S(ASSQ);
        Ref lambda_list=list(S(KEY),S(ALIST),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ASSQ));}
    {   Ref fname=S(FIRSTS);
        Ref lambda_list=list(S(ALIST),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_FIRSTS));}
    {   Ref fname=S(GETF);
        Ref lambda_list=list(S(PLIST),S(INDICATOR),S(&OPTIONAL),S(DEFAULT-VALUE),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_GETF));}
    {   Ref fname=S(PUTF);
        Ref lambda_list=list(S(PLIST),S(INDICATOR),S(NEW-VALUE),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_PUTF));}
    {   Ref fname=S(REMF);
        Ref lambda_list=list(S(PLIST),S(INDICATOR),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_REMF));}
    {   Ref fname=S(VECTOR);
        Ref lambda_list=list(S(&REST),S(ELEMENTS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_VECTOR));}
    {   Ref fname=S(VECTORP);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_VECTORP));}
    {   Ref fname=S(VECTOR-LENGTH);
        Ref lambda_list=list(S(VECTOR),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_VECTOR_LENGTH));}
    {   Ref fname=symbol_intern_cstring("%ARRAY-FILL-POINTER");
        Ref lambda_list=list(S(VECTOR),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ARRAY_FILL_POINTER));}
    {   Ref fname=symbol_intern_cstring("%SET-ARRAY-FILL-POINTER");
        Ref lambda_list=list(S(VECTOR),S(FILL-POINTER),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SET_ARRAY_FILL_POINTER));}
    {   Ref fname=S(SVREF);
        Ref lambda_list=list(S(VECTOR),S(INDEX),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SVREF));}
    {   Ref fname=S(SVREF-SET);
        Ref lambda_list=list(S(VECTOR),S(INDEX),S(NEW-OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SVREF_SET));}
    {   Ref fname=S(BIT-VECTOR-P);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_BIT_VECTOR_P));}
    {   Ref fname=S(BIT-VECTOR-LENGTH);
        Ref lambda_list=list(S(BIT-VECTOR),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_BIT_VECTOR_LENGTH));}
    {   Ref fname=S(BIT);
        Ref lambda_list=list(S(BIT-VECTOR),S(INDEX),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_BIT));}
    {   Ref fname=S(SBIT);
        Ref lambda_list=list(S(BIT-VECTOR),S(INDEX),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SBIT));}
    {   Ref fname=S(BIT-SET);
        Ref lambda_list=list(S(BIT-VECTOR),S(INDEX),S(NEW-BIT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_BIT_SET));}
    {   Ref fname=S(SBIT-SET);
        Ref lambda_list=list(S(BIT-VECTOR),S(INDEX),S(NEW-BIT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SBIT_SET));}
    {   Ref fname=S(BIT-VECTOR-EQUAL);
        Ref lambda_list=list(S(A),S(B),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_BIT_VECTOR_EQUAL));}
    {   Ref fname=S(STRING-LENGTH);
        Ref lambda_list=list(S(STRING),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STRING_LENGTH));}
    {   Ref fname=S(STRING=);
        Ref lambda_list=list(S(A),S(B),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STRINGEQ));}
    {   Ref fname=S(STRING-EQUAL);
        Ref lambda_list=list(S(A),S(B),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STRING_EQUAL));}
    {   Ref fname=S(STRINGP);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STRINGP));}
    {   Ref fname=S(CHAR);
        Ref lambda_list=list(S(STRING),S(INDEX),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_CHAR));}
    {   Ref fname=S(SCHAR);
        Ref lambda_list=list(S(STRING),S(INDEX),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SCHAR));}
    {   Ref fname=S(SET-CHAR);
        Ref lambda_list=list(S(NEW-CHAR),S(STRING),S(INDEX),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SET_CHAR));}
    {   Ref fname=S(SET-SCHAR);
        Ref lambda_list=list(S(NEW-CHAR),S(STRING),S(INDEX),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_SET_SCHAR));}
    {   Ref fname=S(EQL);
        Ref lambda_list=list(S(A),S(B),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_EQL));}
    {   Ref fname=S(EQUAL);
        Ref lambda_list=list(S(A),S(B),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_EQUAL));}
    {   Ref fname=S(VALUES);
        Ref lambda_list=list(S(&REST),S(VALUES),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_VALUES));}
    {   Ref fname=S(VALUES-LIST);
        Ref lambda_list=list(S(VALUES),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_VALUES_LIST));}
    {   Ref fname=S(PATHNAME);
        Ref lambda_list=list(S(PATHNAME-DESIGNATOR),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_PATHNAME));}
    {   Ref fname=S(NAMESTRING);
        Ref lambda_list=list(S(PATHNAME),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_NAMESTRING));}
    {   Ref fname=S(PRIN1-TO-STRING);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_PRIN1_TO_STRING));}
    {   Ref fname=S(PRINT);
        Ref lambda_list=list(S(OBJECT),S(&OPTIONAL),list(S(STREAM),S(*STANDARD-OUTPUT*),NULL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_PRINT));}
    {   Ref fname=S(PRIN1);
        Ref lambda_list=list(S(OBJECT),S(&OPTIONAL),list(S(STREAM),S(*STANDARD-OUTPUT*),NULL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_PRIN1));}
    {   Ref fname=S(PRINC);
        Ref lambda_list=list(S(OBJECT),S(&OPTIONAL),list(S(STREAM),S(*STANDARD-OUTPUT*),NULL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_PRINC));}
    {   Ref fname=S(TERPRI);
        Ref lambda_list=list(S(&OPTIONAL),list(S(STREAM),S(*STANDARD-OUTPUT*),NULL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_TERPRI));}
    {   Ref fname=S(KERNEL-READ-CHAR);
        Ref lambda_list=list(S(&OPTIONAL),list(S(INPUT-STREAM),S(*STANDARD-INPUT*),NULL),list(S(EOF-ERROR-P),S(T),NULL),S(EOF-VALUE),S(RECURSIVE-P),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_KERNEL_READ_CHAR));}
    {   Ref fname=S(KERNEL-PEEK-CHAR);
        Ref lambda_list=list(S(PEEK-TYPE),S(&OPTIONAL),list(S(INPUT-STREAM),S(*STANDARD-INPUT*),NULL),list(S(EOF-ERROR-P),S(T),NULL),S(EOF-VALUE),S(RECURSIVE-P),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_KERNEL_PEEK_CHAR));}
    {   Ref fname=S(KERNEL-READ);
        Ref lambda_list=list(S(&OPTIONAL),list(S(INPUT-STREAM),S(*STANDARD-INPUT*),NULL),list(S(EOF-ERROR-P),S(T),NULL),S(EOF-VALUE),S(RECURSIVE-P),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_KERNEL_READ));}
    {   Ref fname=S(KERNEL-READ-PRESERVING-WHITESPACE);
        Ref lambda_list=list(S(&OPTIONAL),list(S(INPUT-STREAM),S(*STANDARD-INPUT*),NULL),list(S(EOF-ERROR-P),S(T),NULL),S(EOF-VALUE),S(RECURSIVE-P),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_KERNEL_READ_PRESERVING_WHITESPACE));}
    {   Ref fname=S(KERNEL-READ-DELIMITED-LIST);
        Ref lambda_list=list(S(CHARACTER),S(&OPTIONAL),list(S(INPUT-STREAM),S(*STANDARD-INPUT*),NULL),S(RECURSIVE-P),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_KERNEL_READ_DELIMITED_LIST));}
    {   Ref fname=S(KERNEL-READ-FROM-STRING);
        Ref lambda_list=list(S(INPUT-STRING),S(&OPTIONAL),list(S(EOF-ERROR-P),S(T),NULL),S(EOF-VALUE),list(S(START),I(0),NULL),S(END),S(PRESERVE-WHITESPACE),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_KERNEL_READ_FROM_STRING));}
    {   Ref fname=S(MAKE-STRING-INPUT-STREAM);
        Ref lambda_list=list(S(STRING),S(&OPTIONAL),S(START),S(END),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_MAKE_STRING_INPUT_STREAM));}
    {   Ref fname=S(MAKE-STRING-OUTPUT-STREAM);
        Ref lambda_list=list(S(&KEY),S(ELEMENT-TYPE),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_MAKE_STRING_OUTPUT_STREAM));}
    {   Ref fname=S(GET-OUTPUT-STREAM-STRING);
        Ref lambda_list=list(S(STRING-OUTPUT-STREAM),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_GET_OUTPUT_STREAM_STRING));}
    {   Ref fname=S(OPEN);
        Ref lambda_list=list(S(FILESPEC),S(&KEY),list(S(DIRECTION),K(INPUT),NULL),list(S(ELEMENT-TYPE),list(S(QUOTE),S(CHARACTER),NULL),NULL),list(S(IF-EXISTS),K(ERROR),NULL),list(S(IF-DOES-NOT-EXIST),K(CREATE),NULL),list(S(EXTERNAL-FORMAT),K(DEFAULT),NULL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_OPEN));}
    {   Ref fname=S(FINISH-OUTPUT);
        Ref lambda_list=list(S(&OPTIONAL),list(S(STREAM),S(*STANDARD-OUTPUT*),NULL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_FINISH_OUTPUT));}
    {   Ref fname=S(FORCE-OUTPUT);
        Ref lambda_list=list(S(&OPTIONAL),list(S(STREAM),S(*STANDARD-OUTPUT*),NULL),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_FORCE_OUTPUT));}
    {   Ref fname=S(=/I);
        Ref lambda_list=list(S(A),S(B),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_EQ_I));}
    {   Ref fname=S(=/F);
        Ref lambda_list=list(S(A),S(B),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_EQ_F));}
    {   Ref fname=S(MAKE-STRUCTURE-CLASS);
        Ref lambda_list=list(S(NAME),S(DIRECT-SUPERCLASS),S(OFFSET),S(SLOTS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_MAKE_STRUCTURE_CLASS));}
    {   Ref fname=S(STRUCTURE-CLASS-INSTANCE-SIZE);
        Ref lambda_list=list(S(STRUCTURE-CLASS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STRUCTURE_CLASS_INSTANCE_SIZE));}
    {   Ref fname=S(STRUCTURE-CLASS-INSTANCE-DIRECT-SLOTS);
        Ref lambda_list=list(S(STRUCTURE-CLASS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STRUCTURE_CLASS_INSTANCE_DIRECT_SLOTS));}
    {   Ref fname=S(STRUCTURE-CLASS-INSTANCE-SLOTS);
        Ref lambda_list=list(S(STRUCTURE-CLASS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STRUCTURE_CLASS_INSTANCE_SLOTS));}
    {   Ref fname=S(MAKE-STRUCTURE-INSTANCE);
        Ref lambda_list=list(S(STRUCTURE-CLASS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_MAKE_STRUCTURE_INSTANCE));}
    {   Ref fname=S(STRUCTUREP);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STRUCTUREP));}
    {   Ref fname=S(STRUCTURE-GET);
        Ref lambda_list=list(S(STRUCTURE-INSTANCE),S(INDEX),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STRUCTURE_GET));}
    {   Ref fname=S(STRUCTURE-SET);
        Ref lambda_list=list(S(NEW-VALUE),S(STRUCTURE-INSTANCE),S(INDEX),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_STRUCTURE_SET));}
    {   Ref fname=S(MAKE-ENVIRONMENT);
        Ref lambda_list=list(S(&OPTIONAL),S(NEXT-ENVIRONMENT),S(TOPLEVELP),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_MAKE_ENVIRONMENT));}
    {   Ref fname=S(ENVIRONMENT-IS-TOPLEVEL);
        Ref lambda_list=list(S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_IS_TOPLEVEL));}
    {   Ref fname=S(ENVIRONMENT-VARIABLES);
        Ref lambda_list=list(S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_VARIABLES));}
    {   Ref fname=S(ENVIRONMENT-FUNCTIONS);
        Ref lambda_list=list(S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_FUNCTIONS));}
    {   Ref fname=S(ENVIRONMENT-BLOCKS);
        Ref lambda_list=list(S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_BLOCKS));}
    {   Ref fname=S(ENVIRONMENT-CATCHS);
        Ref lambda_list=list(S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_CATCHS));}
    {   Ref fname=S(ENVIRONMENT-DECLARATIONS);
        Ref lambda_list=list(S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_DECLARATIONS));}
    {   Ref fname=S(ENVIRONMENT-NEXT-ENVIRONMENT);
        Ref lambda_list=list(S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_NEXT_ENVIRONMENT));}
    {   Ref fname=S(ENVIRONMENT-FRAMES);
        Ref lambda_list=list(S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_FRAMES));}
    {   Ref fname=S(ENVIRONMENT-ADD-VARIABLE);
        Ref lambda_list=list(S(VARIABLE),S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_ADD_VARIABLE));}
    {   Ref fname=S(ENVIRONMENT-ADD-FUNCTION);
        Ref lambda_list=list(S(FUNCTION),S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_ADD_FUNCTION));}
    {   Ref fname=S(ENVIRONMENT-ADD-BLOCK);
        Ref lambda_list=list(S(NAME),S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_ADD_BLOCK));}
    {   Ref fname=S(ENVIRONMENT-ADD-CATCH);
        Ref lambda_list=list(S(TAG),S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_ADD_CATCH));}
    {   Ref fname=S(ENVIRONMENT-ADD-DECLARATION);
        Ref lambda_list=list(S(DECLARATION),S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_ADD_DECLARATION));}
    {   Ref fname=S(ENVIRONMENT-FIND-VARIABLE);
        Ref lambda_list=list(S(NAME),S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_FIND_VARIABLE));}
    {   Ref fname=S(ENVIRONMENT-FIND-FUNCTION);
        Ref lambda_list=list(S(NAME),S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_FIND_FUNCTION));}
    {   Ref fname=S(ENVIRONMENT-FIND-BLOCK);
        Ref lambda_list=list(S(NAME),S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_FIND_BLOCK));}
    {   Ref fname=S(ENVIRONMENT-FIND-CATCH);
        Ref lambda_list=list(S(TAG),S(ENVIRONMENT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENVIRONMENT_FIND_CATCH));}
    {   Ref fname=S(ENV-SPECIAL-VARIABLE-P);
        Ref lambda_list=list(S(OBJECT),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_ENV_SPECIAL_VARIABLE_P));}
    /* Slice 23 — MAKE-LIST, AREF, AREF-SET. */
    {   Ref fname=S(MAKE-LIST);
        Ref lambda_list=list(S(N),S(&REST),S(KEYS),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_MAKE_LIST));}
    {   Ref fname=S(AREF);
        Ref lambda_list=list(S(ARRAY),S(INDEX),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_AREF));}
    {   Ref fname=S(AREF-SET);
        Ref lambda_list=list(S(VALUE),S(ARRAY),S(INDEX),NULL);
        set_symbol_function(fname,built_in_function_new(fname,lambda_list,kernel_AREF_SET));}
}
/**** THE END ****/
