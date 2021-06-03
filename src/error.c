#include <stdio.h>
#include <stdarg.h>
#include <sysexits.h>
#include "error.h"
#include "memory.h"
#include "kernel.h"
#include "macros.h"
#include "variable.h"
#include "stream.h"
#include "printer.h"

/* Error Handling */

void report_error(unused Ref error){
    fprintf(stderr,"\nERROR: ");
    Ref err=variable_value(S(*ERROR-OUTPUT*));
    prin1(error,err);
    terpri(err);}


_Noreturn void fatal(const char* fname,const char* message_format,...){
    va_list ap;
    fprintf(stderr,"\nIn %s:\n",fname);
    va_start(ap,message_format);
    vfprintf(stderr,message_format,ap);
    va_end(ap);
    fprintf(stderr,"\n\n");
    exit(EX_SOFTWARE);}

_Noreturn void error(const char* fname,const char* message_format,...){
    // TODO: Implement error() with the error handling system.
    va_list ap;
    fprintf(stderr,"\nIn %s:\n",fname);
    va_start(ap,message_format);
    vfprintf(stderr,message_format,ap);
    va_end(ap);
    fprintf(stderr,"\n\n");
    exit(EX_DATAERR);}


_Noreturn void type_error(const char* fname,Ref object,Ref class){
    // TODO: Implement type_error() with the error handling system.
    char* got=string_cstring(object->class->slots->class.name->slots->symbol.name);
    char* exp=string_cstring( class->class->slots->class.name->slots->symbol.name);
    fprintf(stderr,"\nIn %s:\n",fname);
    fprintf(stderr,"TYPE ERROR: got an object of class %s, expected an object of class %s\n\n",got,exp);
    free(exp);
    free(got);
    exit(EX_DATAERR);}


void* check_pointer(const char* fname,void* pointer){
    if(pointer){
        return pointer;}
    fatal(fname,"FATAL ERROR: OUT-OF-MEMORY");}

uword check_size_to_uword(const char* fname,size_t size){
    if((0<=size) && (size<UWORD_MAX)){
        return (uword)size;}
    error(fname,"Size %z too big for an uword (max = %"UWORD_FORMAT")",
          size,UWORD_MAX);}

halfword check_size_to_halfword(const char* fname,size_t size){
    if((0<=size) && (size<HALFWORD_MAX)){
        return (halfword)size;}
    error(fname,"Size %z too big for a halfword (max = %"HALFWORD_FORMAT")",
          size,HALFWORD_MAX);}

word check_uword_to_word(const char* fname,uword value){
   if((value<WORD_MAX)){
        return (word)value;}
    error(fname,"Unsigned word "UWORD_FORMAT" too big for a word (max = %"WORD_FORMAT")",
          value,WORD_MAX);}

halfword check_uword_to_halfword(const char* fname,uword value){
   if((value<HALFWORD_MAX)){
        return (halfword)value;}
    error(fname,"Unsigned word "UWORD_FORMAT" too big for a halfword (max = %"HALFWORD_FORMAT")",
          value,HALFWORD_MAX);}


/**** THE END ****/
