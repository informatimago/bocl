#ifndef cons_h
#define cons_h
#include "kernel_types.h"

#define do_list(elementvar,listexpr)                                                     \
    Object* C(do_list_current,__LINE__); Object* elementvar;                             \
    for((C(do_list_current,__LINE__) = (listexpr),                                       \
         elementvar = ((cons_p(C(do_list_current,__LINE__)))                             \
                       ?cons_car(C(do_list_current,__LINE__))                            \
                       :NULL)) ;                                                         \
        cons_p(C(do_list_current,__LINE__)) ;                                            \
        (C(do_list_current,__LINE__) = cons_cdr(C(do_list_current,__LINE__)),            \
         elementvar = ((cons_p(C(do_list_current,__LINE__)))                             \
                       ?cons_car(C(do_list_current,__LINE__))                            \
                       :NULL)))




/* Cons */

Object* rplaca(Object* cell,Object* newcar);
Object* rplacd(Object* cell,Object* newcdr);

Object* car(Object* cell);
Object* cdr(Object* cell);
Object* caar(Object* cell);
Object* cadr(Object* cell);
Object* cdar(Object* cell);
Object* cddr(Object* cell);
Object* caaar(Object* cell);
Object* caadr(Object* cell);
Object* cadar(Object* cell);
Object* caddr(Object* cell);
Object* cdaar(Object* cell);
Object* cdadr(Object* cell);
Object* cddar(Object* cell);
Object* cdddr(Object* cell);

/* List */

word list_length(Object* list);

Object* push(Object* element,Object** list);
Object* pop(Object** list);

Object* nconc(Object* list,Object* tail);
Object* last(Object* list);
Object* list(Object* first_element,...);

Object* memq(Object* item,Object* list);

/* Plist */

Object* getf(Object* plist,Object* indicator,Object* default_value);
Object* putf(Object* plist,Object* indicator,Object* new_value);
bool remf(Object* plist,Object* indicator);

#endif
