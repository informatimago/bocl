#include <stdarg.h>
#include "cons.h"
#include "kernel.h"
#include "kernel_objects.h"
#include "data.h"


/* Cons */

Object* rplaca(Object* cell,Object* newcar){
    check_class(cell,CONS_Class);
    return cons_rplaca(cell,newcar);}

Object* rplacd(Object* cell,Object* newcdr){
    check_class(cell,CONS_Class);
    return cons_rplacd(cell,newcdr);}

Object* car(Object* cell){
    check_type(cell,LIST_Class);
    if(cons_p(cell)){
        return cons_car(cell);}
    else{
        return NIL_Symbol;}}

Object* cdr(Object* cell){
    check_type(cell,LIST_Class);
    if(cons_p(cell)){
        return cons_cdr(cell);}
    else{
        return NIL_Symbol;}}

Object* caar(Object* cell){ return car(car(cell)); }
Object* cadr(Object* cell){ return car(cdr(cell)); }
Object* cdar(Object* cell){ return cdr(car(cell)); }
Object* cddr(Object* cell){ return cdr(cdr(cell)); }

Object* caaar(Object* cell){ return car(car(car(cell))); }
Object* caadr(Object* cell){ return car(car(cdr(cell))); }
Object* cadar(Object* cell){ return car(cdr(car(cell))); }
Object* caddr(Object* cell){ return car(cdr(cdr(cell))); }

Object* cdaar(Object* cell){ return cdr(car(car(cell))); }
Object* cdadr(Object* cell){ return cdr(car(cdr(cell))); }
Object* cddar(Object* cell){ return cdr(cdr(car(cell))); }
Object* cdddr(Object* cell){ return cdr(cdr(cdr(cell))); }

/* List */


uword list_length(Object* list){
    if(null_p(list)){
        return 0;}
    uword i=0;
    while(cons_p(list)){
        i++;
        list=cons_cdr(list);}
    return i;}

Object* push(Object* element,Object** list){
    return (*list)=cons(element,(*list));}

Object* pop(Object** list){
    Object* result=car(*list);
    (*list)=cdr(*list);
    return result;}


Object* last(Object* list){
    // TODO: check_class(list,LIST_Class); // move NIL_Symbol to the NULL_Class !
    Object* next;
    if(cons_p(list)){
        while(cons_p(next=cons_cdr(list))){
            list=next;}
        return list;}
    else if(null_p(list)){
        return list;}
    else{
        TYPE_ERROR(list,LIST_Class);}}

Object* delete(Object* object,Object* list){
    if(null_p(list)){
        return list;}
    if(eql(object,car(list))){
        return cdr(list);}
    Object* previous=list;
    while(cons_p(cdr(previous)) && !eql(object,cadr(previous))){
        previous=cdr(previous);}
    if(cons_p(cdr(previous))){
        rplacd(cdr(previous),cddr(previous));}
    return list;}

Object* nconc(Object* list,Object* tail){
    if(null_p(list)){
        return tail;}
    else{
        Object* last_cell=last(list);
        rplacd(last_cell,tail);
        return list;}}

Object* list(Object* first_element,...){
    Object* list=cons(first_element,NIL_Symbol);
    Object* tail=list;
    va_list ap;
    va_start(ap,first_element);
    Object* element=va_arg(ap,Object*);
    while(element){
        cons_rplacd(tail,cons(element,NIL_Symbol));
        tail=cons_cdr(tail);
        element=va_arg(ap,Object*);
    }
    va_end(ap);
    return list;}

Object* copy_list(Object* list){
    if(null_p(list)){
        return list;}
    Object* result=cons(car(list),NIL_Symbol);
    Object* tail=result;
    list=cdr(list);
    while(cons_p(list)){
        rplacd(tail,cons(car(list),NIL_Symbol));
        list=cdr(list);}
    rplacd(tail,cdr(list));
    return result;}

Object* memq(Object* item,Object* list){
    while(cons_p(list) && !eql(item,car(list))){
        list=cdr(list);}
    return list;}

Object* nreverse(Object* list){
    if(list==NIL_Symbol){
        return NIL_Symbol;}
    Object* result=NIL_Symbol;
    while(cons_p(list)){
        Object* current=list;
        list=cdr(list);
        rplacd(current,result);
        result=current;}
    return result;}

Object* reverse(Object* list){
    if(list==NIL_Symbol){
        return NIL_Symbol;}
    Object* result=NIL_Symbol;
    while(cons_p(list)){
        result=cons(car(list),result);
        list=cdr(list);}
    return result;}

/* Plist */

Object* getf(Object* plist,Object* indicator,Object* default_value){
    Object* current=plist;
    while(cons_p(current) && !eql(indicator,car(current))){
        current=cddr(current);}
    return(cons_p(current))
            ?cadr(current)
            :default_value;}

Object* putf(Object* plist,Object* indicator,Object* new_value){
    Object* current=plist;
    while(cons_p(current) && !eql(indicator,car(current))){
        current=cddr(current);}
    if(cons_p(current)){
        rplaca(cdr(current),new_value);
        return plist;}
    else{
        return cons(indicator,cons(new_value,plist));}}

bool remf(Object* plist,Object* indicator){
    if(cons_p(plist) && eql(indicator,car(plist))){
        /* first pair: we copy the second pair over the first,
        and skip the second pair */
        Object* removed=cddr(plist);
        rplaca(plist,car(removed));
        rplacd(cdr(plist),cdr(removed));
        return true;}
    /* for the other pairs, we keep a reference to the previous cell
    and just skip the indicated pair */
    Object* previous=cdr(plist);
    while(cons_p(cdr(previous)) && !eql(indicator,cadr(previous))){
        previous=cddr(previous);}
    if(cons_p(cdr(previous))){
        rplacd(previous,cdddr(previous));
        return true;}
    else{
        return false;}}


/**** THE END ****/
