#include "data.h"
#include "kernel.h"
#include "float.h"
#include "integer.h"

bool eql(Ref a,Ref b){
    if(a==b){
        /* identical object: covers symbols, conses, interned chars, and any
           number that happens to be the same object. */
        return true;}
    if(integerp(a)){
        if(integerp(b)){
            return integer_value(a)==integer_value(b);}
        return false;}
    if(bignump(a)){
        /* EQL of two bignums compares value (they are built fresh, so identity is
           not enough).  A bignum is never EQL to a fixnum: by construction its
           magnitude exceeds WORD_MAX, which no fixnum reaches. */
        if(bignump(b)){
            return c_bignum_cmp(a,b)==0;}
        return false;}
    if(floatp(a)){
        if(floatp(b)){
            return float_value(a)==float_value(b);}
        return false;}
    if(ratiop(a)){
        /* EQL of two ratios: same numerator and denominator (CLHS).  Ratios are
           built fresh by arithmetic, so identity is not enough. */
        if(ratiop(b)){
            return eql(ratio_numerator(a),ratio_numerator(b))
                && eql(ratio_denominator(a),ratio_denominator(b));}
        return false;}
    if(complexp(a)){
        /* EQL of two complexes: EQL real parts and EQL imaginary parts. */
        if(complexp(b)){
            return eql(complex_realpart(a),complex_realpart(b))
                && eql(complex_imagpart(a),complex_imagpart(b));}
        return false;}
    if(characterp(a)){
        if(characterp(b)){
            /* characters are interned */
            return a==b;}
        return false;}
    return false;}

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
