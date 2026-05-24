#include "data.h"
#include "kernel.h"
#include "float.h"
#include "integer.h"

bool eql(Ref a,Ref b){
    if(integerp(a)){
        if(a==b){
            return true;}
        if(integerp(b)){
            return integer_value(a)==integer_value(b);}
        return false;}
    if(floatp(a)){
        if(a==b){
            return true;}
        if(floatp(b)){
            return float_value(a)==float_value(b);}
        return false;}
    if(characterp(a)){
        if(characterp(b)){
            /* characters are interned */
            return a==b;}
        return false;}
    return a==b;}

bool equal(Ref a,Ref b){

    if(consp(a)){
        if(!consp(b)){
            return false;}
        return eql(a,b) || (equal(car(a),car(b)) && equal(cdr(a),cdr(b)));}

    if(stringp(a)){
        if(!stringp(b)){
            return false;}
        /* EQUAL is case-sensitive on strings per CL spec; that's string_eq.
           The case-insensitive comparator is reserved for EQUALP / STRING-EQUAL. */
        return eql(a,b) || string_eq(a,b);}

    if(bit_vector_p(a)){
        if(!bit_vector_p(b)){
            return false;}
        return eql(a,b) || bit_vector_equal(a,b);}

    if(physical_pathname_p(a)){
        if(!physical_pathname_p(b)){
            return false;}
        return eql(a,b) || physical_pathname_equal(a,b);}


    return eql(a,b);}

/**** THE END ****/
