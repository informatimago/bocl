#include "printer.h"
#include "kernel.h"
#include "kernel_objects.h"
#include "macros.h"
#include "data.h"
#include "variable.h"
#include "stream.h"
#include "printer.h"

/* Printer */

Ref prin1_to_string(Ref object){
    // TODO: implement prin1_to_string
    // TODO: implement string-stream
    NOT_IMPLEMENTED_YET();}

Ref print(Ref object,Ref stream){
    terpri(stream);
    prin1(object,stream);
    return object;}

static FILE* output_cstream(Ref stream){
    if(eql(stream,T_Symbol)){
        Ref so=S(*STANDARD-OUTPUT*);
        if(symbol_boundp(so)){
            return stream_cstream(variable_value(so));}
        else{
            return stdout;}}
    else{
        return stream_cstream(stream);}}

Ref prin1(Ref object,Ref stream){
    FILE* out=output_cstream(stream);

    if(character_p(object)){
        word code=integer_value(character_code(object));
        if((32<=code)&&(code<=126)){
            fprintf(out,"#\\%c",(char)code);}
        else{
            fprintf(out,"#\\u%04x",(char)code);}
        return object;}
    
    if(float_p(object)){
        fprintf(out,"%f",float_value(object));
        return object;}
    
    if(integer_p(object)){
        fprintf(out,"%"WORD_FORMAT,integer_value(object));
        return object;}

    if(null_p(object)){
        fprintf(out,"NIL");
        return object;}

    if(keyword_p(object)){
        fprintf(out,":");
        prin1(symbol_name(object),stream);
        return object;}
    
    if(symbol_p(object)){
        prin1(symbol_name(object),stream);
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

    if(cons_p(object)){
        fprintf(out,"(");
        prin1(car(object),stream);
        while(cons_p(cdr(object))){
            object=cdr(object);
            fprintf(out," ");
            prin1(car(object),stream);}
        if(cons_p(object)){
            fprintf(out," . ");
            prin1(cdr(object), stream);}
        fprintf(out,")");
        return object;}

    fprintf(out,"#<");
    prin1(class_name(class_of(object)),stream);
    fprintf(out," ");
    prin1(class_name(object),stream);
    fprintf(out," %p>",(void*)object);
    return object;}


Ref princ(Ref object,Ref stream){
    FILE* out=output_cstream(stream);

    if(character_p(object)){
        word code=integer_value(character_code(object));
        fprintf(out,"%c",(char)code);
        return object;}
    
    if(float_p(object)){
        fprintf(out,"%f",float_value(object));
        return object;}
    
    if(integer_p(object)){
        fprintf(out,"%"WORD_FORMAT,integer_value(object));
        return object;}

    if(null_p(object)){
        fprintf(out,"NIL");
        return object;}

    if(keyword_p(object)){
        princ(symbol_name(object),stream);
        return object;}
    
    if(symbol_p(object)){
        princ(symbol_name(object),stream);
        return object;}
    
    if(string_p(object)){
        char* p=string_cstring(object);
        fprintf(out,"%s",p);
        return object;}

    if(cons_p(object)){
        fprintf(out,"(");
        princ(car(object),stream);
        while(cons_p(cdr(object))){
            object=cdr(object);
            fprintf(out," ");
            princ(car(object),stream);}
        if(cons_p(object)){
            fprintf(out," . ");
            princ(cdr(object), stream);}
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
    FILE* out=output_cstream(stream);
    fprintf(out,"\n");
    return NIL_Symbol;}

/**** THE END ****/
