#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sysexits.h>
#include <stdnoreturn.h>
#include <limits.h>
#include <stdint.h>
#include "boerror.h"
#include "memory.h"
#include "kernel.h"
#include "macros.h"
#include "variable.h"
#include "stream.h"
#include "printer.h"
#include "conditions.h"
#include "eval.h"

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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    vfprintf(stderr,message_format,ap);
#pragma GCC diagnostic pop
    va_end(ap);
    fprintf(stderr,"\n\n");
    exit(EX_SOFTWARE);}

_Noreturn void error(const char* fname,const char* message_format,...){
    /* C-level terminal fallback: print to stderr and exit.
       The Lisp-level (ERROR ...) is handled by kernel_ERROR in conditions.c,
       which signals a SIMPLE-ERROR and only calls this function if no handler
       matched.  Direct C callers (bounds checks, size overflows, …) also land
       here; to make those catchable from Lisp, route through kernel_ERROR
       rather than the C ERROR() macro. */
    va_list ap;
    fprintf(stderr,"\nIn %s:\n",fname);
    va_start(ap,message_format);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    vfprintf(stderr,message_format,ap);
#pragma GCC diagnostic pop
    va_end(ap);
    fprintf(stderr,"\n\n");
    exit(EX_DATAERR);}


_Noreturn void type_error(const char* fname,Ref object,Ref class){
    char* got=string_cstring(object->class->slots->class.name->slots->symbol.name);
    char* exp=string_cstring(        class->slots->class.name->slots->symbol.name);

    /* Signal a SIMPLE-TYPE-ERROR through the Lisp handler system so that
       HANDLER-CASE / HANDLER-BIND can catch type errors raised from C built-ins.

       Bootstrap guard: SIMPLE_TYPE_ERROR_Class->class is NULL while class
       objects are still being initialised; skip signalling in that window.

       Re-entry safety: no static flag is used because signal_condition uses
       longjmp for HANDLER-CASE (the type_error frame is gone before any handler
       body runs) and deactivates HANDLER-BIND frames while they execute, so
       a type error raised inside a handler cannot recurse back here via the
       same handler. */
    if (SIMPLE_TYPE_ERROR_Class->class != NULL) {
        uword pool = open_live_pool();
        char msg[512];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(msg, sizeof(msg),
                 "In %s: TYPE ERROR: expected %s, got %s", fname, exp, got);
#pragma GCC diagnostic pop
        Ref fmt  = string_new_cstring(msg);
        add_to_live_pool(fmt);
        Ref cond = make_simple_condition(SIMPLE_TYPE_ERROR_Class, fmt, NIL_Symbol);
        add_to_live_pool(cond);
        signal_condition(cond, eval_global_environment());
        /* signal_condition returned — no handler matched; fall through to exit. */
        close_live_pool(pool);
    }

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
    if(/* (0<=size) && */ (size<UWORD_MAX)){
        return (uword)size;}
    error(fname,"Size %z too big for an uword (max = %"UWORD_FORMAT")",
          size,UWORD_MAX);}

halfword check_size_to_halfword(const char* fname,size_t size){
    if(/* (0<=size) && */ (size<HALFWORD_MAX)){
        return (halfword)size;}
    error(fname,"Size %z too big for a halfword (max = %"HALFWORD_FORMAT")",
          size,HALFWORD_MAX);}

uword check_long_to_uword(const char* fname,long value){
    if((0<=value) /* && (value<UWORD_MAX) always true */){
        return (uword)value;}
    error(fname,"Value long %l too big for an uword (max = %"UWORD_FORMAT")",
          value,UWORD_MAX);}

uword check_word_to_uword(const char* fname,word value){
    if((0<=value) && (((uword)WORD_MAX<UWORD_MAX) || (value<(word)UWORD_MAX))){
        return (uword)value;}
    error(fname,"Word %"WORD_FORMAT" too big for a uword (max = %"UWORD_FORMAT")",
          value,UWORD_MAX);}

size_t check_word_to_size(const char* fname,word value){
    if((0<=value) && (((size_t)WORD_MAX<SIZE_MAX) || (value<(word)SIZE_MAX))){
        return (size_t)value;}
    error(fname,"Word %"WORD_FORMAT" too big for a size_t (max = %zu)",
          value,SIZE_MAX);}

word check_uword_to_word(const char* fname,uword value){
   if((value<WORD_MAX)){
        return (word)value;}
    error(fname,"Unsigned word %"UWORD_FORMAT" too big for a word (max = %"WORD_FORMAT")",
          value,WORD_MAX);}

halfword check_uword_to_halfword(const char* fname,uword value){
   if((value<HALFWORD_MAX)){
        return (halfword)value;}
    error(fname,"Unsigned word %"UWORD_FORMAT" too big for a halfword (max = %"HALFWORD_FORMAT")",
          value,HALFWORD_MAX);}

int check_halfword_to_int(const char* fname,halfword value){
    if(/* (0<=value) && */ ((HALFWORD_MAX<INT_MAX) || (value<(word)INT_MAX))){
        return (int)value;}
    error(fname,"Half word %"HALFWORD_FORMAT" too big for a int (max = %zu)",
          value,INT_MAX);}

/**** THE END ****/
