#include "printer.h"
#include "kernel.h"
#include "kernel_objects.h"
#include "macros.h"
#include "data.h"
#include "variable.h"
#include "stream.h"
#include "bostring.h"
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

    if(characterp(object)){
        octet code=(octet)integer_value(character_code(object));
        if(printablep(code)){
            fprintf(out,"#\\%c",(char)code);}
        else{
            fprintf(out,"#\\%s",string_cstring(character_name(object)));}
        return object;}

    if(floatp(object)){
        fprintf(out,"%g",float_value(object));
        return object;}

    if(integerp(object)){
        fprintf(out,"%"WORD_FORMAT,integer_value(object));
        return object;}

    /* if(nullp(object)){ */
    /*     fprintf(out,"NIL"); */
    /*     return object;} */

    if(keywordp(object) ){
        fprintf(out,":");
        princ(symbol_name(object),stream);
        return object;}

    if(symbolp(object)){
        princ(symbol_name(object),stream);
        return object;}

    if(consp(object)){
        Ref current=object;
        fprintf(out,"(");
        prin1(car(current),stream);
        current=cdr(current);
        while(consp(current)){
            fprintf(out," ");
            prin1(car(current),stream);
            current=cdr(current);}
        if(!nullp(current)){
            fprintf(out," . ");
            prin1(current, stream);}
        fprintf(out,")");
        return object;}

    if(stringp(object)){
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

    if(vectorp(object)){
        /* Pre-fix this branch used car/cdr on the vector and crashed at
           "got VECTOR, expected LIST" the first time anyone called
           (vector …). A vector prints as #(e1 e2 …). Vectors are
           Array-backed under the hood, so iterate via array.elements. */
        fprintf(out,"#(");
        uword len = vector_length(object);
        for(uword i=0; i<len; i++){
            if(i>0){ fprintf(out," "); }
            prin1(object->slots->array.elements->elements[i], stream);}
        fprintf(out,")");
        return object;}

    fprintf(out,"#<");
    prin1(class_name(class_of(object)),stream);

    if(built_in_function_p(object)){
        fprintf(out," ");
        prin1(function_name(object),stream);}
    /* else if(){ */
    /* } */
    if(classp(object)){
        fprintf(out," ");
        prin1(class_name(object),stream);}
    fprintf(out," %p>",(void*)object);
    return object;}


Ref princ(Ref object,Ref stream){
    FILE* out=stream_output_cstream(stream);

    if(characterp(object)){
        octet code=(octet)integer_value(character_code(object));
        fprintf(out,"%c",(char)code);
        return object;}

    if(floatp(object)){
        fprintf(out,"%f",float_value(object));
        return object;}

    if(integerp(object)){
        fprintf(out,"%"WORD_FORMAT,integer_value(object));
        return object;}

    /* if(nullp(object)){ */
    /*     fprintf(out,"NIL"); */
    /*     return object;} */

    if(keywordp(object)||symbolp(object)){
        princ(symbol_name(object),stream);
        return object;}

    if(stringp(object)){
        char* p=string_cstring(object);
        fprintf(out,"%s",p);
        return object;}

    if(consp(object)){
        Ref current=object;
        fprintf(out,"(");
        princ(car(current),stream);
        current=cdr(current);
        while(consp(current)){
            fprintf(out," ");
            princ(car(current),stream);
            current=cdr(current);}
        if(!nullp(current)){
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
