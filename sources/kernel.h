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
#include "kernel_structure_functions.h"
#include "structure_functions.h"


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

/* An INTEGER's value as a floating.  Use this instead of casting INTEGER_VALUE's
   WORD result straight to double: a direct (floating)integer_value(x) is a cast of
   a function-call result to a non-matching type, which trips -Wbad-function-cast. */
static inline floating integer_value_as_floating(Ref object){
    word value = integer_value(object);
    return (floating)value;
}

/* BIGNUM — arbitrary-precision integer; (SIGN . DIGITS) i%-format cons, base 10^9. */
extern Ref BIGNUM_Class;
bool bignump(Ref object);
Ref  bignum_new(Ref value);                       /* value = (sign . digits) cons */
Ref  bignum_value(Ref object);
Ref  make_integer_from_bignum(Ref signed_bignum); /* demote to fixnum if it fits */
void kernel_initialize_bignum(void);

/* C bignum engine (kernel.c) — schoolbook arithmetic over base-10^9 limbs.
   Operands are fixnum-or-bignum; results demote to a fixnum when they fit. */
Ref  c_bignum_add(Ref a, Ref b);
Ref  c_bignum_sub(Ref a, Ref b);
Ref  c_bignum_mul(Ref a, Ref b);
Ref  c_bignum_divide(Ref a, Ref b);               /* exact; else signals (no bignum ratios) */
int  c_bignum_cmp(Ref a, Ref b);                  /* -1 / 0 / 1 */
void c_bignum_divmod(Ref a, Ref b, char mode, Ref* q_out, Ref* r_out); /* t/f/c/r */
Ref  c_bignum_gcd(Ref a, Ref b);
Ref  c_bignum_lcm(Ref a, Ref b);
void print_bignum(Ref object, FILE* out);          /* decimal, straight to OUT */
void print_bignum_radix(Ref object, FILE* out, int base);  /* BASE 2..36 */
Ref  parse_bignum_cstring(const char* token, int base);

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

Ref  keyword_intern_cstring(const char* name);
bool keywordp(Ref object);

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

/* Non-local exit codes shared between eval.c and conditions.c */
typedef enum {
    non_local_exit_initial = 0,
    non_local_exit_unwind  = 1,
    non_local_exit_target  = 2,
} non_local_exit_t;

/* FUNCTIONS */

BuiltInFunction* BuiltInFunction_slots(Ref lisp_name,Ref lambda_list,KernelFunctionPtr kernel_function);
Ref built_in_function_new(Ref lisp_name,Ref lambda_list,KernelFunctionPtr kernel_function);

void kernel_apply(Ref function_or_closure,Ref arguments,MultipleValues* results);
void kernel_apply_closure(Ref closure,Closure* slots,Ref arguments,MultipleValues* results);
void kernel_apply_function(Ref function,Function* slots,Ref arguments,MultipleValues* results);
void kernel_apply_built_in(Ref function,BuiltInFunction* slots,Ref arguments,MultipleValues* results);
/* CLOS bridge predicates — set after (%clos-register …) */
bool clos_instance_p(Ref obj);
bool clos_class_p(Ref cls);
#endif
