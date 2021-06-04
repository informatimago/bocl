#if !defined(_SVID_SOURCE) || !defined(_BSD_SOURCE) || _XOPEN_SOURCE < 500 || !(_XOPEN_SOURCE && _XOPEN_SOURCE_EXTENDED) || _POSIX_C_SOURCE < 200809L
#include <stdlib.h>
#include <string.h>
#include "strdup.h"

char* strdup(const char* string){
  char* buffer=malloc(1+strlen(string));
  if(buffer!=NULL){
      strcpy(buffer,string);}
  return buffer;}

#endif
