#ifndef debug_h
#define debug_h
#include "kernel_types.h"

void dump_obarray(Object* obarray);
void dump_kernel_classes(void);
void debug_typep(Object* object,Object* class);
void debug(void);

#endif
