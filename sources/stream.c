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



/* Map (direction, if-exists, if-does-not-exist) → fopen mode string.
   Returns NULL when the args mean "do not open" (e.g. file-missing + nil). */
static const char* fopen_mode_for(direction_t dir,
                                  Ref if_exists,
                                  Ref if_does_not_exist,
                                  bool exists){
    if(dir==direction_input){
        if(!exists){
            if(if_does_not_exist==NIL_Symbol)           { return NULL; }
            if(if_does_not_exist==K(CREATE))            { return "w+"; }
            /* :error is the default for :input */
            ERROR("File does not exist (and if-does-not-exist is %s)",
                  string_cstring(prin1_to_string(if_does_not_exist)));}
        return "r";}
    if(dir==direction_output || dir==direction_io){
        const char* base = (dir==direction_io) ? "w+" : "w";
        if(exists){
            if(if_exists==NIL_Symbol)         { return NULL; }
            if(if_exists==K(SUPERSEDE)
               || if_exists==K(NEW-VERSION)
               || if_exists==K(RENAME)
               || if_exists==K(RENAME-AND-DELETE)){ return base; }
            if(if_exists==K(OVERWRITE))       { return (dir==direction_io)?"r+":"r+"; }
            if(if_exists==K(APPEND))          { return (dir==direction_io)?"a+":"a"; }
            if(if_exists==K(ERROR)){
                ERROR("File exists (and if-exists is %s)",
                      string_cstring(prin1_to_string(if_exists)));}
            ERROR("Invalid if-exists argument: %s",
                  string_cstring(prin1_to_string(if_exists)));}
        /* file does not exist */
        if(if_does_not_exist==NIL_Symbol)             { return NULL; }
        if(if_does_not_exist==K(CREATE)
           || if_does_not_exist==K(DEFAULT)
           || if_does_not_exist==K(ERROR))            { return base; }
        ERROR("Invalid if-does-not-exist argument: %s",
              string_cstring(prin1_to_string(if_does_not_exist)));}
    FATAL("fopen_mode_for: unhandled direction %d",dir);}

Ref stream_open(Ref filespec,
                Ref direction,
                Ref element_type,
                Ref if_exists,
                Ref if_does_not_exist,
                Ref external_format){
    Ref path;
    if(typep(filespec,PHYSICAL_PATHNAME_Class))     { path=filespec; }
    else if(typep(filespec,STRING_Class))           { path=pathname(filespec); }
    else if(typep(filespec,FILE_STREAM_Class))      { path=stream_pathname(filespec); }
    else                                            { TYPE_ERROR(filespec,PHYSICAL_PATHNAME_Class); }
    char* cpath=string_cstring(namestring(path));
    direction_t dir=convert_direction(direction);

    /* :probe — try to open r/o, immediately close, and return a closed stream
       (or NIL if missing and if-does-not-exist is NIL). */
    if(dir==direction_probe){
        FILE* file=fopen(cpath,"r");
        if(file==NULL){
            if(if_does_not_exist==NIL_Symbol){ free(cpath); return NIL_Symbol; }
            if(if_does_not_exist==K(CREATE)){
                file=fopen(cpath,"w");
                if(file==NULL){
                    ERROR("System error on fopen(%s, w)",cpath);}}
            else if(if_does_not_exist==K(ERROR) || if_does_not_exist==K(DEFAULT)){
                ERROR("File does not exist: %s",cpath);}
            else{
                ERROR("Invalid if-does-not-exist argument: %s",
                      string_cstring(prin1_to_string(if_does_not_exist)));}}
        fclose(file);
        Ref stream=file_stream_new(path,element_type,external_format);
        stream->slots->file_stream.direction=dir;
        free(cpath);
        return stream;}

    /* For :input / :output / :io probe existence so we pick the right mode. */
    bool exists=false;
    {   FILE* probe=fopen(cpath,"r");
        if(probe){ exists=true; fclose(probe); }}
    const char* mode = fopen_mode_for(dir, if_exists, if_does_not_exist, exists);
    if(mode==NULL){
        /* User asked to silently bail out. */
        free(cpath);
        return NIL_Symbol;}
    FILE* file=fopen(cpath,mode);
    if(file==NULL){
        ERROR("System error on fopen(%s, %s)",cpath,mode);}
    free(cpath);

    Ref stream=file_stream_new(path,element_type,external_format);
    stream->slots->file_stream.direction=dir;
    stream->slots->file_stream.is_open=true;
    stream->slots->file_stream.stream=file;
    return stream;}

Ref stream_close(Ref stream){
    check_type(stream,FILE_STREAM_Class);
    if(stream->slots->file_stream.is_open){
        FILE* f=stream->slots->file_stream.stream;
        if(f && f!=stdin && f!=stdout && f!=stderr){
            fclose(f);}
        stream->slots->file_stream.is_open=false;
        stream->slots->file_stream.stream=NULL;}
    return T_Symbol;}



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
