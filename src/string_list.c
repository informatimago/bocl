#include <stdlib.h>
#include "string_list.h"

typedef struct string_list_node {
    char* element;
    struct string_list_node* next;
} string_list_node, string_list;


string_list* string_list_cons(char* element,string_list* next){
    string_list* that=malloc(sizeof(*that));
    that->element=element;
    that->next=next;
    return that;}

void string_list_push(char* element,string_list** list){
    (*list)=string_list_cons(element,*list);}

char* string_list_first(string_list* list){
    return (list==NULL)
            ?NULL
            :list->element;}

string_list* string_list_rest(string_list* list){
    return (list==NULL)
            ?NULL
            :list->next;}

void string_list_free(string_list* list){
    {do_string_list(element,list){
            free(element);}}}

string_list* string_list_nreverse(string_list* list){
    if(list==NULL){
        return NULL;}
    string_list* result=NULL;
    while(list){
        string_list* current=list;
        list=list->next;
        current->next=result;
        result=current;}
    return result;}

