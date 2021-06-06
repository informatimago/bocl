#include "integer.h"
#include "kernel.h"

bool integer_eq(Ref a,Ref b){
    check_type(a,INTEGER_Class);
    if(a==b){
        return true;}
    check_type(b,INTEGER_Class);
    return integer_value(a)==integer_value(b);}

/**** THE END ****/
