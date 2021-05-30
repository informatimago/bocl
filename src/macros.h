#ifndef macros_h
#define macros_h

#define CX(a,b) a ## _ ## b
#define C(a,b) CX(a,b)

#define K(keyword) keyword_intern_cstring(#keyword)
#define S(symbol)  symbol_intern_cstring(#symbol)
#define I(value)   integer_from_word(value)
#define F(value)   float_from_floating(value)

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#endif
