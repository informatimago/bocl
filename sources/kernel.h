#define _POSIX_SOURCE   1
#define _ISOC11_SOURCE  1
#define _POSIX_C_SOURCE 200809L

#ifndef kernel_h
#define kernel_h

#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include "memory.h"
#include "boerror.h"
#include "macros.h"
#include "bolimits.h"
#include "cons.h"
#include "kernel_types.h"
#include "kernel_objects.h"
#include "kernel_functions.h"
#include "built_in_functions.h"


/* Only the "C" functions are declared here.  The declarations of Lisp
functions implemented in the kernel are generated automatically by
generate-built-in-functions.lisp into kernel_functions.h */

void kernel_initialize(void);

void push_to_rootset(Ref object);
void delete_from_rootset(Ref object);

void check_no_more_arguments(Ref function,Ref arguments);
Ref pop_mandatory_argument(Ref function,Ref* arguments);
Ref pop_optional_argument(Ref function,Ref* arguments,Ref default_value);
Ref pop_key_argument(Ref function,Ref* arguments,Ref keyword,Ref default_value);

/* Lisp Objects */

Ref as_boolean(bool value);
bool is_true(Ref object);

/* CLASS */

/* BUILT-IN-CLASS */

/* CONS */

/* INTEGER */

Ref integer_from_word(word value);
Ref integer_from_uword(uword value);
word integer_value(Ref object);

/* FLOAT */

Ref float_from_floating(floating value);
floating float_value(Ref object);

/* CHARACTER */

Ref octet_character(octet code);

/* STRING */

Ref string_new(halfword length);
Ref string_new_cstring(const char* string);
Ref string_new_vector_of_octets(VectorOfOctet* octets);
char* string_cstring(Ref string);
octet* string_octets(Ref string);

/* SYMBOL */

Ref symbol_new_cstring(const char* name);
Ref symbol_intern_cstring(const char* name);

/* Keyword */

Ref keyword_intern_cstring(const char* name);

/* Null */

/* BIT-VECTOR */

Ref bit_vector_new_octets(uword length,octet* bits);

/* External-Format */

Ref normalize_encoding(Ref encoding_o,encoding_t* encoding);
Ref normalize_new_line(Ref new_line_o,new_line_t* new_line);
Ref normalize_on_error(Ref on_error_o,on_error_t* on_error);
Ref normalize_external_format_internal(Ref external_format,
                                           encoding_t* encoding,
                                           new_line_t* new_line,
                                           on_error_t* on_error);
Ref convert_from_direction(direction_t direction);
direction_t convert_direction(Ref direction);
type_t convert_element_type(Ref element_type);


/* PHYSICAL-PATHNAME */

Ref physical_pathname_new(VectorOfOctet* path);
Ref physical_pathname_new_cstring(const char* path);

/* FILESTREAM */

FILE* stream_cstream(Ref stream);
type_t stream_type(Ref stream);

Ref string_stream_new(FILE* memstream);

/* FUNCTIONS */

BuiltInFunction* BuiltInFunction_slots(Ref lisp_name,Ref lambda_list,KernelFunctionPtr kernel_function);
Ref built_in_function_new(Ref lisp_name,Ref lambda_list,KernelFunctionPtr kernel_function);

void kernel_apply(Ref function_or_closure,Ref arguments,MultipleValues* results);
void kernel_apply_closure(Ref closure,Closure* slots,Ref arguments,MultipleValues* results);
void kernel_apply_function(Ref function,Function* slots,Ref arguments,MultipleValues* results);
void kernel_apply_built_in(Ref function,BuiltInFunction* slots,Ref arguments,MultipleValues* results);
#endif
