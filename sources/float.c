#include "float.h"
#include "kernel.h"

bool float_eq(Ref a,Ref b){
    check_type(a,FLOAT_Class);
    if(a==b){
        return true;}
    check_type(b,FLOAT_Class);
    return float_value(a)==float_value(b);}

/**** THE END ****/
