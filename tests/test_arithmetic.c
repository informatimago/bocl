#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"

/* Phase 2 arithmetic tests. Each case round-trips a source string through
   bocl_read_eval_cstring and asserts the result. */

TEST_SUITE_SETUP(arith_init){
    kernel_initialize();
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(arith_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){
    return bocl_read_eval_cstring(text);}

/* ------------------------------------------------------------------ */
/*  Two-argument arithmetic.                                           */
/* ------------------------------------------------------------------ */

static void test_plus_int(void){
    TEST_EQ(eql, I(5),    e("(+ 2 3)"));
    TEST_EQ(eql, I(-1),   e("(+ 2 -3)"));
    TEST_EQ(eql, I(0),    e("(+ 0 0)"));}

static void test_minus_int(void){
    TEST_EQ(eql, I(3),    e("(- 10 7)"));
    TEST_EQ(eql, I(-7),   e("(- 0 7)"));}

static void test_times_int(void){
    TEST_EQ(eql, I(20),   e("(* 4 5)"));
    TEST_EQ(eql, I(0),    e("(* 0 99)"));
    TEST_EQ(eql, I(-6),   e("(* 2 -3)"));}

static void test_div_int_exact(void){
    TEST_EQ(eql, I(5),    e("(/ 20 4)"));
    TEST_EQ(eql, I(-5),   e("(/ -20 4)"));}

static void test_div_int_to_float(void){
    /* Inexact integer / promotes to float (no ratios yet in the kernel). */
    TEST_EQ(float_eq, F(2.5), e("(/ 5 2)"));}

static void test_div_float(void){
    TEST_EQ(float_eq, F(2.5), e("(/ 5.0 2.0)"));}

static void test_mixed_int_float(void){
    /* Mixed int + float promotes to float. */
    TEST_EQ(float_eq, F(5.5), e("(+ 2 3.5)"));
    TEST_EQ(float_eq, F(5.5), e("(+ 3.5 2)"));}

/* ------------------------------------------------------------------ */
/*  Comparisons.                                                       */
/* ------------------------------------------------------------------ */

static void test_num_eq(void){
    TEST_EQ(eql, T_Symbol,   e("(= 3 3)"));
    TEST_EQ(eql, NIL_Symbol, e("(= 3 4)"));
    /* Numeric equality is cross-type: (= 1 1.0) => T. */
    TEST_EQ(eql, T_Symbol,   e("(= 1 1.0)"));}

static void test_num_neq(void){
    TEST_EQ(eql, NIL_Symbol, e("(/= 3 3)"));
    TEST_EQ(eql, T_Symbol,   e("(/= 3 4)"));}

static void test_lt_gt_le_ge(void){
    TEST_EQ(eql, T_Symbol,   e("(<  1 2)"));
    TEST_EQ(eql, NIL_Symbol, e("(<  2 1)"));
    TEST_EQ(eql, NIL_Symbol, e("(<  2 2)"));
    TEST_EQ(eql, T_Symbol,   e("(>  3 1)"));
    TEST_EQ(eql, NIL_Symbol, e("(>  1 3)"));
    TEST_EQ(eql, T_Symbol,   e("(<= 2 2)"));
    TEST_EQ(eql, T_Symbol,   e("(<= 1 2)"));
    TEST_EQ(eql, NIL_Symbol, e("(<= 3 2)"));
    TEST_EQ(eql, T_Symbol,   e("(>= 2 2)"));}

/* ------------------------------------------------------------------ */
/*  Predicates.                                                        */
/* ------------------------------------------------------------------ */

static void test_predicates(void){
    TEST_EQ(eql, T_Symbol,   e("(zerop 0)"));
    TEST_EQ(eql, T_Symbol,   e("(zerop 0.0)"));
    TEST_EQ(eql, NIL_Symbol, e("(zerop 1)"));
    TEST_EQ(eql, T_Symbol,   e("(plusp 3)"));
    TEST_EQ(eql, NIL_Symbol, e("(plusp -3)"));
    TEST_EQ(eql, NIL_Symbol, e("(plusp 0)"));
    TEST_EQ(eql, T_Symbol,   e("(minusp -2)"));
    TEST_EQ(eql, T_Symbol,   e("(oddp 7)"));
    TEST_EQ(eql, NIL_Symbol, e("(oddp 8)"));
    TEST_EQ(eql, T_Symbol,   e("(evenp 8)"));
    TEST_EQ(eql, NIL_Symbol, e("(evenp 7)"));}

/* ------------------------------------------------------------------ */
/*  1+ / 1- / ABS / MIN / MAX.                                         */
/* ------------------------------------------------------------------ */

static void test_inc_dec(void){
    TEST_EQ(eql, I(42),  e("(1+ 41)"));
    TEST_EQ(eql, I(40),  e("(1- 41)"));
    TEST_EQ(float_eq, F(3.5), e("(1+ 2.5)"));}

static void test_abs(void){
    TEST_EQ(eql, I(7),   e("(abs 7)"));
    TEST_EQ(eql, I(7),   e("(abs -7)"));
    TEST_EQ(eql, I(0),   e("(abs 0)"));
    TEST_EQ(float_eq, F(2.5), e("(abs -2.5)"));}

static void test_min_max(void){
    TEST_EQ(eql, I(1),   e("(min 1 2)"));
    TEST_EQ(eql, I(2),   e("(max 1 2)"));
    TEST_EQ(eql, I(-3),  e("(min 1 -3)"));}

/* ------------------------------------------------------------------ */
/*  Integer division family.                                           */
/* ------------------------------------------------------------------ */

static void test_truncate(void){
    TEST_EQ(eql, I(3),   e("(truncate 17 5)"));
    TEST_EQ(eql, I(-3),  e("(truncate -17 5)"));
    TEST_EQ(eql, I(7),   e("(truncate 7)"));}

static void test_floor(void){
    TEST_EQ(eql, I(3),   e("(floor 17 5)"));
    /* Floor of negative rounds toward -inf, so (floor -17 5) => -4. */
    TEST_EQ(eql, I(-4),  e("(floor -17 5)"));}

static void test_ceiling(void){
    TEST_EQ(eql, I(4),   e("(ceiling 17 5)"));
    TEST_EQ(eql, I(-3),  e("(ceiling -17 5)"));}

static void test_round_ties_to_even(void){
    /* Banker's rounding: ties go to the even quotient. */
    TEST_EQ(eql, I(2),   e("(round 5 2)"));   /* 2.5 -> 2 (even)  */
    TEST_EQ(eql, I(2),   e("(round 3 2)"));   /* 1.5 -> 2 (even)  */
    TEST_EQ(eql, I(4),   e("(round 9 2)"));   /* 4.5 -> 4 (even)  */
    TEST_EQ(eql, I(4),   e("(round 7 2)"));}  /* 3.5 -> 4 (even)  */

static void test_mod_rem(void){
    TEST_EQ(eql, I(2),   e("(mod 17 5)"));
    TEST_EQ(eql, I(2),   e("(rem 17 5)"));
    /* MOD takes the sign of the divisor; REM takes the sign of the dividend. */
    TEST_EQ(eql, I(3),   e("(mod -17 5)"));   /* -17 = -4*5 + 3 */
    TEST_EQ(eql, I(-2),  e("(rem -17 5)"));}  /* -17 = -3*5 + (-2) */

/* ------------------------------------------------------------------ */
/*  Bitwise.                                                           */
/* ------------------------------------------------------------------ */

static void test_bitwise(void){
    TEST_EQ(eql, I(0x08), e("(logand 12 10)"));   /* 1100 & 1010 = 1000 */
    TEST_EQ(eql, I(0x0E), e("(logor  12 10)"));   /* 1100 | 1010 = 1110 */
    TEST_EQ(eql, I(0x06), e("(logxor 12 10)"));   /* 1100 ^ 1010 = 0110 */
    TEST_EQ(eql, I(-13),  e("(lognot 12)"));      /* ~12 = -13 */
    TEST_EQ(eql, I(8),    e("(ash 1 3)"));        /*  1 << 3 = 8     */
    TEST_EQ(eql, I(1),    e("(ash 8 -3)"));       /*  8 >> 3 = 1     */
    TEST_EQ(eql, I(0),    e("(ash 0 5)"));
    TEST_EQ(eql, I(4),    e("(integer-length 15)"));  /* 1111 has 4 bits        */
    TEST_EQ(eql, I(0),    e("(integer-length 0)"));
    /* For negative n, CL's integer-length is (ceiling (log (- n) 2)).
       -129 -> ceil(log2(129)) = 8. -128 -> ceil(log2(128)) = 7. */
    TEST_EQ(eql, I(8),    e("(integer-length -129)"));
    TEST_EQ(eql, I(7),    e("(integer-length -128)"));}

/* ------------------------------------------------------------------ */
/*  Coercion.                                                          */
/* ------------------------------------------------------------------ */

static void test_float_coercion(void){
    TEST_EQ(float_eq, F(7.0), e("(float 7)"));
    TEST_EQ(float_eq, F(2.5), e("(float 2.5)"));}

static void test_integer_coercion(void){
    TEST_EQ(eql, I(2),   e("(integer 2.5)"));   /* truncation */
    TEST_EQ(eql, I(-2),  e("(integer -2.5)"));}

static void test_coerce(void){
    TEST_EQ(float_eq, F(3.0), e("(coerce 3 'float)"));
    TEST_EQ(eql, I(7),       e("(coerce 7.0 'integer)"));}

/* ------------------------------------------------------------------ */
/*  Functions over arithmetic — exercising the full Phase 1 + 2 path.  */
/* ------------------------------------------------------------------ */

static void test_function_squares(void){
    e("(defun sq (n) (* n n))");
    TEST_EQ(eql, I(16),  e("(sq 4)"));
    TEST_EQ(eql, I(144), e("(sq 12)"));
    TEST_EQ(eql, I(0),   e("(sq 0)"));}

static void test_function_factorial(void){
    e("(defun fct (n) (if (= n 0) 1 (* n (fct (1- n)))))");
    TEST_EQ(eql, I(1),   e("(fct 0)"));
    TEST_EQ(eql, I(120), e("(fct 5)"));
    TEST_EQ(eql, I(720), e("(fct 6)"));}

/* ------------------------------------------------------------------ */
/*  Variadic arity.                                                     */
/* ------------------------------------------------------------------ */

static void test_arith_variadic(void){
    TEST_EQ(eql, I(15),          e("(+ 1 2 3 4 5)"));
    TEST_EQ(eql, I(120),         e("(* 1 2 3 4 5)"));
    TEST_EQ(eql, I(84),          e("(- 100 10 5 1)"));
    TEST_EQ(eql, I(5),           e("(/ 100 5 4)"));
    /* Identities. */
    TEST_EQ(eql, I(0),           e("(+)"));
    TEST_EQ(eql, I(1),           e("(*)"));
    /* Unary forms. */
    TEST_EQ(eql, I(-5),          e("(- 5)"));
    TEST_EQ(float_eq, F(0.25),   e("(/ 4)"));
    /* The user's original failing form. */
    TEST_EQ(eql, I(1000000000000L), e("(* 10000 10000 10000)"));}

static void test_cmp_variadic(void){
    TEST_EQ(eql, T_Symbol,   e("(< 1 2 3 4)"));
    TEST_EQ(eql, NIL_Symbol, e("(< 1 2 2 4)"));
    TEST_EQ(eql, T_Symbol,   e("(<= 1 2 2 4)"));
    TEST_EQ(eql, T_Symbol,   e("(= 5 5 5)"));
    TEST_EQ(eql, T_Symbol,   e("(/= 1 2 3)"));
    TEST_EQ(eql, NIL_Symbol, e("(/= 1 2 1)"));
    TEST_EQ(eql, T_Symbol,   e("(> 4 3 2 1)"));
    TEST_EQ(eql, T_Symbol,   e("(>= 4 4 3 2)"));}

static void test_minmax_variadic(void){
    TEST_EQ(eql, I(1),   e("(min 3 1 4 1 5 9 2 6)"));
    TEST_EQ(eql, I(9),   e("(max 3 1 4 1 5 9 2 6)"));}

static void test_fixnum_constants(void){
    /* Kernel has no fixnum/bignum split — most-positive-fixnum is WORD_MAX
       and most-negative-fixnum is WORD_MIN. We can only assert easy invariants
       (a full one like 1+ MOST-POSITIVE-FIXNUM = -MOST-NEGATIVE-FIXNUM blows
       up since the right-hand side overflows). */
    TEST_EQ(eql, T_Symbol, e("(plusp most-positive-fixnum)"));
    TEST_EQ(eql, T_Symbol, e("(minusp most-negative-fixnum)"));
    TEST_EQ(eql, T_Symbol, e("(< most-negative-fixnum 0 most-positive-fixnum)"));}

static void test_impl_version(void){
    /* (lisp-implementation-type) returns a non-empty string. */
    TEST_EQ(eql, T_Symbol, e("(stringp (lisp-implementation-type))"));
    TEST_EQ(eql, T_Symbol, e("(stringp (lisp-implementation-version))"));}

TEST_INITIALIZE(test_arithmetic);
TEST_SUITE(test_arithmetic, arith_init, arith_clean)
    TEST_CASE( "+ integers",          test_plus_int)
    TEST_CASE( "- integers",          test_minus_int)
    TEST_CASE( "* integers",          test_times_int)
    TEST_CASE( "/ exact integer",     test_div_int_exact)
    TEST_CASE( "/ int promotes",      test_div_int_to_float)
    TEST_CASE( "/ floats",            test_div_float)
    TEST_CASE( "mixed int+float",     test_mixed_int_float)
    TEST_CASE( "= equality",          test_num_eq)
    TEST_CASE( "/=",                  test_num_neq)
    TEST_CASE( "< > <= >=",           test_lt_gt_le_ge)
    TEST_CASE( "predicates",          test_predicates)
    TEST_CASE( "1+ / 1-",             test_inc_dec)
    TEST_CASE( "abs",                 test_abs)
    TEST_CASE( "min / max",           test_min_max)
    TEST_CASE( "truncate",            test_truncate)
    TEST_CASE( "floor",               test_floor)
    TEST_CASE( "ceiling",             test_ceiling)
    TEST_CASE( "round (banker's)",    test_round_ties_to_even)
    TEST_CASE( "mod / rem",           test_mod_rem)
    TEST_CASE( "bitwise",             test_bitwise)
    TEST_CASE( "float coercion",      test_float_coercion)
    TEST_CASE( "integer coercion",    test_integer_coercion)
    TEST_CASE( "coerce",              test_coerce)
    TEST_CASE( "user fn: squares",    test_function_squares)
    TEST_CASE( "user fn: factorial",  test_function_factorial)
    TEST_CASE( "variadic + - * /",    test_arith_variadic)
    TEST_CASE( "variadic comparisons",test_cmp_variadic)
    TEST_CASE( "variadic min/max",    test_minmax_variadic)
    TEST_CASE( "fixnum constants",    test_fixnum_constants)
    TEST_CASE( "lisp-impl-type/ver",  test_impl_version)
    TEST_FINALIZE()
