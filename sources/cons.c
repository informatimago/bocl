#include <stdarg.h>
#include "cons.h"
#include "kernel.h"
#include "kernel_objects.h"
#include "data.h"
#include "memory.h"

/* Cons */

Ref rplaca(Ref cell,Ref newcar){
    check_class(cell,CONS_Class);
    return cons_rplaca(cell,newcar);}

Ref rplacd(Ref cell,Ref newcdr){
    check_class(cell,CONS_Class);
    return cons_rplacd(cell,newcdr);}

Ref car(Ref cell){
    check_type(cell,LIST_Class);
    if(consp(cell)){
        return cons_car(cell);}
    else{
        return NIL_Symbol;}}

Ref cdr(Ref cell){
    check_type(cell,LIST_Class);
    if(consp(cell)){
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


Ref caaaar(Ref cell){ return car(car(car(car(cell)))); }
Ref caaadr(Ref cell){ return car(car(car(cdr(cell)))); }
Ref caadar(Ref cell){ return car(car(cdr(car(cell)))); }
Ref caaddr(Ref cell){ return car(car(cdr(cdr(cell)))); }

Ref cadaar(Ref cell){ return car(cdr(car(car(cell)))); }
Ref cadadr(Ref cell){ return car(cdr(car(cdr(cell)))); }
Ref caddar(Ref cell){ return car(cdr(cdr(car(cell)))); }
Ref cadddr(Ref cell){ return car(cdr(cdr(cdr(cell)))); }

Ref cdaaar(Ref cell){ return cdr(car(car(car(cell)))); }
Ref cdaadr(Ref cell){ return cdr(car(car(cdr(cell)))); }
Ref cdadar(Ref cell){ return cdr(car(cdr(car(cell)))); }
Ref cdaddr(Ref cell){ return cdr(car(cdr(cdr(cell)))); }

Ref cddaar(Ref cell){ return cdr(cdr(car(car(cell)))); }
Ref cddadr(Ref cell){ return cdr(cdr(car(cdr(cell)))); }
Ref cdddar(Ref cell){ return cdr(cdr(cdr(car(cell)))); }
Ref cddddr(Ref cell){ return cdr(cdr(cdr(cdr(cell)))); }

/* List */


uword list_length(Ref list){
    if(nullp(list)){
        return 0;}
    uword i=0;
    while(consp(list)){
        i++;
        list=cons_cdr(list);}
    return i;}

Ref push(Ref element,Ref* list){
    uword pool=open_live_pool();
    add_to_live_pool(element);
    add_to_live_pool(*list);
    (*list)=cons(element,(*list));
    close_live_pool(pool);
    return (*list);}

Ref pop(Ref* list){
    Ref result=car(*list);
    (*list)=cdr(*list);
    return result;}


Ref last(Ref list){
    // TODO: check_class(list,LIST_Class); // move NIL_Symbol to the NULL_Class !
    Ref next;
    if(consp(list)){
        while(consp(next=cons_cdr(list))){
            list=next;}
        return list;}
    else if(nullp(list)){
        return list;}
    else{
        TYPE_ERROR(list,LIST_Class);}}

Ref delete(Ref object,Ref list){
    if(nullp(list)){
        return list;}
    if(eql(object,car(list))){
        return cdr(list);}
    Ref previous=list;
    while(consp(cdr(previous)) && !eql(object,cadr(previous))){
        previous=cdr(previous);}
    if(consp(cdr(previous))){
        rplacd(cdr(previous),cddr(previous));}
    return list;}

Ref nconc(Ref list,Ref tail){
    if(nullp(list)){
        return tail;}
    else{
        Ref last_cell=last(list);
        rplacd(last_cell,tail);
        return list;}}

Ref list(Ref first_element,...){
    uword pool=open_live_pool();
    /* save the arguments to the rootset before allocating anything: */
    add_to_live_pool(first_element);
    va_list ap;
    va_start(ap,first_element);
    Ref element=va_arg(ap,Ref);
    while(element){
        add_to_live_pool(element);
        element=va_arg(ap,Ref);
    }
    va_end(ap);
    /* then allocate the list: */
    Ref list=cons(first_element,NIL_Symbol);
    Ref tail=list;
    va_start(ap,first_element);
    element=va_arg(ap,Ref);
    while(element){
        cons_rplacd(tail,cons(element,NIL_Symbol));
        tail=cons_cdr(tail);
        element=va_arg(ap,Ref);
    }
    va_end(ap);
    close_live_pool(pool);
    return list;}

Ref copy_list(Ref list){
    uword pool=open_live_pool();
    add_to_live_pool(list);
    if(nullp(list)){
        return list;}
    Ref result=cons(car(list),NIL_Symbol);
    Ref tail=result;
    list=cdr(list);
    while(consp(list)){
        rplacd(tail,cons(car(list),NIL_Symbol));
        list=cdr(list);}
    rplacd(tail,cdr(list));
    close_live_pool(pool);
    return result;}

Ref memq(Ref item,Ref list){
    while(consp(list) && !eql(item,car(list))){
        list=cdr(list);}
    return list;}

Ref assq(Ref key,Ref alist){
    while(consp(alist) && !eql(key,caar(alist))){
        alist=cdr(alist);}
    return car(alist);}

Ref firsts(Ref alist){
    uword pool=open_live_pool();
    add_to_live_pool(alist);
    Ref result=NIL_Symbol;
    while(consp(alist)){
        if(consp(car(alist))){
            push(caar(alist),&result);}
        alist=cdr(alist);}
    result=nreverse(result);
    close_live_pool(pool);
    return result;}

Ref nreverse(Ref list){
    if(list==NIL_Symbol){
        return NIL_Symbol;}
    Ref result=NIL_Symbol;
    while(consp(list)){
        Ref current=list;
        list=cdr(list);
        rplacd(current,result);
        result=current;}
    return result;}

Ref reverse(Ref list){
    uword pool=open_live_pool();
    add_to_live_pool(list);
    if(list==NIL_Symbol){
        close_live_pool(pool);
        return NIL_Symbol;}
    Ref result=NIL_Symbol;
    while(consp(list)){
        result=cons(car(list),result);
        list=cdr(list);}
    close_live_pool(pool);
    return result;}

/* Plist */

Ref getf(Ref plist,Ref indicator,Ref default_value){
    Ref current=plist;
    while(consp(current) && !eql(indicator,car(current))){
        current=cddr(current);}
    return(consp(current))
            ?cadr(current)
            :default_value;}

Ref putf(Ref plist,Ref indicator,Ref new_value){
    uword pool=open_live_pool();
    add_to_live_pool(plist);
    add_to_live_pool(indicator);
    add_to_live_pool(new_value);
    Ref current=plist;
    while(consp(current) && !eql(indicator,car(current))){
        current=cddr(current);}
    if(consp(current)){
        rplaca(cdr(current),new_value);
        close_live_pool(pool);
        return plist;}
    else{
        Ref result=cons(indicator,cons(new_value,plist));
        close_live_pool(pool);
        return result;}}

bool remf(Ref plist,Ref indicator){
    if(consp(plist) && eql(indicator,car(plist))){
        /* first pair: we copy the second pair over the first,
        and skip the second pair */
        Ref removed=cddr(plist);
        rplaca(plist,car(removed));
        rplacd(cdr(plist),cdr(removed));
        return true;}
    /* for the other pairs, we keep a reference to the previous cell
    and just skip the indicated pair */
    Ref previous=cdr(plist);
    while(consp(cdr(previous)) && !eql(indicator,cadr(previous))){
        previous=cddr(previous);}
    if(consp(cdr(previous))){
        rplacd(previous,cdddr(previous));
        return true;}
    else{
        return false;}}


Ref list_position(Ref object,Ref list){
    uword pos=0;
    while(consp(list) && (object!=car(list))){
        pos++;
        list=cdr(list);}
    return consp(list)
            ?integer_from_uword(pos)
            :NIL_Symbol;}


/**** THE END ****/
