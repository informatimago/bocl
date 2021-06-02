#ifndef kernel_h
#define kernel_h
#define _POSIX_SOURCE
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include "memory.h"
#include "macros.h"
#include "limits.h"
#include "cons.h"
#include "kernel_functions.h"
#include "built_in_functions.h"


/* Only the "C" functions are declared here.  The declarations of Lisp
functions implemented in the kernel are generated automatically by
generate-built-in-functions.lisp into kernel_functions.h */

void kernel_initialize(void);

void push_to_rootset(Object* object);
void delete_from_rootset(Object* object);

void check_no_more_arguments(Object* function,Object* arguments);
Object* pop_mandatory_argument(Object* function,Object** arguments);
Object* pop_optional_argument(Object* function,Object** arguments,Object* default_value);
Object* pop_key_argument(Object* function,Object** arguments,Object* keyword,Object* default_value);

/* Lisp Objects */

Object* as_boolean(bool value);
bool is_true(Object* object);

/* CLASS */

/* BUILT-IN-CLASS */

/* CONS */

/* INTEGER */

Object* integer_from_word(word value);
Object* integer_from_uword(uword value);
word integer_value(Object* object);

/* FLOAT */

Object* float_from_floating(floating value);
floating float_value(Object* object);

/* CHARACTER */

/* STRING */

Object* string_new(halfword length);
Object* string_new_cstring(const char* string);
char* string_cstring(Object* string);

/* SYMBOL */

Object* symbol_new_cstring(const char* name);
Object* symbol_intern_cstring(const char* name);

/* Keyword */

Object* keyword_intern_cstring(const char* name);

/* Null */


/* External-Format */

Object* normalize_encoding(Object* encoding_o,encoding_t* encoding);
Object* normalize_new_line(Object* new_line_o,new_line_t* new_line);
Object* normalize_on_error(Object* on_error_o,on_error_t* on_error);
Object* normalize_external_format_internal(Object* external_format,
                                           encoding_t* encoding,
                                           new_line_t* new_line,
                                           on_error_t* on_error);
Object* convert_from_direction(direction_t direction);
direction_t convert_direction(Object* direction);
type_t convert_element_type(Object* element_type);


/* PHYSICAL-PATHNAME */

Object* physical_pathname_new(VectorOfOctet* path);
Object* physical_pathname_new_cstring(const char* path);

/* FILESTREAM */

FILE* stream_cstream(Object* stream);

/* FUNCTIONS */

void kernel_apply(Object* function_or_closure,Object* arguments,MultipleValues* results);
void kernel_apply_closure(Closure* closure,Object* arguments,MultipleValues* results);
void kernel_apply_function(Function* function,Object* arguments,MultipleValues* results);
void kernel_apply_built_in(BuiltInFunction* function,Object* arguments,MultipleValues* results);

#endif
