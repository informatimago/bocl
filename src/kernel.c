#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "kernel.h"
#include "kernel_types.h"
#include "kernel_objects.h"

// Object

Object* Object_new(Object* class,Slots* slots){
    Object* that=allocate(sizeof(*that));
    that->objcount=2;
    that->class=class;
    that->slots=slots;
    return that;}

Object* make_boot_Object(Slots* slots){
    Object* that=allocate(sizeof(*that));
    that->objcount=2;
    that->class=NULL;
    that->slots=slots;
    return that;}

// VectorOfObject

VectorOfObject* VectorOfObject_slots(halfword size){
    VectorOfObject* that=allocate(sizeof(*that)+sizeof(that->elements[0])*size);
    that->objcount=size;
    for(halfword i=0;i<size;i++){
        that->elements[i]=NIL_Symbol;}
    return that;}

// VectorOfUWord

VectorOfUWord* VectorOfUWord_slots(halfword size){
    VectorOfUWord* that=allocate(sizeof(*that)+sizeof(that->elements[0])*size);
    for(halfword i=0;i<size;i++){
        that->elements[i]=0;}
    return that;}

// VectorOfOctet

VectorOfOctet* VectorOfOctet_slots(halfword length){
    VectorOfOctet* that=allocate(sizeof(*that)+sizeof(that->elements[0])*length);
    that->length=length;
    for(halfword i=0;i<length;i++){
        that->elements[i]=0;}
    return that;}

VectorOfOctet* VectorOfOctet_slots_cstring(char* value){
    size_t len=strlen(value);
    if(HALFWORD_MAX<len){
        error(__FUNCTION__,"string too long");}
    halfword length=(halfword)len;
    VectorOfOctet* that=allocate(sizeof(*that)+sizeof(that->elements[0])*length);
    that->length=length;
    for(halfword i=0;i<length;i++){
        that->elements[i]=value[i];}
    return that;}

// Character

Character* Character_slots(Object* name,Object* code){
    Character* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->code=code;
    return that;}

// Integer
// Before we use a bigint library, let's just use 64-bit integers:

Integer* Integer_slots(word value){
    Integer* that=allocate(sizeof(*that));
    that->value=value;
    return that;}

// Float

Float* Float_slots(floating value){
    Float* that=allocate(sizeof(*that));
    that->value=value;
    return that;}

// Array
// used both for ARRAY and VECTOR classes,
// as long as element-type is not octet, bit or character.

Array* Array_slots(Object* element_type,
                   halfword rank,
                   uword* dimensions){
    uword total_array_size=1;
    for(halfword i=0;i<rank;i++){
        total_array_size*=dimensions[i];}
    VectorOfObject* elements=VectorOfObject_slots(total_array_size);
    Array* that=allocate(sizeof(*that)+rank*sizeof(that->dimensions[0]));
    that->objcount=2;
    that->element_type=element_type;
    that->elements=elements;
    for(halfword i=0;i<rank;i++){
        that->dimensions[i]=dimensions[i];}
    return that;}


// OctetArray
// used both for ARRAY and VECTOR classes when
// element-type is octet, bit or character.

OctetArray* OctetArray_slots(Object* element_type,
                             halfword rank,
                             uword* dimensions){
    uword total_array_size=1;
    for(halfword i=0;i<rank;i++){
        total_array_size*=dimensions[i];}
    VectorOfOctet* elements=VectorOfOctet_slots(total_array_size);
    OctetArray* that=allocate(sizeof(*that)+rank*sizeof(that->dimensions[0]));
    that->objcount=2;
    that->element_type=element_type;
    that->elements=elements;
    for(halfword i=0;i<rank;i++){
        that->dimensions[i]=dimensions[i];}
    return that;}

OctetArray* OctetArray_slots_cstring(Object* element_type,
                                     char* initial_value){
    VectorOfOctet* elements=VectorOfOctet_slots_cstring(initial_value);
    halfword rank=1;
    OctetArray* that=allocate(sizeof(*that)+rank*sizeof(that->dimensions[0]));
    that->objcount=2;
    that->element_type=element_type;
    that->elements=elements;
    that->dimensions[0]=elements->length;
    return that;}


