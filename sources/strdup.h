#ifndef strdup_h
#define strdup_h

#if defined __USE_SVID || defined __USE_BSD || defined __USE_XOPEN_EXTENDED || defined __USE_XOPEN2K8
#include <string.h>
#else
char* strdup(const char* string);
#endif

#endif
