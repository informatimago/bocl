#ifndef eval_h
#define eval_h
#include "kernel.h"

Ref evale(Ref form,Ref environment,MultipleValues* results);
Ref eval_global_environment(void);

#endif
