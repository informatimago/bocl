#ifndef cons_h
#define cons_h
#include "kernel_types.h"
#include "kernel_functions.h"
/* The built-in-functions are declared in kernel_functions.h */

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

Object* push(Object* element,Object** list);
Object* pop(Object** list);
Object* list(Object* first_element,...);

#endif
