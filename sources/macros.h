#ifndef macros_h
#define macros_h
#include "kernel.h"

#define CX(a,b) a ## _ ## b
#define CS(a,b) CX(a,b)

/* S(FOO)/K(FOO) name fixed kernel symbols/keywords.  Interning is expensive
   (build a Lisp string, hash it, walk/lookup the obarray) and these macros sit
   on the hottest paths — e.g. evale runs S($MACRO) on every function call during
   operator resolution.  Cache the interned object per call site: the very first
   evaluation interns, every later one returns the cached Ref.  This is safe:
   interned kernel symbols live forever in the obarray (rooted via the package
   registry) and the collector is non-moving, so the cached pointer stays valid.
   (Uses a GCC/Clang statement-expression; bocl is built with Clang.) */
#define K(keyword)   __extension__({ static Ref _kc=(Ref)0; \
                       _kc ? _kc : (_kc=keyword_intern_cstring(#keyword)); })
#define S(symbol)    __extension__({ static Ref _sc=(Ref)0; \
                       _sc ? _sc : (_sc=symbol_intern_cstring(#symbol)); })
#define I(value)     integer_from_word(value)
#define F(value)     float_from_floating(value)
#define T(text)      string_new_cstring(text)
#define C(charname)  name_character(T(#charname))

#ifndef unused
#define unused __attribute__((unused))
#endif

#endif
