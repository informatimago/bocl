#include "built_in_functions.h"
#include "kernel_functions.h"
#include "kernel.h"
#include "cons.h"
#include "variable.h"
#include "stream.h"
#include "reader.h"
#include "printer.h"

void kernel_KERNEL_ROOTSET(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_rootset();
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CLASS_NEW(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = class_new(par_name);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CLASS_P(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(class_p(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CLASS_NAME(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = class_name(par_class);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CLASS_DIRECT_SUPERCLASSES(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = class_direct_superclasses(par_class);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CLASS_ADD_SUPERCLASS(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_class = pop_mandatory_argument(function,&current);
    Object* par_superclass = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=0;
    class_add_superclass(par_class, par_superclass);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CLASS_OF(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = class_of(par_object);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_FIND_CLASS(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_class_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = find_class(par_class_name);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SUBCLASSP(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_class = pop_mandatory_argument(function,&current);
    Object* par_superclass = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(subclassp(par_class, par_superclass));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_TYPEP(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    Object* par_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(typep(par_object, par_class));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CHECK_TYPE(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    Object* par_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=0;
    check_type(par_object, par_class);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CHECK_CLASS(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    Object* par_class = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=0;
    check_class(par_object, par_class);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_BUILT_IN_CLASS_NEW(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = built_in_class_new(par_name);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CONS(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_car = pop_mandatory_argument(function,&current);
    Object* par_cdr = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons(par_car, par_cdr);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CONSP(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(cons_p(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CONS_CAR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cell = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons_car(par_cell);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CONS_CDR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cell = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons_cdr(par_cell);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CONS_RPLACA(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cell = pop_mandatory_argument(function,&current);
    Object* par_newcar = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons_rplaca(par_cell, par_newcar);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CONS_RPLACD(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cell = pop_mandatory_argument(function,&current);
    Object* par_newcdr = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cons_rplacd(par_cell, par_newcdr);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_INTEGERP(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(integer_p(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_FLOATP(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(float_p(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CHARACTERP(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(character_p(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CHAR_CODE(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_character = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = character_code(par_character);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CHAR_NAME(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_character = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = character_name(par_character);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CODE_CHAR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_code = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = code_character(par_code);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_NAME_CHAR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = name_character(par_name);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STRING_LENGTH(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_string = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = integer_from_uword(string_length(par_string));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STRINGEQ(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_a = pop_mandatory_argument(function,&current);
    Object* par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(string_eq(par_a, par_b));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STRING_EQUAL(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_a = pop_mandatory_argument(function,&current);
    Object* par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(string_equal(par_a, par_b));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STRINGP(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(string_p(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_FIND_SYMBOL(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_name = pop_mandatory_argument(function,&current);
    Object* par_obarray = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = find_symbol(par_name, par_obarray);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_MAKE_SYMBOL(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_new(par_name);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_INTERN(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_intern(par_name);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_NAME(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_name(par_symbol);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_PACKAGE(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_package(par_symbol);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_VALUE(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_value(par_symbol);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_FUNCTION(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_function(par_symbol);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_PLIST(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_plist(par_symbol);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_SET_PACKAGE(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_symbol = pop_mandatory_argument(function,&current);
    Object* par_new_package = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_set_package(par_symbol, par_new_package);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_SET_VALUE(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_symbol = pop_mandatory_argument(function,&current);
    Object* par_new_value = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_set_value(par_symbol, par_new_value);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_SET_FUNCTION(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_symbol = pop_mandatory_argument(function,&current);
    Object* par_new_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_set_function(par_symbol, par_new_function);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOL_SET_PLIST(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_symbol = pop_mandatory_argument(function,&current);
    Object* par_new_plist = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = symbol_set_plist(par_symbol, par_new_plist);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_SYMBOLP(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(symbol_p(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_BOUNDP(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(symbol_boundp(par_symbol));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_FBOUNDP(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_symbol = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(symbol_fboundp(par_symbol));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_INTERN_KEYWORD(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_name = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = keyword_intern(par_name);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_KEYWORDP(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(keyword_p(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_NULL(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(null_p(par_object));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_NORMALIZE_EXTERNAL_FORMAT(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_external_format = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = normalize_external_format(par_external_format);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PHYSICAL_PATHNAME(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_namestring = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = physical_pathname(par_namestring);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_MAKE_FILE_STREAM(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_pathname = pop_mandatory_argument(function,&current);
    Object* par_element_type = pop_mandatory_argument(function,&current);
    Object* par_external_format = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = file_stream_new(par_pathname, par_element_type, par_external_format);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STREAM_PATHNAME(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_pathname(par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STREAM_ELEMENT_TYPE(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_element_type(par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STREAM_EXTERNAL_FORMAT(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_external_format(par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STREAM_DIRECTION(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = stream_direction(par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_OPEN_STREAM_P(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_stream = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(stream_open_p(par_stream));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STANDARD_INPUT_STREAM(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = standard_input_stream();
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STANDARD_OUTPUT_STREAM(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = standard_output_stream();
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_STANDARD_ERROR_STREAM(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = standard_error_stream();
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_RPLACA(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cell = pop_mandatory_argument(function,&current);
    Object* par_new_car = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = rplaca(par_cell, par_new_car);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_RPLACD(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cell = pop_mandatory_argument(function,&current);
    Object* par_new_car = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = rplacd(par_cell, par_new_car);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CAR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = car(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CDR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdr(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CAAR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caar(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CADR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cadr(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CDAR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdar(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CDDR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cddr(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CAAAR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caaar(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CAADR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caadr(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CADAR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cadar(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CADDR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = caddr(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CDAAR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdaar(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CDADR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdadr(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CDDAR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cddar(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_CDDDR(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_cons = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = cdddr(par_cons);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_LIST_LENGTH(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = integer_from_word(list_length(par_list));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_NCONC(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_list = pop_mandatory_argument(function,&current);
    Object* par_tail = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = nconc(par_list, par_tail);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_LAST(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = last(par_list);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_MEMQ(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_item = pop_mandatory_argument(function,&current);
    Object* par_list = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = memq(par_item, par_list);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_GETF(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_plist = pop_mandatory_argument(function,&current);
    Object* par_indicator = pop_mandatory_argument(function,&current);
    Object* def_default_value = variable_value(S(NIL));
    Object* par_default_value = pop_optional_argument(function,&current,def_default_value);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = getf(par_plist, par_indicator, par_default_value);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PUTF(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_plist = pop_mandatory_argument(function,&current);
    Object* par_indicator = pop_mandatory_argument(function,&current);
    Object* par_new_value = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = putf(par_plist, par_indicator, par_new_value);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_REMF(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_plist = pop_mandatory_argument(function,&current);
    Object* par_indicator = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(remf(par_plist, par_indicator));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_EQL(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_a = pop_mandatory_argument(function,&current);
    Object* par_b = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = as_boolean(eql(par_a, par_b));
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PATHNAME(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_pathname_designator = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = pathname(par_pathname_designator);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_NAMESTRING(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_pathname = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = namestring(par_pathname);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PRIN1_TO_STRING(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = prin1_to_string(par_object);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PRINT(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    Object* def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Object* par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = print(par_object, par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PRIN1(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    Object* def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Object* par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = prin1(par_object, par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_PRINC(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_object = pop_mandatory_argument(function,&current);
    Object* def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Object* par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = princ(par_object, par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_TERPRI(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* def_stream = variable_value(S(*STANDARD-OUTPUT*));
    Object* par_stream = pop_optional_argument(function,&current,def_stream);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = terpri(par_stream);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_KERNEL_READ(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* def_input_stream = variable_value(S(*STANDARD-INPUT*));
    Object* par_input_stream = pop_optional_argument(function,&current,def_input_stream);
    Object* def_eof_error_p = variable_value(S(T));
    Object* par_eof_error_p = pop_optional_argument(function,&current,def_eof_error_p);
    Object* def_eof_value = variable_value(S(NIL));
    Object* par_eof_value = pop_optional_argument(function,&current,def_eof_value);
    Object* def_recursive_p = variable_value(S(NIL));
    Object* par_recursive_p = pop_optional_argument(function,&current,def_recursive_p);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_read(par_input_stream, par_eof_error_p, par_eof_value, par_recursive_p);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_KERNEL_READ_PRESERVING_WHITESPACE(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* def_input_stream = variable_value(S(*STANDARD-INPUT*));
    Object* par_input_stream = pop_optional_argument(function,&current,def_input_stream);
    Object* def_eof_error_p = variable_value(S(T));
    Object* par_eof_error_p = pop_optional_argument(function,&current,def_eof_error_p);
    Object* def_eof_value = variable_value(S(NIL));
    Object* par_eof_value = pop_optional_argument(function,&current,def_eof_value);
    Object* def_recursive_p = variable_value(S(NIL));
    Object* par_recursive_p = pop_optional_argument(function,&current,def_recursive_p);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_read_preserving_whitespace(par_input_stream, par_eof_error_p, par_eof_value, par_recursive_p);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_KERNEL_READ_DELIMITED_LIST(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_character = pop_mandatory_argument(function,&current);
    Object* def_input_stream = variable_value(S(*STANDARD-INPUT*));
    Object* par_input_stream = pop_optional_argument(function,&current,def_input_stream);
    Object* def_recursive_p = variable_value(S(NIL));
    Object* par_recursive_p = pop_optional_argument(function,&current,def_recursive_p);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_read_delimited_list(par_character, par_input_stream, par_recursive_p);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_KERNEL_READ_FROM_STRING(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_input_string = pop_mandatory_argument(function,&current);
    Object* def_eof_error_p = variable_value(S(T));
    Object* par_eof_error_p = pop_optional_argument(function,&current,def_eof_error_p);
    Object* def_eof_value = variable_value(S(NIL));
    Object* par_eof_value = pop_optional_argument(function,&current,def_eof_value);
    Object* def_start = I(0);
    Object* par_start = pop_optional_argument(function,&current,def_start);
    Object* def_end = variable_value(S(NIL));
    Object* par_end = pop_optional_argument(function,&current,def_end);
    Object* def_preserve_whitespace = variable_value(S(NIL));
    Object* par_preserve_whitespace = pop_optional_argument(function,&current,def_preserve_whitespace);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = kernel_read_from_string(par_input_string, par_eof_error_p, par_eof_value, par_start, par_end, par_preserve_whitespace);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}

void kernel_OPEN(Object* function,Object* arguments,MultipleValues* results){
    push_to_rootset(function);
    push_to_rootset(arguments);
    Object* current=arguments;
    Object* par_filespec = pop_mandatory_argument(function,&current);
    Object* def_direction = K(INPUT);
    Object* par_direction = pop_key_argument(function,&current,K(DIRECTION),def_direction);
    Object* def_element_type = S(CHARACTER);
    Object* par_element_type = pop_key_argument(function,&current,K(ELEMENT-TYPE),def_element_type);
    Object* def_if_exists = K(ERROR);
    Object* par_if_exists = pop_key_argument(function,&current,K(IF-EXISTS),def_if_exists);
    Object* def_if_does_not_exist = K(CREATE);
    Object* par_if_does_not_exist = pop_key_argument(function,&current,K(IF-DOES-NOT-EXIST),def_if_does_not_exist);
    Object* def_external_format = K(DEFAULT);
    Object* par_external_format = pop_key_argument(function,&current,K(EXTERNAL-FORMAT),def_external_format);
    results->count=1;
    results->values[0] = stream_open(par_filespec, par_direction, par_element_type, par_if_exists, par_if_does_not_exist, par_external_format);
    delete_from_rootset(arguments);
    delete_from_rootset(function);
}


/**** THE END ****/
