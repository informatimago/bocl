#include "vector.h"

bool vectorp(Ref object){
    return typep(object,VECTOR_Class);}

/* Vectors are stored as Array (with element_type / elements* / dimensions[]),
   NOT as VectorOfObject (which has an inline VARRAY). The pre-fix versions
   read from vector_of_object.* and returned garbage / segfaulted on the
   first non-empty (vector ...) call. */

uword vector_length(Ref vector){
    check_type(vector,VECTOR_Class);
    return vector->slots->array.dimensions[0];}

Ref svref(Ref vector,Ref index){
    check_type(vector,VECTOR_Class);
    uword len = vector->slots->array.dimensions[0];
    word  i   = integer_value(index);
    if((0<=i)&&((uword)i<len)){
        return vector->slots->array.elements->elements[i];}
    else{
        OUT_OF_BOUNDS(vector,integer_from_uword(len),index);}}

Ref svref_set(Ref vector,Ref index,Ref new_object){
    check_type(vector,VECTOR_Class);
    uword len  = vector->slots->array.dimensions[0];
    word  i    = integer_value(index);
    if((0<=i)&&((uword)i<len)){
        vector->slots->array.elements->elements[i]=new_object;
        /* The element storage is the inner VectorOfObject, a block separate from
           both the Object and the Array slots block — barrier on it. */
        gc_write_barrier(vector->slots->array.elements);
        return new_object;}
    else{
        OUT_OF_BOUNDS(vector,integer_from_uword(len),index);}}


/**** THE END ****/
