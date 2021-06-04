#include "filename.h"
#include "kernel.h"

Ref pathname(Ref pathname_designator){
    if(typep(pathname_designator,PATHNAME_Class)){
        return pathname_designator;}
    if(typep(pathname_designator,FILE_STREAM_Class)){
        return stream_pathname(pathname_designator);}
    if(typep(pathname_designator,STRING_Class)){
        return physical_pathname(pathname_designator);}
    /* TODO: pathname-designator = (or pathname string file-stream) */
    TYPE_ERROR(pathname_designator,STRING_Class);}

Ref namestring(Ref pathname){
    check_type(pathname,PATHNAME_Class);
    return string_new_vector_of_octets(&pathname->slots->vector_of_octet);}

/**** THE END ****/
