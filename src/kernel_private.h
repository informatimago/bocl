#ifndef kernel_private_h
#define kernel_private_h
#include <stdint.h>
#include <stdbool.h>
#include "memory.h"
#include "macros.h"
#include "cons.h"


Object* boot_last(Object* list);
Object* boot_nconc(Object* list,Object* tail);
void boot_class_add_superclass(Object* class, Object* superclass);

#endif
