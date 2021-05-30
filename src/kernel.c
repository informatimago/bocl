#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "kernel.h"
#include "kernel_types.h"
#include "kernel_objects.h"
#include "macros.h"
#include "cons.h"
#include "variable.h"
#include "printer.h"
#include "reader.h"
#include "stream.h"
#include "data.h"

/****************************************/
/*** Object and Slots *******************/
/****************************************/

/* Object */

Object* Object_new(Object* class,Slots* slots){
    Object* that=allocate(sizeof(*that));
    that->objcount=2;
    that->class=class;
    that->slots=slots;
    return that;}

Object* make_boot_Object(Slots* slots){
    Object* that=allocate(sizeof(*that));
    that->objcount=2;
    that->class=&uninitialized_object;
    that->slots=slots;
    return that;}

/* VectorOfObject */

VectorOfObject* VectorOfObject_slots(halfword size){
    VectorOfObject* that=allocate(sizeof(*that)+sizeof(that->elements[0])*size);
    that->objcount=size;
    for(halfword i=0;i<size;i++){
        that->elements[i]=NIL_Symbol;}
    return that;}

/* VectorOfUWord */

VectorOfUWord* VectorOfUWord_slots(halfword size){
    VectorOfUWord* that=allocate(sizeof(*that)+sizeof(that->elements[0])*size);
    for(halfword i=0;i<size;i++){
        that->elements[i]=0;}
    return that;}

/* VectorOfOctet */

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

/* ARRAY */
/* Used both for ARRAY and VECTOR classes, */
/* as long as element-type is not octet, bit or character. */

