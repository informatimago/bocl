#include "bostring.h"
#include "kernel.h"
#include "boerror.h"

/*
(loop for i from 32 to 255 do (insert(format "%c"i)))
ABCDEFGHIJKLMNOPQRSTUVWXYZ
abcdefghijklmnopqrstuvwxyz
ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞ
àáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþ

(- ?a ?A)32
(- ?à ?À)32
?a 97
?z 122
?à 224
?÷ 247
?þ 254
*/

octet* octet_upcase(octet* string){
    octet* current=string;
    while(*current){
        if(((97<=(*current))&&((*current)<=122))
           ||((224<=(*current))&&((*current)<=254)&&((*current)!=247))){
            (*current)=(*current)-32;}
        current++;}
    return string;}


Ref char_ref(Ref string,Ref index){
    /* TODO: use (array-dimensions string 0) */
    uword len = string_length(string);
    word  i   = integer_value(index);
    if((0<=i)&&((uword)i<len)){
        return octet_character(string->slots->octet_array.elements->elements[i]);}
    else{
        OUT_OF_BOUNDS(string,integer_from_uword(len),index);}}

Ref char_set(Ref string,Ref index,Ref new_char){
    /* TODO: use (array-dimensions string 0) */
    uword len  = string_length(string);
    word  i    = integer_value(index);
    octet code = (octet)integer_value(character_code(new_char));
    if((0<=i)&&((uword)i<len)){
        string->slots->octet_array.elements->elements[i]=code;
        return new_char;}
    else{
        OUT_OF_BOUNDS(string,integer_from_uword(len),index);}}


/**** THE END ****/
