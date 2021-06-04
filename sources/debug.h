#ifndef debug_h
#define debug_h
#include "kernel_types.h"

void dump_obarray(Ref obarray);
void dump_kernel_classes(void);
void debug_typep(Ref object,Ref class);
void debug(void);

#endif
