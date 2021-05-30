#ifndef kernel_h
#define kernel_h
#include <stdint.h>
#include <stdbool.h>
#include "memory.h"
#include "macros.h"

#include "cons.h"


Object* kernel_rootset(void);
void kernel_initialize(void);


/* Internal Object Structure */

Object*                   Object_new                     (Object*  class,        Slots*   slots);

Array*                    Array_slots                    (Object*  element_type, halfword rank, halfword* dimensions);
BuiltInClass*             BuiltInClass_slots             (Object*  name,         Object*  superclasses);
Character*                Character_slots                (Object*  name,         Object*  code);
Class*                    Class_slots                    (Object*  name,         Object*  superclasses);
Cons*                     Cons_slots                     (Object*  car,          Object*  cdr);
Float*                    Float_slots                    (floating value);
FuncallableStandardClass* FuncallableStandardClass_slots (Object*  name,         Object*  superclasses);
Integer*                  Integer_slots                  (word     value);
OctetArray*               OctetArray_slots               (Object*  element_type, halfword rank, halfword* dimensions);
OctetArray*               OctetArray_slots_cstring       (Object*  element_type, const char* initial_value);
StandardClass*            StandardClass_slots            (Object*  name,         Object*  superclasses);
StructureClass*           StructureClass_slots           (Object*  name,         Object*  superclasses);
Symbol*                   Symbol_slots                   (Object*  name,         Object*  package);
VectorOfObject*           VectorOfObject_slots           (halfword size);
VectorOfOctet*            VectorOfOctet_slots            (halfword length);
VectorOfOctet*            VectorOfOctet_slots_cstring    (const char* value);
VectorOfUWord*            VectorOfUWord_slots            (halfword size);


/* Lisp Objects */

Object* as_boolean(bool value);
bool is_true(Object* object);

/* CLASS */

bool typep_p(Object* object,Object* class);
/* whether object is a direct instance of class, or one of its subclasses */

Object* class_new(Object* name);
bool class_p(Object* object);
Object* class_name(Object* class);
Object* class_direct_superclasses(Object* class);
void class_add_superclass(Object* class, Object* superclass);

Object* class_of(Object* object);
Object* find_class(Object* name);
bool subclassp(Object* class,Object* superclass);
bool typep(Object* object,Object* class);
void check_type(Object* object,Object* class);
void check_class(Object* object,Object* class);

/* BUILT-IN-CLASS */

Object* built_in_class_new(Object* name);

/* CONS */

Object* cons(Object* car,Object* cdr);
bool cons_p(Object* object);
Object* cons_car(Object* cell);
Object* cons_cdr(Object* cell);
Object* cons_rplaca(Object* cell,Object* newcar);
Object* cons_rplacd(Object* cell,Object* newcdr);


/* INTEGER */

Object* integer_from_word(word value);
bool integer_p(Object* object);
word integer_value(Object* object);

/* FLOAT */

Object* float_from_floating(floating value);
bool float_p(Object* object);
floating float_value(Object* object);

/* CHARACTER */

bool character_p(Object* object);
Object* character_code(Object* object);
Object* character_name(Object* object);

/* STRING */

Object* string_new(halfword length);
Object* string_new_cstring(const char* string);
char* string_cstring(Object* string);
uword string_length(Object* string);
bool string_eq(Object* a,Object* b);
bool string_p(Object* object);

/* SYMBOL */

Object* find_symbol(Object* name,Object* obarray);

Object* symbol_new(Object* name);
Object* symbol_new_cstring(const char* name);
Object* symbol_intern(Object* name);
Object* symbol_intern_cstring(const char* name);

Object* symbol_name(Object* symbol);
Object* symbol_package(Object* symbol);
Object* symbol_value(Object* symbol);
Object* symbol_function(Object* symbol);
Object* symbol_plist(Object* symbol);

Object* symbol_set_package(Object* symbol,Object* new_package);
Object* symbol_set_value(Object* symbol,Object* new_value);
Object* symbol_set_function(Object* symbol,Object* new_function);
Object* symbol_set_plist(Object* symbol,Object* new_plist);

bool symbol_p(Object* object);
bool symbol_boundp(Object* symbol);
bool symbol_fboundp(Object* symbol);

/* Keyword */

Object* keyword_intern(Object* name);
Object* keyword_intern_cstring(const char* name);
bool keyword_p(Object* object);

/* Null */

bool null_p(Object* object);

/* External-Format */

Object* normalize_encoding(Object* encoding_o,encoding_t* encoding);
Object* normalize_new_line(Object* new_line_o,new_line_t* new_line);
Object* normalize_on_error(Object* on_error_o,on_error_t* on_error);
Object* normalize_external_format(Object* external_format,
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

Object* FileStream_new(Object* pathname,     /* a PHYSICAL-PATHNAME */
                       Object* element_type, /* CHARACTER or (UNSIGNED-BYTE 8) or BIT */
                       Object* external_format);

Object* stream_pathname(Object* stream);
Object* stream_element_type(Object* stream);
Object* stream_external_format(Object* stream);
Object* stream_direction(Object* stream);
bool stream_open_p(Object* stream);
FILE* stream_cstream(Object* stream);

Object* standard_input_stream(void);
Object* standard_output_stream(void);
Object* standard_error_stream(void);

#endif
