#ifndef string_list_h
#define string_list_h
#include "macros.h"

typedef struct string_list_node  string_list;

string_list* string_list_cons(char* element,string_list* next);
char* string_list_first(string_list* list);
string_list* string_list_rest(string_list* list);
void string_list_free(string_list* list);
string_list* string_list_nreverse(string_list* list);

#define do_string_list(stringvar,stringlistexpr)                                                    \
    string_list* C(do_string_list_current,__LINE__) ; char* stringvar;                              \
    for((C(do_string_list_current,__LINE__) = (stringlistexpr),                                     \
         stringvar = ((C(do_string_list_current,__LINE__) != NULL)                                  \
                      ?string_list_first(C(do_string_list_current,__LINE__))                        \
                      :NULL)) ;                                                                     \
        C(do_string_list_current,__LINE__) != NULL ;                                                \
        (C(do_string_list_current,__LINE__) = string_list_rest(C(do_string_list_current,__LINE__)), \
         stringvar = ((C(do_string_list_current,__LINE__) != NULL)                                  \
                      ?string_list_first(C(do_string_list_current,__LINE__))                        \
                               :NULL)))

#endif