// Cons

Cons* Cons_slots(Object* car,Object* cdr){
    Cons* that=allocate(sizeof(*that));
    that->objcount=2;
    that->car=car;
    that->cdr=cdr;
    return that;}

// Symbol

Symbol* Symbol_slots(Object* name,Object* package){
    Symbol* that=allocate(sizeof(*that));
    that->objcount=5;
    that->name=name;
    that->package=package;
    that->value=UNBOUND_Object;
    that->function=UNBOUND_Object;
    that->plist=NIL_Symbol;
    return that;}


/* NULL class has a SYMBOL superclass and has a NIL_Symbol instance. */

/* PHYSICAL-PATHNAME class  has VectorOfOctet slots, like a STRING. */

Object* PhysicalPathname_new(VectorOfOctet* path){
    return Object_new(find_class(symbol_intern_cstring("PHYSICAL-PATHNAME")),
                      (Slots*)path);}

Object* PhysicalPathname_new_cstring(char* path){
    return PhysicalPathname_new(VectorOfOctet_slots_cstring(path));}

/* FILESTREAM */


/* CONDITION */
/* Function */
/* StandardObject */


/* Class */

Class* Class_slots(Object* name,Object* superclasses){
    Class* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->direct_superclasses=superclasses;
    return that;}

Class* make_boot_Class(){
    Class* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=NULL;
    that->direct_superclasses=NULL;
    return that;}

/* StandardClass */

StandardClass* StandardClass_slots(Object* name,Object* superclasses){
    StandardClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->direct_superclasses=superclasses;
    return that;}

StandardClass* make_boot_StandardClass(void){
    StandardClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=NULL;
    that->direct_superclasses=NULL;
    return that;}

/* BuiltInClass */

BuiltInClass* BuiltInClass_slots(Object* name,Object* superclasses){
    BuiltInClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->direct_superclasses=superclasses;
    return that;}

BuiltInClass* make_boot_BuiltInClass(void){
    BuiltInClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=NULL;
    that->direct_superclasses=NULL;
    return that;}

/* StructureClass */

StructureClass* StructureClass_slots(Object* name,Object* superclasses){
    StructureClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->direct_superclasses=superclasses;
    return that;}

/* FuncallableStandardClass */

FuncallableStandardClass* FuncallableStandardClass_slots(Object* name,Object* superclasses){
    FuncallableStandardClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->direct_superclasses=superclasses;
    return that;}

FuncallableStandardClass* make_boot_FuncallableStandardClass(void){
    FuncallableStandardClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=NULL;
    that->direct_superclasses=NULL;
    return that;}


/* Boolean */

Object* as_boolean(bool value){
    return (value
            ?T_Symbol
            :NIL_Symbol);}

bool is_true(Object* object){
    return object!=NIL_Symbol;}

/* Class */

bool class_p(Object* object,Object* class){
    return (object->class==class);}

Object* class_of(Object* object){
    return object->class;}

Object* class_name(Object* class){
    check_class(class,CLASS_Class);
    return class->slots->class.name;}

Object* class_direct_superclasses(Object* class){
    check_class(class,CLASS_Class);
    return class->slots->class.direct_superclasses;}

bool instance_of(Object* object,Object* class){
    Object* object_class=class_of(object);
    if(object_class==class){
        return true;}
    {do_list(superclass,object_class->slots->class.direct_superclasses){
            if(instance_of(object,superclass)){
                return true;}}}
    return false;}

void check_class(Object* object,Object* class){
    if(class && !instance_of(object,class)){
        type_error(__FUNCTION__,object,class);}}

Object* Class_new(Object* name){
    return Object_new(CLASS_Class,(Slots*)Class_slots(name,NIL_Symbol));}

Object* BuiltInClass_new(Object* name){
    return Object_new(BUILT_IN_CLASS_Class,(Slots*)BuiltInClass_slots(name,NIL_Symbol));}

