#include <stdio.h>
#include <stdlib.h>
#include "stream.h"
#include "kernel_types.h"
#include "kernel_objects.h"
#include "kernel.h"
#include "filename.h"
#include "printer.h"
#include "variable.h"
#include "boerror.h"



Ref stream_open(Ref filespec,
                Ref direction,
                Ref element_type,
                unused Ref if_exists,
                Ref if_does_not_exist,
                Ref external_format){
    Ref path;
    if(typep(filespec,PHYSICAL_PATHNAME_Class)) {
        path=filespec;}
    else if(typep(filespec,STRING_Class)) {
        path=pathname(filespec);}
    else if(typep(filespec,FILE_STREAM_Class)) {
        path=stream_pathname(filespec);}
    else {
        TYPE_ERROR(filespec,PHYSICAL_PATHNAME_Class);}
    char* cpath=string_cstring(namestring(path));
    direction_t dir=convert_direction(direction);
    if(direction_probe==dir){
        /* 1- test if exists: */
        FILE* file=fopen(cpath,"r");
        int file_error=ferror(file);
        if(file==NULL){
            if(if_does_not_exist==K(ERROR)){
                ERROR("System error on fopen(%s): %d",
                      cpath,file_error);}
            else if(if_does_not_exist==NIL_Symbol){
                return NIL_Symbol;}
            else if(if_does_not_exist==K(CREATE)){
                /* 2- create if does not exist: */
                file=fopen(cpath,"w");
                if(file==NULL){
                    ERROR("System error on fopen(%s): %d",
                          cpath,file_error);}}
            else{
                ERROR("Invalid IF-DOES-NOT-EXIST argument: %s",
                      string_cstring(prin1_to_string(if_does_not_exist)));}}
        fclose(file);
        /* 3- if existed or created, return the closed stream */
        Ref stream=file_stream_new(path,element_type,external_format);
        stream->slots->file_stream.direction=dir;
        return stream;}

#if 0
    char* mode;
    switch(dir){
      case direction_input:
          mode="r";
          break;
      case direction_output:
      case direction_io:
          if((if_exists==K(SUPERSEDE))
             ||(if_exists==K(NEW-VERSION))
             ||(if_exists==K(RENAME))
             ||(if_exists==K(RENAME-AND-DELETE))){
              /* do supersede */
          }
          else if(if_exists==K(ERROR)){
              mode="wx";
          }
          else if(if_exists==K(OVERWRITE)){
              mode="w";
          }
          else if(if_exists==K(APPEND)){ }
          mode="a";

          else if(if_exists==NIL_Symbol){ }
          mode="wx";

          else{
              ERROR("Invalid IF-ERROR argument: %s",
                    string_cstring(prin1_to_string(if_exists)));}
          break;}

    FILE* file=fopen(cpath,"r");
    int file_error=ferror(file);
    if(file==NULL){
        if(if_does_not_exist==K(ERROR)){
            ERROR("System error on fopen(%s): %d",
                  cpath,file_error);}
        else if(if_does_not_exist==NIL_Symbol){
            return NIL_Symbol;}
        else if(if_does_not_exist==K(CREATE)){
            /* 2- create if does not exist: */
            file=fopen(cpath,"w");
            if(file==NULL){
                ERROR("System error on fopen(%s): %d",
                      cpath,file_error);}}
        else{
            ERROR("Invalid IF-DOES-NOT-EXIST argument: %s",
                  string_cstring(prin1_to_string(if_does_not_exist)));}}

#endif
    NOT_IMPLEMENTED_YET();
    return NIL_Symbol;
}



void stream_initialize(void){

    define_variable(S(*STANDARD-INPUT*),standard_input_stream());
    define_variable(S(*STANDARD-OUTPUT*),standard_output_stream());
    define_variable(S(*ERROR-OUTPUT*),standard_error_stream());

    define_variable(S(*TRACE-OUTPUT*),variable_value(S(*ERROR-OUTPUT*)));

    /* TODO: Implement  TWO-WAY-STREAM and SYNONYM-STREAM */
    define_variable(S(*TERMINAL-IO*),NIL_Symbol);
    define_variable(S(*DEBUG-IO*),NIL_Symbol);

}


FILE* stream_output_cstream(Ref stream){
    if(eql(stream,T_Symbol)){
        Ref so=S(*STANDARD-OUTPUT*);
        if(symbol_boundp(so)){
            return stream_cstream(variable_value(so));}
        else{
            return stdout;}}
    else{
        return stream_cstream(stream);}}


Ref stream_finish_output(Ref stream){
    FILE* out=stream_output_cstream(stream);
    fflush(out);
    return NIL_Symbol;}


/**** THE END ****/
