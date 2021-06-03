#include <stdarg.h>
#include "cons.h"
#include "kernel.h"
#include "kernel_objects.h"
#include "data.h"


/* Cons */

Ref rplaca(Ref cell,Ref newcar){
    check_class(cell,CONS_Class);
    return cons_rplaca(cell,newcar);}

Ref rplacd(Ref cell,Ref newcdr){
    check_class(cell,CONS_Class);
    return cons_rplacd(cell,newcdr);}

Ref car(Ref cell){
    check_type(cell,LIST_Class);
    if(cons_p(cell)){
        return cons_car(cell);}
    else{
        return NIL_Symbol;}}

Ref cdr(Ref cell){
    check_type(cell,LIST_Class);
    if(cons_p(cell)){
        return cons_cdr(cell);}
    else{
        return NIL_Symbol;}}

Ref caar(Ref cell){ return car(car(cell)); }
Ref cadr(Ref cell){ return car(cdr(cell)); }
Ref cdar(Ref cell){ return cdr(car(cell)); }
Ref cddr(Ref cell){ return cdr(cdr(cell)); }

Ref caaar(Ref cell){ return car(car(car(cell))); }
Ref caadr(Ref cell){ return car(car(cdr(cell))); }
Ref cadar(Ref cell){ return car(cdr(car(cell))); }
Ref caddr(Ref cell){ return car(cdr(cdr(cell))); }

Ref cdaar(Ref cell){ return cdr(car(car(cell))); }
Ref cdadr(Ref cell){ return cdr(car(cdr(cell))); }
Ref cddar(Ref cell){ return cdr(cdr(car(cell))); }
Ref cdddr(Ref cell){ return cdr(cdr(cdr(cell))); }

/* List */


uword list_length(Ref list){
    if(null_p(list)){
        return 0;}
    uword i=0;
    while(cons_p(list)){
        i++;
        list=cons_cdr(list);}
    return i;}

Ref push(Ref element,Ref* list){
    return (*list)=cons(element,(*list));}

Ref pop(Ref* list){
    Ref result=car(*list);
    (*list)=cdr(*list);
    return result;}


Ref last(Ref list){
    // TODO: check_class(list,LIST_Class); // move NIL_Symbol to the NULL_Class !
    Ref next;
    if(cons_p(list)){
        while(cons_p(next=cons_cdr(list))){
            list=next;}
        return list;}
    else if(null_p(list)){
        return list;}
    else{
        TYPE_ERROR(list,LIST_Class);}}

Ref delete(Ref object,Ref list){
    if(null_p(list)){
        return list;}
    if(eql(object,car(list))){
        return cdr(list);}
    Ref previous=list;
    while(cons_p(cdr(previous)) && !eql(object,cadr(previous))){
        previous=cdr(previous);}
    if(cons_p(cdr(previous))){
        rplacd(cdr(previous),cddr(previous));}
    return list;}

Ref nconc(Ref list,Ref tail){
    if(null_p(list)){
        return tail;}
    else{
        Ref last_cell=last(list);
        rplacd(last_cell,tail);
        return list;}}

Ref list(Ref first_element,...){
    Ref list=cons(first_element,NIL_Symbol);
    Ref tail=list;
    va_list ap;
    va_start(ap,first_element);
    Ref element=va_arg(ap,Ref);
    while(element){
        cons_rplacd(tail,cons(element,NIL_Symbol));
        tail=cons_cdr(tail);
        element=va_arg(ap,Ref);
    }
    va_end(ap);
    return list;}

Ref copy_list(Ref list){
    if(null_p(list)){
        return list;}
    Ref result=cons(car(list),NIL_Symbol);
    Ref tail=result;
    list=cdr(list);
    while(cons_p(list)){
        rplacd(tail,cons(car(list),NIL_Symbol));
        list=cdr(list);}
    rplacd(tail,cdr(list));
    return result;}

Ref memq(Ref item,Ref list){
    while(cons_p(list) && !eql(item,car(list))){
        list=cdr(list);}
    return list;}

Ref nreverse(Ref list){
    if(list==NIL_Symbol){
        return NIL_Symbol;}
    Ref result=NIL_Symbol;
    while(cons_p(list)){
        Ref current=list;
        list=cdr(list);
        rplacd(current,result);
        result=current;}
    return result;}

Ref reverse(Ref list){
    if(list==NIL_Symbol){
        return NIL_Symbol;}
    Ref result=NIL_Symbol;
    while(cons_p(list)){
        result=cons(car(list),result);
        list=cdr(list);}
    return result;}

/* Plist */

Ref getf(Ref plist,Ref indicator,Ref default_value){
    Ref current=plist;
    while(cons_p(current) && !eql(indicator,car(current))){
        current=cddr(current);}
    return(cons_p(current))
            ?cadr(current)
            :default_value;}

Ref putf(Ref plist,Ref indicator,Ref new_value){
    Ref current=plist;
    while(cons_p(current) && !eql(indicator,car(current))){
        current=cddr(current);}
    if(cons_p(current)){
        rplaca(cdr(current),new_value);
        return plist;}
    else{
        return cons(indicator,cons(new_value,plist));}}

bool remf(Ref plist,Ref indicator){
    if(cons_p(plist) && eql(indicator,car(plist))){
        /* first pair: we copy the second pair over the first,
        and skip the second pair */
        Ref removed=cddr(plist);
        rplaca(plist,car(removed));
        rplacd(cdr(plist),cdr(removed));
        return true;}
    /* for the other pairs, we keep a reference to the previous cell
    and just skip the indicated pair */
    Ref previous=cdr(plist);
    while(cons_p(cdr(previous)) && !eql(indicator,cadr(previous))){
        previous=cddr(previous);}
    if(cons_p(cdr(previous))){
        rplacd(previous,cdddr(previous));
        return true;}
    else{
        return false;}}


/**** THE END ****/