void class_add_superclass(Object* class, Object* superclass){
    check_class(class,STANDARD_OBJECT_Class);
    check_class(superclass,STANDARD_OBJECT_Class);
    if(class->slots->class.direct_superclasses){
        class->slots->class.direct_superclasses=nconc(class->slots->class.direct_superclasses,
                                                      cons(superclass,NIL_Symbol));}
    else{
        class->slots->class.direct_superclasses=cons(superclass,NIL_Symbol);}}

Object* boot_last(Object* list){
    Object* next;
    if(cons_p(list)){
        while(cons_p(next=list->slots->cons.cdr)){
            list=next;}
        return list;}
    else if(null_p(list)){
        return list;}
    else{
        type_error(__FUNCTION__,list,LIST_Class);}}

Object* boot_nconc(Object* list,Object* tail){
    if(list){
        Object* last_cell=boot_last(list);
        ((Cons*)(last_cell->slots))->cdr=tail;
        return list;
    }else{
        return tail;}}

void boot_class_add_superclass(Object* class, Object* superclass){
    if(class->slots->class.direct_superclasses){
        class->slots->class.direct_superclasses=boot_nconc(class->slots->class.direct_superclasses,
                                                      cons(superclass,NIL_Symbol));}
    else{
        class->slots->class.direct_superclasses=cons(superclass,NIL_Symbol);}}


Object* find_class(Object* name){
    error(__FUNCTION__,NOT_IMPLEMENTED_YET);
    return NULL;}


/* Cons */

Object* cons(Object* car,Object* cdr){
    return Object_new(CONS_Class,(Slots*)Cons_slots(car,cdr));}

bool cons_p(Object* object){
    return class_p(object,CONS_Class);}

Object* cons_car(Object* cell){
    check_class(cell,CONS_Class);
    return ((Cons*)(cell->slots))->car;}

Object* cons_cdr(Object* cell){
    check_class(cell,CONS_Class);
    return ((Cons*)(cell->slots))->car;}

Object* cons_rplaca(Object* cell,Object* newcar){
    check_class(cell,CONS_Class);
    ((Cons*)(cell->slots))->car=newcar;
    return cell;}

Object* cons_rplacd(Object* cell,Object* newcar){
    check_class(cell,CONS_Class);
    ((Cons*)(cell->slots))->cdr=newcar;
    return cell;}

/* List */

Object* last(Object* list){
    // TODO: check_class(list,LIST_Class); // move NIL_Symbol to the NULL_Class !
    Object* next;
    if(cons_p(list)){
        while(cons_p(next=cons_cdr(list))){
            list=next;}
        return list;}
    else if(null_p(list)){
        return list;}
    else{
        type_error(__FUNCTION__,list,LIST_Class);}}

Object* nconc(Object* list,Object* tail){
    if(list){
        Object* last_cell=last(list);
        cons_rplacd(last_cell,tail);
        return list;
    }else{
        return tail;}}

Object* list(Object* first_element,...){
    Object* list=cons(first_element,NIL_Symbol);
    Object* tail=list;
    va_list ap;
    va_start(ap,first_element);
    Object* element=va_arg(ap,Object*);
    while(element){
        cons_rplacd(tail,cons(element,NIL_Symbol));
        tail=cons_cdr(tail);
        element=va_arg(ap,Object*);
    }
    va_end(ap);
    return list;}

/* Integer */

Object* integer_from_word(word value){
    return Object_new(INTEGER_Class,(Slots*)Integer_slots(value));}

bool integer_p(Object* object){
    return class_p(object,INTEGER_Class);}

word integer_value(Object* object){
    check_class(object,INTEGER_Class);
    return ((Integer*)(object->slots))->value;}


/* Float */

Object* float_from_floating(floating value){
    return Object_new(FLOAT_Class,(Slots*)Float_slots(value));}

bool float_p(Object* object){
    return class_p(object,FLOAT_Class);}

floating float_value(Object* object){
    check_class(object,FLOAT_Class);
    return ((Float*)(object->slots))->value;}

