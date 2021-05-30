#ifndef stream_h
#define stream_h
#include "kernel.h"
#include "macros.h"


Object* stream_open(Object* filespec,
                    Object* direction,
                    Object* element_type,
                    Object* if_exists,
                    Object* if_does_not_exist,
                    Object* external_format);

void stream_initialize(void);

#endif
