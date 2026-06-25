#ifndef environment_h
#define environment_h
#include "kernel_types.h"

Ref env_variable_value(Ref variable);
Ref env_set_variable_value(Ref new_value, Ref variable);
Ref environment_local_bindings(Ref environment);

#endif
