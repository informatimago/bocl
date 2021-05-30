#ifndef printer_h
#define printer_h
#include "kernel_types.h"


Object* prin1_to_string(Object* object);

Object* print(Object* object,Object* stream);
Object* prin1(Object* object,Object* stream);
Object* princ(Object* object,Object* stream);
Object* terpri(Object* stream);

#endif
