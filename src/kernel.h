#ifndef kernel_h
#define kernel_h
#include <stdint.h>
#include <stdbool.h>
#include "memory.h"

Object* rootset();
void kernel_initialize(void);

Object* Object_new(Object* class,Slots* slots);
VectorOfObject* VectorOfObject_slots(halfword size);
VectorOfUWord* VectorOfUWord_slots(halfword size);
VectorOfOctet* VectorOfOctet_slots(halfword length);
VectorOfOctet* VectorOfOctet_slots_cstring(char* value);
Character* Character_slots(Object* name,Object* code);
Integer* Integer_slots(word value);
Float* Float_slots(floating value);
Array* Array_slots(Object* element_type,halfword rank,uword* dimensions);
OctetArray* OctetArray_slots(Object* element_type,halfword rank,uword* dimensions);
OctetArray* OctetArray_slots_cstring(Object* element_type,char* initial_value);
Object* String_new(uword length);
Object* String_new_cstring(char* string);
Cons* Cons_slots(Object* car,Object* cdr);
Symbol* Symbol_slots(Object* name,Object* package);
Object* PhysicalPathname_new(VectorOfOctet* path);
Object* PhysicalPathname_new_cstring(char* path);

VectorOfObject* VectorOfObject_slots(halfword size);
VectorOfUWord* VectorOfUWord_slots(halfword size);
VectorOfOctet* VectorOfOctet_slots(halfword length);
Character* Character_slots(Object* name,Object* code);
Integer* Integer_slots(word value);
Float* Float_slots(floating value);
Array* Array_slots(Object* element_type,
                   halfword rank,
                   uword* dimensions);
OctetArray* OctetArray_slots(Object* element_type,
                             halfword rank,
                             uword* dimensions);
Cons* Cons_slots(Object* car,Object* cdr);
Symbol* Symbol_slots(Object* name,Object* package);
Class* Class_slots(Object* name,Object* superclasses);
StandardClass* StandardClass_slots(Object* name,Object* superclasses);
BuiltInClass* BuiltInClass_slots(Object* name,Object* superclasses);
StructureClass* StructureClass_slots(Object* name,Object* superclasses);
FuncallableStandardClass* FuncallableStandardClass_slots(Object* name,Object* superclasses);

/* Lisp Objects */


Object* as_boolean(bool value);
bool is_true(Object* object);

/* Class */

Object* find_class(Object* name);
bool class_p(Object* object,Object* class);
Object* class_of(Object* object);
Object* class_name(Object* class);
Object* class_direct_superclasses(Object* class);
bool instance_of(Object* object,Object* class);
void check_class(Object* object,Object* class);
Object* Class_new(Object* name);
Object* BuiltInClass_new(Object* name);
void class_add_superclass(Object* class, Object* superclass);
void boot_class_add_superclass(Object* class, Object* superclass);


/* Cons */

#define do_list(elementvar,listexpr,body)                                       \
    do{                                                                         \
        Object* do_list_current ##  __LINE__ =(listexpr);                       \
        while(cons_p(do_list_current ## __LINE__)){                             \
            Object* elementvar=cons_car(do_list_current ## __LINE__);           \
            do_list_current ## __LINE__=cons_cdr(do_list_current ## __LINE__);  \
            { body }}}while(0)


Object* cons(Object* car,Object* cdr);
bool cons_p(Object* object);
Object* cons_car(Object* cell);
Object* cons_cdr(Object* cell);
Object* cons_rplaca(Object* cell,Object* newcar);
Object* cons_rplacd(Object* cell,Object* newcdr);

/* Integer */
Object* integer_from_word(word value);
bool integer_p(Object* object);
word integer_value(Object* object);

/* Float */
Object* float_from_floating(floating value);
bool float_p(Object* object);
floating float_value(Object* object);

/* String */

Object* string_new(uword length);
Object* string_new_cstring(char* string);
char* string_cstring(Object* string);
bool string_eq(Object* a,Object* b);
bool string_p(Object* object);

/* Symbol */

Object* symbol_intern(Object* name);
Object* symbol_intern_cstring(char* name);
Object* make_symbol_cstring(char* name);
Object* symbol_new(Object* name);
Object* symbol_new_cstring(char* name);
Object* symbol_name(Object* symbol);
Object* symbol_package(Object* symbol);
bool symbol_boundp(Object* symbol);
bool symbol_fboundp(Object* symbol);
Object* symbol_value(Object* symbol);
Object* symbol_function(Object* symbol);
Object* symbol_plist(Object* symbol);

/* Null */

bool null_p(Object* object);

/* List */

Object* nconc(Object* list,Object* tail);
Object* last(Object* list);
Object* list(Object* first_element,...);


#endif
