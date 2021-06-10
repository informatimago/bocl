#ifndef variable_h
#define variable_h
#include "kernel_types.h"

void make_constant_variable(Ref symbol);
bool constant_variable_p(Ref symbol);
void make_special_variable(Ref symbol);
bool special_variable_p(Ref symbol);

Ref define_constant(Ref symbol,Ref value);
Ref define_variable(Ref symbol,Ref value);
Ref define_parameter(Ref symbol,Ref value);
Ref variable_value(Ref symbol);

#endif
