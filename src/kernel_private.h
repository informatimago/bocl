#ifndef kernel_private_h
#define kernel_private_h
#include <stdint.h>
#include <stdbool.h>
#include "memory.h"
#include "macros.h"
#include "cons.h"


Ref boot_last(Ref list);
Ref boot_nconc(Ref list,Ref tail);
void boot_class_add_superclass(Ref class, Ref superclass);

#endif
