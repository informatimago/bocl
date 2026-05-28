#ifndef version_h
#define version_h

/* bocl version. By project policy, every commit that touches code,
   tests, documents, or bootstrap material MUST bump BOCL_VERSION_PATCH
   by one. Initial post-Phase-2 release: 0.0.10. */

#define BOCL_VERSION_MAJOR 0
#define BOCL_VERSION_MINOR 0
#define BOCL_VERSION_PATCH 42

/* Stringify the version triple, e.g. "0.0.10". */
#define BOCL_VERSION_STRINGIZE(x) #x
#define BOCL_VERSION_TO_STRING(x) BOCL_VERSION_STRINGIZE(x)
#define BOCL_VERSION                                                \
    BOCL_VERSION_TO_STRING(BOCL_VERSION_MAJOR) "."                  \
    BOCL_VERSION_TO_STRING(BOCL_VERSION_MINOR) "."                  \
    BOCL_VERSION_TO_STRING(BOCL_VERSION_PATCH)

#endif
