#ifndef error_h
#define error_h
#include <stdlib.h>
#include <stdbool.h>
#include "kernel_types.h"

#if defined(__STDC_ISO_10646__) || defined(__USE_ISOC11) || defined(strict_iso_c)
/* TODO: we could also remove the fname parameter altogether */
#ifndef __FUNCTION__
#define __FUNCTION__ "some function"
#endif
#endif


#define FATAL(message_format,...)                    fatal(__FUNCTION__,"FATAL ERROR:" message_format,__VA_ARGS__)
#define ERROR(message_format,...)                    error(__FUNCTION__,"ERROR: " message_format,__VA_ARGS__)
#define ARITHMETIC_ERROR(message_format,...)         error(__FUNCTION__,"ARITHMETIC ERROR: " message_format,__VA_ARGS__)
#define TYPE_ERROR(object,class)                     type_error(__FUNCTION__,object,class)
#define OUT_OF_BOUNDS(vector,length,index)           error(__FUNCTION__,"OUT-OF-BOUNDS ERROR: index %"WORD_FORMAT" is out of bound to index (max %"WORD_FORMAT")",integer_value(index),integer_value(length))

#define END_OF_FILE(stream)                          error(__FUNCTION__,"END-OF-FILE ERROR: on stream %s",string_cstring(prin1_to_string(stream)))
#define STREAM_ERROR(stream)                         error(__FUNCTION__,"INPUT-OUTPUT ERROR: on stream %s",string_cstring(prin1_to_string(stream)))
#define READER_ERROR(stream,message_format,...)      error(__FUNCTION__,"READER ERROR: on stream %s " message_format,string_cstring(prin1_to_string(stream)),__VA_ARGS__)

#define NOT_IMPLEMENTED_YET()                        error(__FUNCTION__,"NOT IMPLEMENTED YET")

#define CHECK_HALFWORD_TO_INT(value)                 check_halfword_to_int(__FUNCTION__,value)
#define CHECK_LONG_TO_UWORD(value)                   check_long_to_uword(__FUNCTION__,value)
#define CHECK_POINTER(pointer)                       check_pointer(__FUNCTION__,pointer)
#define CHECK_SIZE_TO_HALFWORD(size)                 check_size_to_halfword(__FUNCTION__,size)
#define CHECK_SIZE_TO_UWORD(size)                    check_size_to_uword(__FUNCTION__,size)
#define CHECK_UWORD_TO_HALFWORD(size)                check_uword_to_halfword(__FUNCTION__,size)
#define CHECK_UWORD_TO_WORD(value)                   check_uword_to_word(__FUNCTION__,value)
#define CHECK_WORD_TO_SIZE(value)                    check_word_to_size(__FUNCTION__,value)
#define CHECK_WORD_TO_UWORD(value)                   check_word_to_uword(__FUNCTION__,value)

_Noreturn void fatal(const char* fname,const char* message_format,...);
_Noreturn void error(const char* fname,const char* message_format,...);
_Noreturn void type_error(const char* fname,Ref object,Ref class);

void*    check_pointer           (const char* fname,void*    pointer);
uword    check_word_to_uword     (const char* fname,word     value);
size_t   check_word_to_size      (const char* fname,word     value);
word     check_uword_to_word     (const char* fname,uword    value);
halfword check_uword_to_halfword (const char* fname,uword    value);
int      check_halfword_to_int   (const char* fname,halfword value);
uword    check_size_to_uword     (const char* fname,size_t   size);
halfword check_size_to_halfword  (const char* fname,size_t   size);
uword    check_long_to_uword     (const char* fname,long     value);

void report_error(Ref error);

void error_initialize(void);

#endif
