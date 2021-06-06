#ifndef macros_h
#define macros_h
#include "kernel.h"

#define CX(a,b) a ## _ ## b
#define CS(a,b) CX(a,b)

#define K(keyword)   keyword_intern_cstring(#keyword)
#define S(symbol)    symbol_intern_cstring(#symbol)
#define I(value)     integer_from_word(value)
#define F(value)     float_from_floating(value)
#define T(text)      string_new_cstring(text)
#define C(charname)  name_character(T(#charname))

#ifndef unused
#define unused __attribute__((unused))
#endif

#endif
