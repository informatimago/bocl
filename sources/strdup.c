#if defined __USE_SVID || defined __USE_BSD || defined __USE_XOPEN_EXTENDED || defined __USE_XOPEN2K8
/* nope */
#else

#include <stdlib.h>
#include <string.h>
#include "strdup.h"

char* strdup(const char* string){
  char* buffer=malloc(1+strlen(string));
  if(buffer!=NULL){
      strcpy(buffer,string);}
  return buffer;}

#endif
