#ifndef strdup_h
#define strdup_h
#if !defined(_SVID_SOURCE) || !defined(_BSD_SOURCE) || _XOPEN_SOURCE < 500 || !(_XOPEN_SOURCE && _XOPEN_SOURCE_EXTENDED) || _POSIX_C_SOURCE < 200809L

char* strdup(const char* string);

#endif
#endif
