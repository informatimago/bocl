#include "printer.h"
#include "kernel.h"
#include "kernel_objects.h"
#include "macros.h"
#include "data.h"
#include "variable.h"
#include "stream.h"
#include "string.h"
#include "character.h"

/* Printer */

Ref prin1_to_string(Ref object){
    Ref stream=stream_output_string(S(CHARACTER));
    prin1(object,stream);
    return string_output_stream_get_string(stream);}

Ref print(Ref object,Ref stream){
    terpri(stream);
    prin1(object,stream);
    return object;}

Ref prin1(Ref object,Ref stream){
    FILE* out=stream_output_cstream(stream);

    if(character_p(object)){
        octet code=(octet)integer_value(character_code(object));
        if(printablep(code)){
            fprintf(out,"#\\%c",(char)code);}
        else{
            fprintf(out,"#\\%s",string_cstring(character_name(object)));}
        return object;}

    if(float_p(object)){
        fprintf(out,"%g",float_value(object));
        return object;}

    if(integer_p(object)){
        fprintf(out,"%"WORD_FORMAT,integer_value(object));
        return object;}

    /* if(null_p(object)){ */
    /*     fprintf(out,"NIL"); */
    /*     return object;} */

    if(keyword_p(object) || symbol_p(object)){
        princ(symbol_name(object),stream);
        return object;}

    if(cons_p(object)){
        Ref current=object;
        fprintf(out,"(");
        prin1(car(current),stream);
        current=cdr(current);
        while(cons_p(current)){
            fprintf(out," ");
            prin1(car(current),stream);
            current=cdr(current);}
        if(!null_p(current)){
            fprintf(out," . ");
            prin1(current, stream);}
        fprintf(out,")");
        return object;}

    if(string_p(object)){
        char* p=string_cstring(object);
        fprintf(out,"\"");
        while(*p){
            switch(*p){
              case '\\': fprintf(out,"\\\\"); break;
              case '"': fprintf(out,"\\\""); break;
              default: fprintf(out,"%c",*p); break;}
            p++;}
        fprintf(out,"\"");
        return object;}

    if(vector_p(object)){
        fprintf(out,"(");
        prin1(car(object),stream);
        object=cdr(object);
        while(cons_p(object)){
            fprintf(out," ");
            prin1(car(object),stream);
            object=cdr(object);}
        if(!null_p(object)){
            fprintf(out," . ");
            prin1(object, stream);}
        fprintf(out,")");
        return object;}

    fprintf(out,"#<");
    prin1(class_name(class_of(object)),stream);
    if(class_p(object)){
        fprintf(out," ");
        prin1(class_name(object),stream);}
    fprintf(out," %p>",(void*)object);
    return object;}


Ref princ(Ref object,Ref stream){
    FILE* out=stream_output_cstream(stream);

    if(character_p(object)){
        octet code=(octet)integer_value(character_code(object));
        fprintf(out,"%c",(char)code);
        return object;}

    if(float_p(object)){
        fprintf(out,"%f",float_value(object));
        return object;}

    if(integer_p(object)){
        fprintf(out,"%"WORD_FORMAT,integer_value(object));
        return object;}

    /* if(null_p(object)){ */
    /*     fprintf(out,"NIL"); */
    /*     return object;} */

    if(keyword_p(object)||symbol_p(object)){
        princ(symbol_name(object),stream);
        return object;}

    if(string_p(object)){
        char* p=string_cstring(object);
        fprintf(out,"%s",p);
        return object;}

    if(cons_p(object)){
        Ref current=object;
        fprintf(out,"(");
        princ(car(current),stream);
        current=cdr(current);
        while(cons_p(current)){
            fprintf(out," ");
            princ(car(current),stream);
            current=cdr(current);}
        if(!null_p(current)){
            fprintf(out," . ");
            princ(current, stream);}
        fprintf(out,")");
        return object;}

    fprintf(out,"#<");
    princ(class_name(class_of(object)),stream);
    if(typep(object,CLASS_Class)){
        fprintf(out," ");
        princ(class_name(object),stream);}
    fprintf(out," %p>",(void*)object);
    return object;}


Ref terpri(Ref stream){
    FILE* out=stream_output_cstream(stream);
    fprintf(out,"\n");
    return NIL_Symbol;}

/**** THE END ****/
