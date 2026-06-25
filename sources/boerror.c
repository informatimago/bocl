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
    /* Format the message once into a buffer so we can both signal it as a
       catchable condition and, if nobody handles it, print it on the way out. */
    char body[1024];
    va_list ap;
    va_start(ap,message_format);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    vsnprintf(body,sizeof(body),message_format,ap);
#pragma GCC diagnostic pop
    va_end(ap);

    /* Signal a catchable SIMPLE-ERROR so that HANDLER-CASE / HANDLER-BIND —
       in particular the ANSI test runner's per-test handlers — can recover
       from errors raised inside C built-ins (undefined function, bad args,
       unsupported operations, …) instead of the whole process dying.  This
       mirrors type_error().  Bootstrap guard: skip while the condition class
       is still being initialised (its metaclass slot is NULL then). */
    if(SIMPLE_ERROR_Class->class != NULL){
        uword pool = open_live_pool();
        char msg[1100];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(msg,sizeof(msg),"In %s: %s",fname,body);
#pragma GCC diagnostic pop
        Ref fmt  = string_new_cstring(msg);
        add_to_live_pool(fmt);
        Ref cond = make_simple_condition(SIMPLE_ERROR_Class, fmt, NIL_Symbol);
        add_to_live_pool(cond);
        signal_condition(cond, eval_global_environment());
        /* signal_condition returned — no handler matched; fall through. */
        close_live_pool(pool);}

    fprintf(stderr,"\nIn %s:\n",fname);
    fprintf(stderr,"%s\n\n",body);
    exit(EX_DATAERR);}


_Noreturn void program_error(const char* fname,const char* message_format,...){
    /* Like error(), but signals a catchable PROGRAM-ERROR — used for malformed
       lambda-list / keyword-argument calls (CLHS 3.5.1.x), which the standard
       classifies as program-errors. */
    char body[1024];
    va_list ap;
    va_start(ap,message_format);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    vsnprintf(body,sizeof(body),message_format,ap);
#pragma GCC diagnostic pop
    va_end(ap);

    if(PROGRAM_ERROR_Class->class != NULL){
        uword pool = open_live_pool();
        char msg[1100];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(msg,sizeof(msg),"In %s: %s",fname,body);
#pragma GCC diagnostic pop
        Ref fmt  = string_new_cstring(msg);
        add_to_live_pool(fmt);
        Ref cond = make_simple_condition(PROGRAM_ERROR_Class, fmt, NIL_Symbol);
        add_to_live_pool(cond);
        signal_condition(cond, eval_global_environment());
        /* signal_condition returned — no handler matched; fall through. */
        close_live_pool(pool);}

    fprintf(stderr,"\nIn %s:\n",fname);
    fprintf(stderr,"%s\n\n",body);
    exit(EX_DATAERR);}


_Noreturn void package_error(const char* fname,const char* message_format,...){
    /* Like error(), but signals a catchable PACKAGE-ERROR (CLHS 11.x): unknown
       package designators, name/use conflicts, etc. */
    char body[1024];
    va_list ap;
    va_start(ap,message_format);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    vsnprintf(body,sizeof(body),message_format,ap);
#pragma GCC diagnostic pop
    va_end(ap);

    if(PACKAGE_ERROR_Class->class != NULL){
        uword pool = open_live_pool();
        char msg[1100];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(msg,sizeof(msg),"In %s: %s",fname,body);
#pragma GCC diagnostic pop
        Ref fmt  = string_new_cstring(msg);
        add_to_live_pool(fmt);
        Ref cond = make_simple_condition(PACKAGE_ERROR_Class, fmt, NIL_Symbol);
        add_to_live_pool(cond);
        signal_condition(cond, eval_global_environment());
        /* signal_condition returned — no handler matched; fall through. */
        close_live_pool(pool);}

    fprintf(stderr,"\nIn %s:\n",fname);
    fprintf(stderr,"%s\n\n",body);
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


_Noreturn void stack_overflow_error(void){
    /* The interpreter's recursion is about to exhaust the C control stack.  Signal
       a catchable STORAGE-CONDITION (a SERIOUS-CONDITION, the CL class for resource
       exhaustion) while a margin of stack remains, so a HANDLER-CASE / the REPL can
       recover instead of the process segfaulting.  The eval stack guard disables
       itself across this call (see evale), so signalling + the handler running in
       place do not re-trigger the guard. */
    if(STORAGE_CONDITION_Class->class != NULL){
        uword pool = open_live_pool();
        Ref fmt = string_new_cstring(
            "STORAGE ERROR: control stack exhausted (recursion too deep)");
        add_to_live_pool(fmt);
        Ref cond = make_simple_condition(STORAGE_CONDITION_Class, fmt, NIL_Symbol);
        add_to_live_pool(cond);
        signal_condition(cond, eval_global_environment());
        /* signal_condition returned — no handler matched; fall through to exit. */
        close_live_pool(pool);
    }
    fprintf(stderr,"\nSTORAGE ERROR: control stack exhausted (recursion too deep)\n\n");
    exit(EX_SOFTWARE);}


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
