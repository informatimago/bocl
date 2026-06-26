#ifndef cons_h
#define cons_h
#include "kernel_types.h"
#include "kernel_functions.h"
/* The built-in-functions are declared in kernel_functions.h */

#define dolist(elementvar,listexpr)                                                     \
    Ref CS(dolist_current,__LINE__); Ref elementvar;                             \
    for((CS(dolist_current,__LINE__) = (listexpr),                                       \
         elementvar = ((consp(CS(dolist_current,__LINE__)))                             \
                       ?cons_car(CS(dolist_current,__LINE__))                            \
                       :NULL)) ;                                                         \
        consp(CS(dolist_current,__LINE__)) ;                                            \
        (CS(dolist_current,__LINE__) = cons_cdr(CS(dolist_current,__LINE__)),            \
         elementvar = ((consp(CS(dolist_current,__LINE__)))                             \
                       ?cons_car(CS(dolist_current,__LINE__))                            \
                       :NULL)))

Ref push(Ref element,Ref* list);
Ref pop(Ref* list);
Ref list(Ref first_element,...);
Ref bocl_append(Ref lists);   /* APPEND of a list of lists (last shared, rest copied) */

#endif
