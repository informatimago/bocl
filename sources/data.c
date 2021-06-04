#include "data.h"
#include "kernel.h"

bool eql(Ref a,Ref b){
    return a==b;}

bool equal(Ref a,Ref b){

    if(cons_p(a)){
        if(!cons_p(b)){
            return false;}
        return eql(a,b) || (equal(car(a),car(b)) && equal(cdr(a),cdr(b)));}

    if(string_p(a)){
        if(!string_p(b)){
            return false;}
        return eql(a,b) || string_equal(a,b);}

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
