#include "vector.h"

bool bit_vector_p(Ref object){
    return typep(object,BIT_VECTOR_Class);}

uword bit_vector_length(Ref vector){
    check_type(vector,BIT_VECTOR_Class);
    return vector->slots->octet_array.dimensions[0];}

Ref bit(Ref vector,Ref index){
    uword len = bit_vector_length(vector);
    word  i   = integer_value(index);
    if((0<=i)&&((uword)i<len)){
        return integer_from_word(vector->slots->octet_array.elements->elements[i]);}
    else{
        OUT_OF_BOUNDS(vector,integer_from_uword(len),index);}}

Ref bit_set(Ref vector,Ref index,Ref new_bit){
    uword len  = bit_vector_length(vector);
    word  i    = integer_value(index);
    octet bit  = 1&(octet)integer_value(new_bit);
    if((0<=i)&&((uword)i<len)){
        vector->slots->octet_array.elements->elements[i]=bit;
        return new_bit;}
    else{
        OUT_OF_BOUNDS(vector,integer_from_uword(len),index);}}


/**** THE END ****/
