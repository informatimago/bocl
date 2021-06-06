#ifndef cons_h
#define cons_h
#include "kernel_types.h"
#include "kernel_functions.h"
/* The built-in-functions are declared in kernel_functions.h */

#define do_list(elementvar,listexpr)                                                     \
    Ref CS(do_list_current,__LINE__); Ref elementvar;                             \
    for((CS(do_list_current,__LINE__) = (listexpr),                                       \
         elementvar = ((consp(CS(do_list_current,__LINE__)))                             \
                       ?cons_car(CS(do_list_current,__LINE__))                            \
                       :NULL)) ;                                                         \
        consp(CS(do_list_current,__LINE__)) ;                                            \
        (CS(do_list_current,__LINE__) = cons_cdr(CS(do_list_current,__LINE__)),            \
         elementvar = ((consp(CS(do_list_current,__LINE__)))                             \
                       ?cons_car(CS(do_list_current,__LINE__))                            \
                       :NULL)))

Ref push(Ref element,Ref* list);
Ref pop(Ref* list);
Ref list(Ref first_element,...);

#endif
