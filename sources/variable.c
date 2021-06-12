#include <assert.h>
#include "variable.h"
#include "kernel.h"
#include "kernel_objects.h"
#include "data.h"
#include "printer.h"

void make_constant_variable(Ref symbol){
    if(special_variable_p(symbol)){
        ERROR("Cannot make constant a special variable %s",
              string_cstring(symbol_name(symbol)));}
    Ref plist=symbol_plist(symbol);
    plist=putf(plist,S($CONSTANT),S(T));
    set_symbol_plist(symbol,plist);}

bool constant_variable_p(Ref symbol){
   return !nullp(getf(symbol_plist(symbol),S($CONSTANT),S(NIL)));}

void make_special_variable(Ref symbol){
    if(special_variable_p(symbol)){
        ERROR("Cannot make special a constant variable %s",
              string_cstring(symbol_name(symbol)));}
    Ref plist=symbol_plist(symbol);
    plist=putf(plist,S($SPECIAL),S(T));
    set_symbol_plist(symbol,plist);}

bool special_variable_p(Ref symbol){
    return !nullp(getf(symbol_plist(symbol),S($SPECIAL),S(NIL)));}

Ref define_constant(Ref symbol,Ref value){
    set_symbol_value(symbol,value);
    make_constant_variable(symbol);
    return symbol;}

Ref define_variable(Ref symbol,Ref value){
    make_special_variable(symbol);
    if(!(symbol_boundp(symbol)||eql(value,UNBOUND_Object))){
        set_symbol_value(symbol,value);}
    return symbol;}

Ref define_parameter(Ref symbol,Ref value){
    make_special_variable(symbol);
    set_symbol_value(symbol,value);
    return symbol;}

Ref variable_value(Ref symbol){
    if(symbol_boundp(symbol)){
        return symbol_value(symbol);}
    ERROR("Variable %s is not bound",
          string_cstring(symbol_name(symbol)));}

/**** THE END ****/
