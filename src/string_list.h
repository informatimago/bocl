#ifndef string_list_h
#define string_list_h

typedef struct string_list_node  string_list;

string_list* string_list_cons(char* element,string_list* next);
char* string_list_first(string_list* list);
string_list* string_list_rest(string_list* list);
void string_list_free(string_list* list);
string_list* string_list_nreverse(string_list* list);

#define do_string_list(stringvar,stringlistexpr,body)                                                   \
    do{                                                                                                 \
        string_list* do_string_list_current ##  __LINE__ =(stringlistexpr);                             \
        while(do_string_list_current ## __LINE__){                                                      \
            char* stringvar=string_list_first(do_string_list_current ## __LINE__);                      \
            do_string_list_current ## __LINE__=string_list_rest(do_string_list_current ## __LINE__);    \
            { body }}}while(0)


#endif
