#ifndef memory_h
#define memory_h
#include <stdlib.h>
#include <stdbool.h>
#include "kernel_types.h"

#ifdef strict_iso_c
/* TODO: we could also remove the fname parameter altogether */
#define NOT_IMPLEMENTED_YET()           error("some function","NOT IMPLEMENTED YET")
#define FATAL(message_format,...)       fatal("some function",message_format,__VA_ARGS__)
#define ERROR(message_format,...)       error("some function",message_format,__VA_ARGS__)
#define TYPE_ERROR(message_format,...)  type_error("some function",message_format,__VA_ARGS__)
#define CHECK_POINTER(pointer)          check_pointer("some function",pointer)
#define CHECK_SIZE_TO_UWORD(size)       check_size_to_uword("some function",size)
#define CHECK_SIZE_TO_HALFWORD(size)    check_size_to_halfword("some function",size)
#define CHECK_UWORD_TO_WORD(value)      check_uword_to_word("some function",value)
#else
#define NOT_IMPLEMENTED_YET()           error(__FUNCTION__,"NOT IMPLEMENTED YET")
#define FATAL(message_format,...)       fatal(__FUNCTION__,message_format,__VA_ARGS__)
#define ERROR(message_format,...)       error(__FUNCTION__,message_format,__VA_ARGS__)
#define TYPE_ERROR(message_format,...)  type_error(__FUNCTION__,message_format,__VA_ARGS__)
#define CHECK_POINTER(pointer)          check_pointer(__FUNCTION__,pointer)
#define CHECK_SIZE_TO_UWORD(size)       check_size_to_uword(__FUNCTION__,size)
#define CHECK_SIZE_TO_HALFWORD(size)    check_size_to_halfword(__FUNCTION__,size)
#define CHECK_UWORD_TO_WORD(value)      check_uword_to_word(__FUNCTION__,value)
#endif

_Noreturn void fatal(const char* fname,const char* message_format,...);
_Noreturn void error(const char* fname,const char* message_format,...);
_Noreturn void type_error(const char* fname,Object* object,Object* class);
void* check_pointer(const char* fname,void* pointer);
uword check_size_to_uword(const char* fname,size_t size);
halfword check_size_to_halfword(const char* fname,size_t size);
word check_uword_to_word(const char* fname,uword value);

void report_error(Object* error);

void* allocate(size_t size);

bool garbage_collection_is_enabled(void);
bool set_garbage_collection_enabled(bool on);
uword garbage_collect(void);

void set_rootset(Block* block);

void memory_initialize(void);

#endif
