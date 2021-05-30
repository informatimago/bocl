#include <assert.h>
#include "variable.h"
#include "kernel.h"
#include "kernel_objects.h"
#include "data.h"
#include "printer.h"

void make_constant_variable(Object* symbol){
    if(special_variable_p(symbol)){
        error(__FUNCTION__,"Cannot make constant a special variable %s",
              string_cstring(symbol_name(symbol)));}
    Object* plist=symbol_plist(symbol);
    plist=putf(plist,S($CONSTANT),S(T));
    symbol_set_plist(symbol,plist);}

bool constant_variable_p(Object* symbol){
   return !null_p(getf(symbol_plist(symbol),S($CONSTANT),S(NIL)));}

void make_special_variable(Object* symbol){
    if(special_variable_p(symbol)){
        error(__FUNCTION__,"Cannot make special a constant variable %s",
              string_cstring(symbol_name(symbol)));}
    Object* plist=symbol_plist(symbol);
    plist=putf(plist,S($SPECIAL),S(T));
    symbol_set_plist(symbol,plist);}

bool special_variable_p(Object* symbol){
    return !null_p(getf(symbol_plist(symbol),S($SPECIAL),S(NIL)));}

Object* define_constant(Object* symbol,Object* value){
    symbol_set_value(symbol,value);
    make_constant_variable(symbol);
    return symbol;}

Object* define_variable(Object* symbol,Object* value){
    make_special_variable(symbol);
    if(!(symbol_boundp(symbol)||eql(value,UNBOUND_Object))){
        symbol_set_value(symbol,value);}
    return symbol;}

Object* define_parameter(Object* symbol,Object* value){
    make_special_variable(symbol);
    symbol_set_value(symbol,value);
    return symbol;}

Object* variable_value(Object* symbol){
    if(symbol_boundp(symbol)){
        return symbol_value(symbol);}
    error(__FUNCTION__,"Variable %s is not bound",
          string_cstring(symbol_name(symbol)));}          

/**** THE END ****/
