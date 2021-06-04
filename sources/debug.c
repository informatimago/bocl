#include <stdio.h>
#include <stdlib.h>
#include "debug.h"
#include "kernel.h"
#include "kernel_types.h"
#include "kernel_objects.h"
#include "stream.h"
#include "variable.h"
#include "printer.h"
#include "cons.h"



void dump_obarray(Ref obarray){
    if(!consp(obarray)){
        fprintf(stderr,"dump obarray: NOT A CONS CELL, but %s\n",
                string_cstring(symbol_name(obarray->class->slots->class.name)));
        abort();}
    fprintf(stderr,"dump obarray: %-16s\n",
            string_cstring(symbol_name(cons_car(obarray))));
    Ref syms=cons_cdr(obarray);
    long sc=0;
    while(consp(syms)){
        sc++;
        fprintf(stderr,"    %s\n",string_cstring(symbol_name(cons_car(syms))));
        if(symbol_package(cons_car(syms))!=obarray){
            fprintf(stderr,"wrong package for %s\n",
                    string_cstring(symbol_name(cons_car(syms))));}
        syms=cons_cdr(syms);}
    fprintf(stderr," = %3ld symbols\n",sc);
    if(syms!=NIL_Symbol){
        fprintf(stderr," dotted-list obarray, last cdr is a %s\n",
                string_cstring(symbol_name(syms->class->slots->class.name)));}}

void dump_kernel_classes(){
    fprintf(stderr,"Dumping kernel_classes\n");
    if(!kernel_classes){
        fprintf(stderr,"kernel_classes is NULL!\n");
        return;}
    fprintf(stderr,"kernel_classes length is %lld\n",
            list_length(kernel_classes));
    int i=0;
    Ref current=kernel_classes;
    while(consp(current)){
        Ref class=cons_car(current);
        current=cons_cdr(current);
        i++;
        if(!class){
            fprintf(stderr,"%3d: NULL!\n",i);
            continue;}
        Ref class_name_class=class->slots->class.name;
        if(!class_name_class){
            fprintf(stderr,"%3d: %p NULL name!\n",i,(void*)class);
            continue;}
        if(!class_of(class)){
            fprintf(stderr,"%3d: %-34s NULL metaclass!\n",
                    i,
                    string_cstring(symbol_name(class_name_class)));
            continue;}
        Ref class_name_metaclass=class_of(class)->slots->class.name;
        if(!class_name_metaclass){
            fprintf(stderr,"%3d: %-34s %p NULL name of metaclass!\n",
                    i,
                    string_cstring(symbol_name(class_name_class)),
                    (void*)class_of(class));
            continue;}
        fprintf(stderr,"class-name: %-34s   metaclass: %-26s    direct-superclasses:",
                string_cstring(symbol_name(class_name_class)),
                string_cstring(symbol_name(class_name_metaclass)));

        Ref ccurrent=class->slots->class.direct_superclasses;
        if(!ccurrent){
            fprintf(stderr,"C NULL direct-superclasses!\n");
            continue;}
        while(consp(ccurrent)){
            Ref super=cons_car(ccurrent);
            current=cons_cdr(ccurrent);
            Ref class_name_super=super->slots->class.name;
            fprintf(stderr," %s",string_cstring(symbol_name(class_name_super)));}
        fprintf(stderr,"\n");}}


void debug_typep(Ref object,Ref class){
    Ref object_class=class_of(object);
    fprintf(stderr,"Is object %p of type %p\n",
            (void*)object,
            (void*)class);
    if(object==NULL){
        fprintf(stderr,"NULL object!\n");
        abort();}
    if(class==NULL){
        fprintf(stderr,"NULL class!\n");
        abort();}
    fprintf(stderr,"Is object of class %p of type %p\n",
            (void*)symbol_name(object_class->slots->class.name),
            (void*)symbol_name(       class->slots->class.name));
    fprintf(stderr,"Is object of class %s of type %s\n",
            string_cstring(symbol_name(object_class->slots->class.name)),
            string_cstring(symbol_name(       class->slots->class.name)));}


void debug(void){

    printf("N %p\n",(void*)NIL_Symbol);
    printf("E %p\n",(void*)KERNEL_obarray);
    printf("S %p\n",(void*)S(NIL));
    printf("NIL_Symbol           = ");prin1(NIL_Symbol,T_Symbol);terpri(T_Symbol);
    printf("KERNEL_obarray       = ");prin1(KERNEL_obarray,T_Symbol);terpri(T_Symbol);
    printf("S(NIL)               = ");prin1(S(NIL),T_Symbol);terpri(T_Symbol);
    printf("f %p\n",(void*)find_symbol(string_new_cstring("NIL"),KERNEL_obarray));
    printf("find-symbol(\"NIL\") = ");prin1(find_symbol(string_new_cstring("NIL"),KERNEL_obarray),T_Symbol);terpri(T_Symbol);


}
