#include "built_in_functions.h"
#include "kernel_functions.h"
#include "kernel.h"
#include "cons.h"
#include "variable.h"
#include "stream.h"
#include "reader.h"
#include "printer.h"

void kernel_KERNEL_ROOTSET(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_rootset();
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CLASS_NEW(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = class_new(par_name);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CLASSP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(classp(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CLASS_NAME(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = class_name(par_class);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CLASS_DIRECT_SUPERCLASSES(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = class_direct_superclasses(par_class);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CLASS_ADD_SUPERCLASS(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_class = pop_mandatory_argument(function,&current);
    Ref par_superclass = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=0;
    class_add_superclass(par_class, par_superclass);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CLASS_OF(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = class_of(par_object);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_FIND_CLASS(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_class_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = find_class(par_class_name);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SUBCLASSP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_class = pop_mandatory_argument(function,&current);
    Ref par_superclass = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(subclassp(par_class, par_superclass));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_TYPEP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref par_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(typep(par_object, par_class));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CHECK_TYPE(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref par_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=0;
    check_type(par_object, par_class);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CHECK_CLASS(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref par_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=0;
    check_class(par_object, par_class);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_BUILT_IN_CLASS_NEW(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = built_in_class_new(par_name);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CONS(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_car = pop_mandatory_argument(function,&current);
    Ref par_cdr = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons(par_car, par_cdr);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CONSP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(consp(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CONS_CAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cell = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons_car(par_cell);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CONS_CDR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cell = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons_cdr(par_cell);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CONS_RPLACA(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cell = pop_mandatory_argument(function,&current);
    Ref par_newcar = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons_rplaca(par_cell, par_newcar);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CONS_RPLACD(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cell = pop_mandatory_argument(function,&current);
    Ref par_newcdr = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons_rplacd(par_cell, par_newcdr);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_INTEGERP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(integerp(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_FLOATP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(floatp(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CHARACTERP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(characterp(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CHAR_CODE(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_character = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = character_code(par_character);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CHAR_INT(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_character = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = character_code(par_character);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CHAR_NAME(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_character = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = character_name(par_character);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CODE_CHAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_code = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = code_character(par_code);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_NAME_CHAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = name_character(par_name);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_FIND_SYMBOL(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    Ref par_obarray = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = find_symbol(par_name, par_obarray);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_MAKE_SYMBOL(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_new(par_name);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_INTERN(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_intern(par_name);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_NAME(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_name(par_symbol);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_PACKAGE(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_package(par_symbol);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_VALUE(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_value(par_symbol);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_FUNCTION(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_function(par_symbol);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_PLIST(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_plist(par_symbol);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_SET_PACKAGE(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    Ref par_new_package = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_set_package(par_symbol, par_new_package);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_SET_VALUE(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    Ref par_new_value = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_set_value(par_symbol, par_new_value);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_SET_FUNCTION(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    Ref par_new_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_set_function(par_symbol, par_new_function);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_SET_PLIST(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    Ref par_new_plist = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_set_plist(par_symbol, par_new_plist);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOLP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(symbolp(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_BOUNDP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(symbol_boundp(par_symbol));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_FBOUNDP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(symbol_fboundp(par_symbol));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_INTERN_KEYWORD(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = keyword_intern(par_name);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_KEYWORDP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(keywordp(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_OBARRAY_ALL_SYMBOLS(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_obarray = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = obarray_all_symbols(par_obarray);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_NULL(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(nullp(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_NORMALIZE_EXTERNAL_FORMAT(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_external_format = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = normalize_external_format(par_external_format);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PHYSICAL_PATHNAME(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_namestring = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = physical_pathname(par_namestring);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PHYSICAL_PATHNAME_EQUAL(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_a = pop_mandatory_argument(function,&current);
    Ref par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(physical_pathname_equal(par_a, par_b));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PHYSICAL_PATHNAME_P(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(physical_pathname_p(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PATHNAMEP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(physical_pathname_p(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_MAKE_FILE_STREAM(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_pathname = pop_mandatory_argument(function,&current);
    Ref par_element_type = pop_mandatory_argument(function,&current);
    Ref par_external_format = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = file_stream_new(par_pathname, par_element_type, par_external_format);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STREAM_PATHNAME(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_pathname(par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STREAM_ELEMENT_TYPE(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_element_type(par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STREAM_EXTERNAL_FORMAT(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_external_format(par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STREAM_DIRECTION(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_direction(par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_OPEN_STREAM_P(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(stream_open_p(par_stream));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STANDARD_INPUT_STREAM(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = standard_input_stream();
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STANDARD_OUTPUT_STREAM(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = standard_output_stream();
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STANDARD_ERROR_STREAM(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = standard_error_stream();
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CLOSUREP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(closurep(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_FUNCTION_NAME(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = function_name(par_function);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_RPLACA(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cell = pop_mandatory_argument(function,&current);
    Ref par_new_car = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = rplaca(par_cell, par_new_car);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_RPLACD(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cell = pop_mandatory_argument(function,&current);
    Ref par_new_car = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = rplacd(par_cell, par_new_car);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = car(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CDR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdr(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CAAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caar(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CADR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cadr(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CDAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdar(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CDDR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cddr(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CAAAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caaar(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CAADR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caadr(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CADAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cadar(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CADDR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caddr(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CDAAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdaar(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CDADR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdadr(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CDDAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cddar(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CDDDR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdddr(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_LIST_LENGTH(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = integer_from_uword(list_length(par_list));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_NCONC(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_list = pop_mandatory_argument(function,&current);
    Ref par_tail = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = nconc(par_list, par_tail);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_LAST(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = last(par_list);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_MEMQ(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_item = pop_mandatory_argument(function,&current);
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = memq(par_item, par_list);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_COPY_LIST(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = copy_list(par_list);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_LIST(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_elements = current;
    results->count=1;
    results->values[0] = copy_list(par_elements);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_DELETE(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = delete(par_object, par_list);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_NREVERSE(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = nreverse(par_list);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_REVERSE(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = reverse(par_list);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_GETF(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_plist = pop_mandatory_argument(function,&current);
    Ref par_indicator = pop_mandatory_argument(function,&current);
    Ref def_default_value = variable_value(S(NIL));
    Ref par_default_value = pop_optional_argument(function,&current,def_default_value);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = getf(par_plist, par_indicator, par_default_value);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PUTF(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_plist = pop_mandatory_argument(function,&current);
    Ref par_indicator = pop_mandatory_argument(function,&current);
    Ref par_new_value = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = putf(par_plist, par_indicator, par_new_value);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_REMF(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_plist = pop_mandatory_argument(function,&current);
    Ref par_indicator = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(remf(par_plist, par_indicator));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_VECTOR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_elements = current;
    results->count=1;
    results->values[0] = vector_new(par_elements);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_VECTORP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(vectorp(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_VECTOR_LENGTH(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_vector = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = integer_from_uword(vector_length(par_vector));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SVREF(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_vector = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = svref(par_vector, par_index);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SVREF_SET(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_vector = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    Ref par_new_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = svref_set(par_vector, par_index, par_new_object);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_BIT_VECTOR_P(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(bit_vector_p(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_BIT_VECTOR_LENGTH(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_bit_vector = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = integer_from_uword(bit_vector_length(par_bit_vector));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_BIT(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_bit_vector = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = bit(par_bit_vector, par_index);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SBIT(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_bit_vector = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = bit(par_bit_vector, par_index);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_BIT_SET(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_bit_vector = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    Ref par_new_bit = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = bit_set(par_bit_vector, par_index, par_new_bit);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SBIT_SET(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_bit_vector = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    Ref par_new_bit = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = bit_set(par_bit_vector, par_index, par_new_bit);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_BIT_VECTOR_EQUAL(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_a = pop_mandatory_argument(function,&current);
    Ref par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(bit_vector_equal(par_a, par_b));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STRING_LENGTH(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_string = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = integer_from_uword(string_length(par_string));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STRINGEQ(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_a = pop_mandatory_argument(function,&current);
    Ref par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(string_eq(par_a, par_b));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STRING_EQUAL(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_a = pop_mandatory_argument(function,&current);
    Ref par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(string_equal(par_a, par_b));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STRINGP(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(stringp(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CHAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_string = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = char_ref(par_string, par_index);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SCHAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_string = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = char_ref(par_string, par_index);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SET_CHAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_string = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    Ref par_new_char = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = char_set(par_string, par_index, par_new_char);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SET_SCHAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_string = pop_mandatory_argument(function,&current);
    Ref par_index = pop_mandatory_argument(function,&current);
    Ref par_new_char = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = char_set(par_string, par_index, par_new_char);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_EQL(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_a = pop_mandatory_argument(function,&current);
    Ref par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(eql(par_a, par_b));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_EQUAL(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_a = pop_mandatory_argument(function,&current);
    Ref par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(equal(par_a, par_b));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PATHNAME(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_pathname_designator = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = pathname(par_pathname_designator);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_NAMESTRING(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_pathname = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = namestring(par_pathname);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PRIN1_TO_STRING(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = prin1_to_string(par_object);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PRINT(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Ref par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = print(par_object, par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PRIN1(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Ref par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = prin1(par_object, par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PRINC(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_object = pop_mandatory_argument(function,&current);
    Ref def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Ref par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = princ(par_object, par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_TERPRI(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Ref par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = terpri(par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_KERNEL_READ_CHAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref def_input_stream = variable_value(S(*STANDARD-INPUT*));
    Ref par_input_stream = pop_optional_argument(function,&current,def_input_stream);
    Ref def_eof_error_p = variable_value(S(T));
    Ref par_eof_error_p = pop_optional_argument(function,&current,def_eof_error_p);
    Ref def_eof_value = variable_value(S(NIL));
    Ref par_eof_value = pop_optional_argument(function,&current,def_eof_value);
    Ref def_recursive_p = variable_value(S(NIL));
    Ref par_recursive_p = pop_optional_argument(function,&current,def_recursive_p);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_read_char(par_input_stream, par_eof_error_p, par_eof_value, par_recursive_p);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_KERNEL_PEEK_CHAR(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_peek_type = pop_mandatory_argument(function,&current);
    Ref def_input_stream = variable_value(S(*STANDARD-INPUT*));
    Ref par_input_stream = pop_optional_argument(function,&current,def_input_stream);
    Ref def_eof_error_p = variable_value(S(T));
    Ref par_eof_error_p = pop_optional_argument(function,&current,def_eof_error_p);
    Ref def_eof_value = variable_value(S(NIL));
    Ref par_eof_value = pop_optional_argument(function,&current,def_eof_value);
    Ref def_recursive_p = variable_value(S(NIL));
    Ref par_recursive_p = pop_optional_argument(function,&current,def_recursive_p);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_peek_char(par_peek_type, par_input_stream, par_eof_error_p, par_eof_value, par_recursive_p);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_KERNEL_READ(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref def_input_stream = variable_value(S(*STANDARD-INPUT*));
    Ref par_input_stream = pop_optional_argument(function,&current,def_input_stream);
    Ref def_eof_error_p = variable_value(S(T));
    Ref par_eof_error_p = pop_optional_argument(function,&current,def_eof_error_p);
    Ref def_eof_value = variable_value(S(NIL));
    Ref par_eof_value = pop_optional_argument(function,&current,def_eof_value);
    Ref def_recursive_p = variable_value(S(NIL));
    Ref par_recursive_p = pop_optional_argument(function,&current,def_recursive_p);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_read(par_input_stream, par_eof_error_p, par_eof_value, par_recursive_p);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_KERNEL_READ_PRESERVING_WHITESPACE(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref def_input_stream = variable_value(S(*STANDARD-INPUT*));
    Ref par_input_stream = pop_optional_argument(function,&current,def_input_stream);
    Ref def_eof_error_p = variable_value(S(T));
    Ref par_eof_error_p = pop_optional_argument(function,&current,def_eof_error_p);
    Ref def_eof_value = variable_value(S(NIL));
    Ref par_eof_value = pop_optional_argument(function,&current,def_eof_value);
    Ref def_recursive_p = variable_value(S(NIL));
    Ref par_recursive_p = pop_optional_argument(function,&current,def_recursive_p);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_read_preserving_whitespace(par_input_stream, par_eof_error_p, par_eof_value, par_recursive_p);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_KERNEL_READ_DELIMITED_LIST(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_character = pop_mandatory_argument(function,&current);
    Ref def_input_stream = variable_value(S(*STANDARD-INPUT*));
    Ref par_input_stream = pop_optional_argument(function,&current,def_input_stream);
    Ref def_recursive_p = variable_value(S(NIL));
    Ref par_recursive_p = pop_optional_argument(function,&current,def_recursive_p);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_read_delimited_list(par_character, par_input_stream, par_recursive_p);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_KERNEL_READ_FROM_STRING(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_input_string = pop_mandatory_argument(function,&current);
    Ref def_eof_error_p = variable_value(S(T));
    Ref par_eof_error_p = pop_optional_argument(function,&current,def_eof_error_p);
    Ref def_eof_value = variable_value(S(NIL));
    Ref par_eof_value = pop_optional_argument(function,&current,def_eof_value);
    Ref def_start = I(0);
    Ref par_start = pop_optional_argument(function,&current,def_start);
    Ref def_end = variable_value(S(NIL));
    Ref par_end = pop_optional_argument(function,&current,def_end);
    Ref def_preserve_whitespace = variable_value(S(NIL));
    Ref par_preserve_whitespace = pop_optional_argument(function,&current,def_preserve_whitespace);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_read_from_string(par_input_string, par_eof_error_p, par_eof_value, par_start, par_end, par_preserve_whitespace);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_MAKE_STRING_INPUT_STREAM(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_string = pop_mandatory_argument(function,&current);
    Ref def_start = variable_value(S(NIL));
    Ref par_start = pop_optional_argument(function,&current,def_start);
    Ref def_end = variable_value(S(NIL));
    Ref par_end = pop_optional_argument(function,&current,def_end);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_input_string(par_string, par_start, par_end);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_MAKE_STRING_OUTPUT_STREAM(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref def_element_type = variable_value(S(NIL));
    Ref par_element_type = pop_key_argument(function,&current,K(ELEMENT-TYPE),def_element_type);
    results->count=1;
    results->values[0] = stream_output_string(par_element_type);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_GET_OUTPUT_STREAM_STRING(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref par_string_output_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = string_output_stream_get_string(par_string_output_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_OPEN(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
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
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_FINISH_OUTPUT(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Ref par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_finish_output(par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_FORCE_OUTPUT(Ref function,Ref arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Ref current=arguments;
    Ref def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Ref par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_finish_output(par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}


/**** THE END ****/