/* String */

Object* string_new(uword length){
    return Object_new(STRING_Class,
                      (Slots*)OctetArray_slots(CHARACTER_Symbol,1,&length));}

Object* string_new_cstring(char* string){
    return Object_new(STRING_Class,
                      (Slots*)OctetArray_slots_cstring(CHARACTER_Symbol,string));}

bool string_p(Object* object){
    return class_p(object,STRING_Class);}

uword string_length(Object* string){
    check_class(string,STRING_Class);
    return string->slots->octet_array.dimensions[0];}

bool string_eq(Object* a,Object* b){
    uword length=string_length(a);
    if(length!=string_length(b)){
        return false;}
    OctetArray* aa=&(a->slots->octet_array);
    OctetArray* bb=&(b->slots->octet_array);
    for(uword i=0;i<length;i++){
        if(aa->elements->elements[i]!=bb->elements->elements[i]){
            return false;}}
    return true;}

char* string_cstring(Object* string){
    check_class(string,STRING_Class);
    size_t length=string->slots->octet_array.dimensions[0];
    char* result=check_pointer(__FUNCTION__,malloc(1+length));
    strncpy(result,(const char*)string->slots->octet_array.elements->elements,length);
    result[length]='\0';
    return result;}

/* Symbol */

Object* find_symbol(Object* name,Object* obarray){
    Object* syms=cons_cdr(obarray);
    Object* sname;
    while(cons_p(syms) && ((sname=symbol_name(cons_car(syms)))) && !string_eq(sname,name)){
        syms=cons_cdr(syms);}
    return cons_p(syms)
            ? cons_car(syms)
            : NULL;}

Object* symbol_intern(Object* name){
    Object* symbol=find_symbol(name,KERNEL_obarray);
    if(symbol){
        return symbol;}
    symbol=Object_new(SYMBOL_Class, (Slots*)Symbol_slots(name,KERNEL_obarray));
    cons_rplacd(KERNEL_obarray,cons(symbol,cons_cdr(KERNEL_obarray)));
    return symbol;}

Object* symbol_intern_cstring(char* name){
    return symbol_intern(string_new_cstring(name));}

Object* make_symbol_cstring(char* name){
    return Object_new(SYMBOL_Class,(Slots*)Symbol_slots(string_new_cstring(name),NULL));}

Object* symbol_new(Object* name){
    /* make an uninterned symbol */
    return Object_new(SYMBOL_Class, (Slots*)Symbol_slots(name,NIL_Symbol));}

Object* symbol_new_cstring(char* name){
    /* make an uninterned symbol */
    return symbol_new(string_new_cstring(name));}

Object* symbol_name(Object* symbol){
    check_class(symbol,SYMBOL_Class);
    return symbol->slots->symbol.name;}

Object* symbol_package(Object* symbol){
    check_class(symbol,SYMBOL_Class);
    return symbol->slots->symbol.package;}

bool symbol_boundp(Object* symbol){
    check_class(symbol,SYMBOL_Class);
    return UNBOUND_Object!=symbol->slots->symbol.value;}

bool symbol_fboundp(Object* symbol){
    check_class(symbol,SYMBOL_Class);
    return UNBOUND_Object!=symbol->slots->symbol.function;}

Object* symbol_value(Object* symbol){
    check_class(symbol,SYMBOL_Class);
    return symbol->slots->symbol.value;}

Object* symbol_function(Object* symbol){
    check_class(symbol,SYMBOL_Class);
    return symbol->slots->symbol.function;}

Object* symbol_plist(Object* symbol){
    check_class(symbol,SYMBOL_Class);
    return symbol->slots->symbol.plist;}

/* Null */

bool null_p(Object* object){
    return object==NIL_Symbol;}


Object* kernel_rootset(){
    // Don't use list because it's called before check_class can work.
    return cons(UNBOUND_Object,
                cons(KERNEL_obarray,
                     cons(KEYWORD_obarray,
                          cons(kernel_classes,
                               NIL_Symbol))));}

