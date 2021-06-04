#include "vector.h"

bool vectorp(Ref object){
    return typep(object,VECTOR_Class);}

uword vector_length(Ref vector){
    check_type(vector,VECTOR_Class);
    return vector->slots->vector_of_object.objcount;}

Ref svref(Ref vector,Ref index){
    /* TODO: use (array-dimensions vector 0) */
    uword len = vector_length(vector);
    word  i   = integer_value(index);
    if((0<=i)&&((uword)i<len)){
        return vector->slots->vector_of_object.elements[i];}
    else{
        OUT_OF_BOUNDS(vector,integer_from_uword(len),index);}}

Ref svref_set(Ref vector,Ref index,Ref new_object){
    /* TODO: use (array-dimensions vector 0) */
    uword len  = vector_length(vector);
    word  i    = integer_value(index);
    if((0<=i)&&((uword)i<len)){
        vector->slots->vector_of_object.elements[i]=new_object;
        return new_object;}
    else{
        OUT_OF_BOUNDS(vector,integer_from_uword(len),index);}}


/**** THE END ****/