Array* Array_slots(Object* element_type,halfword rank,uword* dimensions){
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


/* OCTET-ARRAY */
/* Used both for ARRAY and VECTOR classes when */
/* element-type is octet, bit or character. */

OctetArray* OctetArray_slots(Object* element_type,halfword rank,uword* dimensions){
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

OctetArray* OctetArray_slots_cstring(Object* element_type,char* initial_value){
    VectorOfOctet* elements=VectorOfOctet_slots_cstring(initial_value);
    halfword rank=1;
    OctetArray* that=allocate(sizeof(*that)+rank*sizeof(that->dimensions[0]));
    that->objcount=2;
    that->element_type=element_type;
    that->elements=elements;
    that->dimensions[0]=elements->length;
    return that;}


/* CHARACTER */

Character* Character_slots(Object* name,Object* code){
    Character* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->code=code;
    return that;}

bool character_p(Object* object){
    return class_of(object)==CHARACTER_Class;}

Object* character_code(Object* object){
    check_class(object,CHARACTER_Class);
    return object->slots->character.code;}

Object* character_name(Object* object){
    check_class(object,CHARACTER_Class);
    return object->slots->character.name;}


/* INTEGER */
/* Before we use a bigint library, let's just use 64-bit integers: */

Integer* Integer_slots(word value){
    Integer* that=allocate(sizeof(*that));
    that->value=value;
    return that;}

Object* integer_from_word(word value){
    return Object_new(INTEGER_Class,(Slots*)Integer_slots(value));}

bool integer_p(Object* object){
    return class_of(object)==INTEGER_Class;}

word integer_value(Object* object){
    check_class(object,INTEGER_Class);
    return object->slots->integer.value;}


/* FLOAT */

Float* Float_slots(floating value){
    Float* that=allocate(sizeof(*that));
    that->value=value;
    return that;}

Object* float_from_floating(floating value){
    return Object_new(FLOAT_Class,(Slots*)Float_slots(value));}

bool float_p(Object* object){
    return class_of(object)==FLOAT_Class;}

floating float_value(Object* object){
    check_class(object,FLOAT_Class);
    return object->slots->floater.value;}


/* CONS */

Cons* Cons_slots(Object* car,Object* cdr){
    Cons* that=allocate(sizeof(*that));
    that->objcount=2;
    that->car=car;
    that->cdr=cdr;
    return that;}

Object* cons(Object* car,Object* cdr){
    return Object_new(CONS_Class,(Slots*)Cons_slots(car,cdr));}

bool cons_p(Object* object){
    return class_of(object)==CONS_Class;}

Object* cons_car(Object* cell){
    return cell->slots->cons.car;}

Object* cons_cdr(Object* cell){
    return cell->slots->cons.cdr;}

Object* cons_rplaca(Object* cell,Object* newcar){
    cell->slots->cons.car=newcar;
    return cell;}

Object* cons_rplacd(Object* cell,Object* newcdr){
    cell->slots->cons.cdr=newcdr;
    return cell;}



/* STRING */

Object* string_new(uword length){
    return Object_new(STRING_Class,
                      (Slots*)OctetArray_slots(CHARACTER_Symbol,1,&length));}

Object* string_new_cstring(char* string){
    return Object_new(STRING_Class,
                      (Slots*)OctetArray_slots_cstring(CHARACTER_Symbol,string));}

bool string_p(Object* object){
    return class_of(object)==STRING_Class;}

uword string_length(Object* string){
    check_class(string,STRING_Class);
    return string->slots->octet_array.dimensions[0];}

char* string_cstring(Object* string){
    check_class(string,STRING_Class);
    size_t length=string->slots->octet_array.dimensions[0];
    char* result=check_pointer(__FUNCTION__,malloc(1+length));
    strncpy(result,(const char*)string->slots->octet_array.elements->elements,length);
    result[length]='\0';
    return result;}

bool string_eq(Object* a,Object* b){
    if(a==b){
        return true;}
    uword length=string_length(a);
    if(length!=string_length(b)){
        return false;}
    OctetArray* aa=&(a->slots->octet_array);
    OctetArray* bb=&(b->slots->octet_array);
    for(uword i=0;i<length;i++){
        if(aa->elements->elements[i]!=bb->elements->elements[i]){
            return false;}}
    return true;}


/* SYMBOL */

Symbol* Symbol_slots(Object* name,Object* package){
    Symbol* that=allocate(sizeof(*that));
    that->objcount=5;
    that->name=name;
    that->package=package;
    that->value=UNBOUND_Object;
    that->function=UNBOUND_Object;
    that->plist=NIL_Symbol;
    return that;}

Object* find_symbol(Object* name,Object* obarray){
#ifdef DEBUG
    {
        static long c=0;
        fprintf(stderr,"find_symbol %3ld: %-40s in %-16s",
                c++,string_cstring(name),
                string_cstring(symbol_name(cons_car(obarray))));
        Object* syms=cons_cdr(obarray);
        long sc=0;
        while(cons_p(syms)){
            sc++;
            if(string_eq(symbol_name(cons_car(syms)),name)){
                fprintf(stderr,"found %s\n",
                        string_cstring(symbol_name(cons_car(syms))));}
            if(symbol_package(cons_car(syms))!=obarray){
                fprintf(stderr,"wrong package for %s\n",
                        string_cstring(symbol_name(cons_car(syms))));}
            syms=cons_cdr(syms);}
        fprintf(stderr," %3ld symbols\n",sc);}
#endif
    Object* syms=cons_cdr(obarray);
    while(cons_p(syms) && !string_eq(cons_car(syms)->slots->symbol.name,name)){
        syms=cons_cdr(syms);}
    return cons_p(syms)
            ? cons_car(syms)
            : NULL;}

Object* symbol_new(Object* name){
    /* make an uninterned symbol */
    return Object_new(SYMBOL_Class, (Slots*)Symbol_slots(name,NIL_Symbol));}

Object* symbol_new_cstring(char* name){
    /* make an uninterned symbol */
    return symbol_new(string_new_cstring(name));}

Object* symbol_intern_obarray(Object* name,Object* obarray){
    Object* symbol=find_symbol(name,obarray);
    if(symbol){
        return symbol;}
    symbol=Object_new(SYMBOL_Class, (Slots*)Symbol_slots(name,obarray));
    cons_rplacd(obarray,cons(symbol,cons_cdr(obarray)));
    return symbol;}

Object* symbol_intern(Object* name){
    return symbol_intern_obarray(name,KERNEL_obarray);}

Object* symbol_intern_cstring(char* name){
    return symbol_intern(string_new_cstring(name));}

bool symbol_p(Object* object){
    return class_of(object)==SYMBOL_Class;}

Object* symbol_package(Object* symbol){
    check_type(symbol,SYMBOL_Class);
    return symbol->slots->symbol.package;}

Object* symbol_name(Object* symbol){
    check_type(symbol,SYMBOL_Class);
    return symbol->slots->symbol.name;}

bool symbol_boundp(Object* symbol){
    check_type(symbol,SYMBOL_Class);
    return UNBOUND_Object!=symbol->slots->symbol.value;}

bool symbol_fboundp(Object* symbol){
    check_type(symbol,SYMBOL_Class);
    return UNBOUND_Object!=symbol->slots->symbol.function;}

Object* symbol_value(Object* symbol){
    check_type(symbol,SYMBOL_Class);
    return symbol->slots->symbol.value;}

Object* symbol_function(Object* symbol){
    check_type(symbol,SYMBOL_Class);
    return symbol->slots->symbol.function;}

Object* symbol_plist(Object* symbol){
    check_type(symbol,SYMBOL_Class);
    return symbol->slots->symbol.plist;}

Object* symbol_set_package(Object* symbol,Object* new_package){
    check_type(symbol,SYMBOL_Class);
    return symbol->slots->symbol.package=new_package;}

Object* symbol_set_value(Object* symbol,Object* new_value){
    check_type(symbol,SYMBOL_Class);
    return symbol->slots->symbol.value=new_value;}

Object* symbol_set_function(Object* symbol,Object* new_function){
    check_type(symbol,SYMBOL_Class);
    check_type(new_function,FUNCTION_Class);
    return symbol->slots->symbol.function=new_function;}

Object* symbol_set_plist(Object* symbol,Object* new_plist){
    check_type(symbol,SYMBOL_Class);
    if(cons_p(new_plist)||null_p(new_plist)){
        return symbol->slots->symbol.plist=new_plist;}
    else{
        type_error(__FUNCTION__,new_plist,LIST_Class);}}


/* KEYWORD */
/* Note: KEYWORD is not a class, but a subtype of SYMBOL */

Object* keyword_intern(Object* name){
    Object* keyword=symbol_intern_obarray(name,KEYWORD_obarray);
    define_constant(keyword,keyword);
    return keyword;}

Object* keyword_intern_cstring(char* name){
    return keyword_intern(string_new_cstring(name));}

bool keyword_p(Object* object){
    return symbol_p(object)
            && !null_p(memq(object,cdr(KEYWORD_obarray)));}

/* NULL */
/* NULL class has a SYMBOL superclass and has a NIL_Symbol instance. */

bool null_p(Object* object){
    return object==NIL_Symbol;}


/* BOOLEAN */

Object* as_boolean(bool value){
    return (value
            ?T_Symbol
            :NIL_Symbol);}

bool is_true(Object* object){
    return object!=NIL_Symbol;}


/* CLASS */

Class* Class_slots(Object* name,Object* superclasses){
    Class* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->direct_superclasses=superclasses;
    return that;}

Class* make_boot_Class(){
    Class* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=&uninitialized_object;
    that->direct_superclasses=&uninitialized_object;
    return that;}

Object* class_new(Object* name){
    return Object_new(CLASS_Class,(Slots*)Class_slots(name,NIL_Symbol));}

Object* class_of(Object* object){
    return object->class;}

bool class_p(Object* object){
    return class_of(object)==CLASS_Class;}

Object* class_name(Object* class){
    check_type(class,CLASS_Class);
    return class->slots->class.name;}

Object* class_direct_superclasses(Object* class){
    check_type(class,CLASS_Class);
    return class->slots->class.direct_superclasses;}

void class_add_superclass(Object* class, Object* superclass){
    check_type(class,CLASS_Class);
    check_type(superclass,CLASS_Class);
    if(class->slots->class.direct_superclasses){
        class->slots->class.direct_superclasses=nconc(class->slots->class.direct_superclasses,
                                                      cons(superclass,NIL_Symbol));}
    else{
        class->slots->class.direct_superclasses=cons(superclass,NIL_Symbol);}}


Object* find_class(Object* name){
    {do_list(class,kernel_classes){
            if(eql(class->slots->class.name,name)){
                return class;}}}
    return NULL;}

bool subclassp(Object* class,Object* superclass){
    if(class==superclass){
        return true;}
    Object* direct_superclasses=class->slots->class.direct_superclasses;
    while(cons_p(direct_superclasses)){
        Object* direct_superclass=cons_car(direct_superclasses);
        direct_superclasses=cons_cdr(direct_superclasses);
        if(subclassp(direct_superclass,superclass)){
            return true;}}
    return false;}

bool typep(Object* object,Object* class){
    Object* object_class=class_of(object);
#ifdef DEBUG
    fprintf(stderr,"Is object %p of type %p\n",
            (void*)object,
            (void*)class);
    if(object==NULL){
        fprintf(stderr,"NULL object!\n");
        abort();}
    if(class==NULL){
        fprintf(stderr,"NULL class!\n");
        abort();}
    if(object_class==class){
        return true;}
    fprintf(stderr,"Is object of class %p of type %p\n",
            (void*)object_class->slots->class.name->slots->symbol.name,
            (void*)       class->slots->class.name->slots->symbol.name);
    fprintf(stderr,"Is object of class %s of type %s\n",
            string_cstring(object_class->slots->class.name->slots->symbol.name),
            string_cstring(       class->slots->class.name->slots->symbol.name));
#endif
    /* check_type(class,CLASS_Class); */
    return(object_class==class)
            ?true
            :subclassp(object_class,class);}


void check_type(Object* object,Object* class){
    /* checks that the object is an instance of the class, direct or indirect */
    if(!typep(object,class)){
        type_error(__FUNCTION__,object,class);}}


void check_class(Object* object,Object* class){
    /* checks that the object is a direct instance of the class */
    if(class_of(object)!=class){
        type_error(__FUNCTION__,object,class);}}






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
    if(null_p(list)){
        return tail;}
    else if(cons_p(list)){
        Object* last_cell=boot_last(list);
        last_cell->slots->cons.cdr=tail;
        return list;}
    else{
        type_error(__FUNCTION__,list,LIST_Class);}}

void boot_class_add_superclass(Object* class, Object* superclass){
    if(class->slots->class.direct_superclasses){
        class->slots->class.direct_superclasses=boot_nconc(class->slots->class.direct_superclasses,
                                                           cons(superclass,NIL_Symbol));}
    else{
        class->slots->class.direct_superclasses=cons(superclass,NIL_Symbol);}}



/* STANDARD-CLASS */

StandardClass* StandardClass_slots(Object* name,Object* superclasses){
    StandardClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->direct_superclasses=superclasses;
    return that;}

StandardClass* make_boot_StandardClass(void){
    StandardClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=&uninitialized_object;
    that->direct_superclasses=&uninitialized_object;
    return that;}


/* BUILT-IN-CLASS */

Object* built_in_class_new(Object* name){
    return Object_new(BUILT_IN_CLASS_Class,(Slots*)BuiltInClass_slots(name,NIL_Symbol));}

BuiltInClass* BuiltInClass_slots(Object* name,Object* superclasses){
    BuiltInClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->direct_superclasses=superclasses;
    return that;}

BuiltInClass* make_boot_BuiltInClass(void){
    BuiltInClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=&uninitialized_object;
    that->direct_superclasses=&uninitialized_object;
    return that;}


/* STRUCTURE-CLASS */

StructureClass* StructureClass_slots(Object* name,Object* superclasses){
    StructureClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->direct_superclasses=superclasses;
    return that;}


/* FUNCALLABLE-STANDARD-CLASS */

FuncallableStandardClass* FuncallableStandardClass_slots(Object* name,Object* superclasses){
    FuncallableStandardClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->direct_superclasses=superclasses;
    return that;}

FuncallableStandardClass* make_boot_FuncallableStandardClass(void){
    FuncallableStandardClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=&uninitialized_object;
    that->direct_superclasses=&uninitialized_object;
    return that;}




/* PHYSICAL-PATHNAME */

Object* physical_pathname_new(VectorOfOctet* path){
    return Object_new(find_class(symbol_intern_cstring("PHYSICAL-PATHNAME")),
                      (Slots*)path);}

Object* physical_pathname_new_cstring(char* path){
    return physical_pathname_new(VectorOfOctet_slots_cstring(path));}



/* EXTERNAL-FORMAT */

/*
External-formats can be:

- :DEFAULT in place of (:iso-8859-1 :lf :error)

- a keyword denoting an encoding in (:us-ascii :iso-8859-1);
  the default line-termination is :lf, and
  the default encoding error handling is :error (signal an error).

- a list containing:

  + a keyword denoting an encoding in (:us-ascii :iso-8859-1),

  + a line-termination keyword (:dos :crlf :mac :cr :unix :lf),
    the default if NIL or absent is :LF.

  + an object specifing the handling of encoding errors:

    * a character used as substitution character #\sub is suggested,
      or an integer used as substitution code 26 (SUB) is suggested.
      (code-char code) is used on input encoding error.
      (char-code char) is used on output decoding error.

    * the keyword :error asking for a continuable encoding-error to be
      signaled, with a substitute-code restart. This is the default if absent.

    * the symbol NIL asking to ignore and skip the unencodable
      character.

Encoding or decoding errors can only occur with a code>127 given with
:us-ascii, since we deal only with :us-ascii (7-bit) and :iso-8859-1
(8-bit) and internal coding is iso-8859-1.


Note: kernel:stream-external-format accept only (:crlf :cr :lf) as
line-termination; the mapping is done in the CL layer.

(list (member :US-ASCII :ISO-8859-1)
      (member :crlf :cr :lf)
      (or character (member :error nil)))
*/

#define DEFAULT_ENCODING      K(ISO-8859-1)
#define DEFAULT_ENCODING_ENUM encoding_iso_8859_1

Object* normalize_encoding(Object* encoding_o,encoding_t* encoding){
    if(encoding_o==K(DEFAULT)){
        (*encoding)=DEFAULT_ENCODING_ENUM;
        return DEFAULT_ENCODING;}
    else if(encoding_o==K(ISO-8859-1)){
        (*encoding)=encoding_iso_8859_1;
        return encoding_o;}
    else if(encoding_o==K(US-ASCII)){
        (*encoding)=encoding_us_ascii;
        return encoding_o;}
    else{
        error("Invalid encoding %s",
              string_cstring(prin1_to_string(encoding_o)));}}

Object* normalize_new_line(Object* new_line_o,new_line_t* new_line){
    if((new_line_o==K(LF))||(new_line_o==K(UNIX))
       ||(new_line_o==K(DEFAULT))||(new_line_o==NIL_Symbol)){
        (*new_line)=new_line_lf;
        return K(LF);}
    else if((new_line_o==K(CR))||(new_line_o==K(MAC))){
        (*new_line)=new_line_cr;
        return K(CR);}
    else if((new_line_o==K(CRLF))||(new_line_o==K(DOS))){
        (*new_line)=new_line_crlf;
        return K(CRLF);}
    else{
        error("Invalid new-line format %s",
              string_cstring(prin1_to_string(new_line_o)));}}

Object* normalize_on_error(Object* on_error_o,on_error_t* on_error){
    if(character_p(on_error_o)){
        (*on_error)=integer_value(character_code(on_error_o));
        return on_error_o;}
    else if(on_error_o==K(ERROR)){
        (*on_error)=on_error_error;
        return on_error_o;}
    else if(on_error_o==NIL_Symbol){
        (*on_error)=on_error_ignore;
        return on_error_o;}
    else{
        error("Invalid on-error format %s",
              string_cstring(prin1_to_string(on_error_o)));}}

Object* normalize_external_format(Object* external_format,
                                  encoding_t* encoding,
                                  new_line_t* new_line,
                                  on_error_t* on_error){
    Object* encoding_o=DEFAULT_ENCODING;
    Object* new_line_o=K(LF);
    Object* on_error_o=K(ERROR);
    if(symbol_p(external_format)){
        encoding_o=normalize_encoding(external_format,encoding);}
    else if(cons_p(external_format)){
        encoding_o=normalize_encoding(car(external_format),encoding);
        new_line_o=normalize_new_line(cadr(external_format),new_line);
        if(null_p(cddr(external_format))){
            on_error_o=K(ERROR);
            (*on_error)=on_error_error;}
        else{
            on_error_o=normalize_on_error(caddr(external_format),on_error);}}
    else{
        error("Invalid external-format %s",
              string_cstring(prin1_to_string(external_format)));}
    return cons(encoding_o,cons(new_line_o,cons(on_error_o,NIL_Symbol)));}

direction_t convert_direction(Object* direction){
    if(direction==K(INPUT)){
        return direction_input;}
    else if(direction==K(OUTPUT)){
        return direction_output;}
    else if(direction==K(IO)){
        return direction_io;}
    else if(direction==K(PROBE)){
        return direction_probe;}
    else {
        error(__FUNCTION__,"Invalid direction %s",
              string_cstring(prin1_to_string(direction)));}}

Object* convert_from_direction(direction_t direction){
    switch(direction){
      case direction_input:   return K(INPUT);
      case direction_output:  return K(OUTPUT);
      case direction_io:      return K(IO);
      case direction_probe:   return K(PROBE);
      default:
          fatal(__FUNCTION__,"Invalid direction value %d",direction);}}

type_t convert_element_type(Object* element_type){
    /* CHARACTER or (UNSIGNED-BYTE 8) or BIT */
    if(element_type==S(CHARACTER)){
        return type_character;}
    if(element_type==S(BIT)){
        return type_bit;}
    if((cons_p(element_type)
        &&(car(element_type)==S(UNSIGNED-BYTE))
        &&(cons_p(cdr(element_type)))
        &&(integer_p(cadr(element_type)))
        &&(integer_value(cadr(element_type))==8)
        &&(null_p(cddr(element_type))))){
        return type_octet;}
    error(__FUNCTION__,"Invalid element-type %s",
          string_cstring(prin1_to_string(element_type)));}



/* FILE-STREAM */

Object* FileStream_new(Object* pathname,     /* a PHYSICAL-PATHNAME */
                       Object* element_type, /* CHARACTER or (UNSIGNED-BYTE 8) or BIT */
                       Object* external_format){
    check_class(pathname,PHYSICAL_PATHNAME_Class);
    type_t type=convert_element_type(element_type);
    encoding_t encoding=encoding_iso_8859_1;
    new_line_t new_line=new_line_lf;
    on_error_t on_error=on_error_error;
    external_format=normalize_external_format(external_format,&encoding,&new_line,&on_error);
    FileStream* slots=(FileStream*)allocate(sizeof(*slots));
    slots->objcount=3;
    slots->pathname=pathname;
    slots->element_type=element_type;
    slots->external_format=external_format;
    slots->is_open=0;
    slots->direction=direction_input;
    slots->encoding=encoding;
    slots->new_line=new_line;
    slots->on_error=on_error;
    slots->stream=NULL;
    return Object_new(find_class(symbol_intern_cstring("FILE-STREAM")),
                      (Slots*)slots);}

Object* stream_pathname(Object* stream){
    check_class(stream,FILE_STREAM_Class);
    return stream->slots->file_stream.pathname;}

Object* stream_element_type(Object* stream){
    check_class(stream,FILE_STREAM_Class);
    return stream->slots->file_stream.element_type;}

Object* stream_external_format(Object* stream){
    check_class(stream,FILE_STREAM_Class);
    return stream->slots->file_stream.external_format;}

Object* stream_direction(Object* stream){
    check_class(stream,FILE_STREAM_Class);
    return convert_from_direction(stream->slots->file_stream.direction);}

bool stream_open_p(Object* stream){
    check_class(stream,FILE_STREAM_Class);
    return stream->slots->file_stream.is_open;}

FILE* stream_cstream(Object* stream){
    check_class(stream,FILE_STREAM_Class);
    return stream->slots->file_stream.stream;}


Object* standard_input_stream(void){
    Object* stream=FileStream_new(physical_pathname_new_cstring("/dev/stdin"),
                                  S(CHARACTER),
                                  K(DEFAULT));
    stream->slots->file_stream.is_open=true;
    stream->slots->file_stream.direction=direction_input;
    stream->slots->file_stream.stream=stdin;
    return stream;}

Object* standard_output_stream(void){
    Object* stream=FileStream_new(physical_pathname_new_cstring("/dev/stdout"),
                                  S(CHARACTER),
                                  K(DEFAULT));
    stream->slots->file_stream.is_open=true;
    stream->slots->file_stream.direction=direction_output;
    stream->slots->file_stream.stream=stdout;
    return stream;}

Object* standard_error_stream(void){
    Object* stream=FileStream_new(physical_pathname_new_cstring("/dev/stderr"),
                                  S(CHARACTER),
                                  K(DEFAULT));
    stream->slots->file_stream.is_open=true;
    stream->slots->file_stream.direction=direction_output;
    stream->slots->file_stream.stream=stderr;
    return stream;}


/* CONDITION */
/* FUNCTION */
/* STANDARD-OBJECT */






Object* kernel_rootset(void){
    return list(UNBOUND_Object,
                KERNEL_obarray,
                KEYWORD_obarray,
                kernel_classes,
                NULL);}

void kernel_initialize_features(void){
    Object* features=S("*FEATURES*");
    Object* list=NIL_Symbol;

    push(K(COMMON-LISP),&list);
    push(K(BOCL),&list);
    push(K(BOCL-1.0),&list);

#ifdef _POSIX_V6_LP64_OFF64
    push(K(64-BIT-TARGET),&list);
    push(K(64-BIT-HOST),&list);
#else
    push(K(32-BIT-TARGET),&list);
    push(K(32-BIT-HOST),&list);
#endif

    define_parameter(features,list);

    define_constant(S(ARRAY-TOTAL-SIZE-LIMIT),  I(4096));
    define_constant(S(ARRAY-DIMENSION-LIMIT),   I(4096));
    define_constant(S(ARRAY-RANK-LIMIT),        I(   8));
    define_constant(S(CALL-ARGUMENTS-LIMIT),    I(  50));
    define_constant(S(CHAR-CODE-LIMIT),         I( 256));
    define_constant(S(LAMBDA-PARAMETERS-LIMIT), I(  50));
    define_constant(S(MULTIPLE-VALUES-LIMIT),   I(  20));
}

void kernel_initialize(void){
    memory_initialize();
    set_garbage_collection_enabled(false);

    kernel_objects_initialize();
    kernel_initialize_features();
    reader_initialize();
    stream_initialize();

    set_rootset((Block*)kernel_rootset());
    set_garbage_collection_enabled(true);
}



/**** THE END ****/
