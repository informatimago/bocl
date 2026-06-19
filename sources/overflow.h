#ifndef bocl_overflow_h
#define bocl_overflow_h
/* Portable overflow-checked arithmetic + popcount.
 *
 * bocl's fixnum/bignum code uses the GNU/Clang __builtin_*_overflow and
 * __builtin_popcount intrinsics, which excludes other C17 compilers (e.g. tcc).
 * gcc/clang keep their intrinsics; everyone else gets a correct hand-written
 * fallback, so bocl builds with any conforming C17 compiler.
 *
 * Shared by arithmetic.c (signed `word` ops) and kernel.c (unsigned `uword`
 * bignum-magnitude ops). */
#include <stdbool.h>
#include "kernel_types.h"   /* word, uword, WORD_MIN, WORD_MAX */

#if defined(__TINYC__)
#  define BOCL_HAVE_BUILTINS 0
#elif defined(__has_builtin)
#  if __has_builtin(__builtin_add_overflow) && __has_builtin(__builtin_sub_overflow) \
   && __has_builtin(__builtin_mul_overflow) && __has_builtin(__builtin_popcountll)
#    define BOCL_HAVE_BUILTINS 1
#  else
#    define BOCL_HAVE_BUILTINS 0
#  endif
#elif defined(__GNUC__) && (__GNUC__ >= 5)
#  define BOCL_HAVE_BUILTINS 1
#else
#  define BOCL_HAVE_BUILTINS 0
#endif

/* ---- signed word ---- */
static inline bool bocl_add_overflow(word a, word b, word* r){
#if BOCL_HAVE_BUILTINS
    return __builtin_add_overflow(a, b, r);
#else
    *r = (word)((uword)a + (uword)b);                 /* wrapped result, as the builtin stores */
    return ((b > 0) && (a > WORD_MAX - b)) || ((b < 0) && (a < WORD_MIN - b));
#endif
}

static inline bool bocl_sub_overflow(word a, word b, word* r){
#if BOCL_HAVE_BUILTINS
    return __builtin_sub_overflow(a, b, r);
#else
    *r = (word)((uword)a - (uword)b);
    return ((b < 0) && (a > WORD_MAX + b)) || ((b > 0) && (a < WORD_MIN + b));
#endif
}

static inline bool bocl_mul_overflow(word a, word b, word* r){
#if BOCL_HAVE_BUILTINS
    return __builtin_mul_overflow(a, b, r);
#else
    *r = (word)((uword)a * (uword)b);
    if(a == 0 || b == 0){ return false; }
    if(a == WORD_MIN || b == WORD_MIN){ return !(a == 1 || b == 1); }  /* |WORD_MIN| > WORD_MAX */
    if(a > 0){ return (b > 0) ? (a > WORD_MAX / b) : (b < WORD_MIN / a); }
    else     { return (b > 0) ? (a < WORD_MIN / b) : (a < WORD_MAX / b); }
#endif
}

/* ---- unsigned uword ---- */
static inline bool bocl_uadd_overflow(uword a, uword b, uword* r){
#if BOCL_HAVE_BUILTINS
    return __builtin_add_overflow(a, b, r);
#else
    *r = a + b;
    return *r < a;                                    /* unsigned wraparound */
#endif
}

static inline bool bocl_umul_overflow(uword a, uword b, uword* r){
#if BOCL_HAVE_BUILTINS
    return __builtin_mul_overflow(a, b, r);
#else
    *r = a * b;
    return a != 0 && *r / a != b;
#endif
}

/* Population count over a full uword (popcountll, not popcountl, so the high
   32 bits are counted when uword is 64-bit on a 32-bit-long target). */
static inline int bocl_popcount_uword(uword x){
#if BOCL_HAVE_BUILTINS
    return __builtin_popcountll((unsigned long long)x);
#else
    int c = 0; while(x){ c += (int)(x & 1u); x >>= 1; } return c;
#endif
}

#endif
