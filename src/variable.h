#ifndef variable_h
#define variable_h
#include "kernel_types.h"

void make_constant_variable(Object* symbol);
bool constant_variable_p(Object* symbol);

void make_special_variable(Object* symbol);
bool special_variable_p(Object* symbol);

Object* define_constant(Object* symbol,Object* value);
Object* define_variable(Object* symbol,Object* value);
Object* define_parameter(Object* symbol,Object* value);
Object* variable_value(Object* symbol);

#endif
