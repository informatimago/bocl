/* tests/test_slice33.c — CUnit tests for Slice 34 RATIO and COMPLEX:
   ratio reading "1/3", arithmetic, reduction, NUMERATOR/DENOMINATOR,
   complex reading "#C(1 2)", arithmetic, REALPART/IMAGPART, type predicates. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s33_init){
    kernel_initialize();
    bocl_load_cstring("/Users/pjb/src/public/bocl/bootstrap/prelude.lisp");
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s33_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- RATIO basics -------------------------------------------------------- */

static void test_ratio_read(void){
    TEST_EQ(eql, T_Symbol, e("(if (ratiop 1/3) t nil)"));
    TEST_EQ(eql, NIL_Symbol, e("(if (ratiop 5) t nil)"));
    TEST_EQ(equal, e("\"1/3\""), e("(write-to-string 1/3)"));
    TEST_EQ(eql, I(2), e("(numerator 2/3)"));
    TEST_EQ(eql, I(3), e("(denominator 2/3)"));}

static void test_ratio_reduce(void){
    /* 2/4 reduces to 1/2 */
    TEST_EQ(equal, e("\"1/2\""), e("(write-to-string 2/4)"));
    /* 6/3 reduces to the integer 2 */
    TEST_EQ(eql, T_Symbol, e("(if (integerp 6/3) t nil)"));
    TEST_EQ(eql, I(2), e("6/3"));}

static void test_ratio_arith(void){
    TEST_EQ(equal, e("\"5/6\""), e("(write-to-string (+ 1/2 1/3))"));
    TEST_EQ(equal, e("\"1/6\""), e("(write-to-string (- 1/2 1/3))"));
    TEST_EQ(equal, e("\"1/3\""), e("(write-to-string (* 1/2 2/3))"));
    TEST_EQ(equal, e("\"3/2\""), e("(write-to-string (/ 1/2 1/3))"));
    /* exact integer division yields a ratio */
    TEST_EQ(equal, e("\"1/3\""), e("(write-to-string (/ 1 3))"));
    /* (/ 3 1) is the integer 3 */
    TEST_EQ(eql, T_Symbol, e("(if (integerp (/ 3 1)) t nil)"));
    /* float contagion: ratio + float => float */
    TEST_EQ(eql, T_Symbol, e("(if (floatp (+ 1.0 1/2)) t nil)"));}

static void test_ratio_compare(void){
    TEST_EQ(eql, T_Symbol, e("(if (< 1/3 1/2) t nil)"));
    TEST_EQ(eql, T_Symbol, e("(if (> 2/3 1/2) t nil)"));
    TEST_EQ(eql, T_Symbol, e("(if (= 1/2 2/4) t nil)"));}

/* ---- COMPLEX basics ------------------------------------------------------ */

static void test_complex_read(void){
    TEST_EQ(eql, T_Symbol, e("(if (complexp #C(1 2)) t nil)"));
    TEST_EQ(eql, I(3), e("(realpart #C(3 4))"));
    TEST_EQ(eql, I(4), e("(imagpart #C(3 4))"));
    TEST_EQ(equal, e("\"#C(1 2)\""), e("(write-to-string (complex 1 2))"));}

static void test_complex_arith(void){
    /* (1+2i) + (3+4i) = 4+6i */
    TEST_EQ(equal, e("\"#C(4 6)\""), e("(write-to-string (+ #C(1 2) #C(3 4)))"));
    /* (1+2i)(3-2i) = 3-2i+6i-4i^2 = 3+4i+4 = 7+4i */
    TEST_EQ(eql, I(7), e("(realpart (* #C(1 2) #C(3 -2)))"));
    TEST_EQ(eql, I(4), e("(imagpart (* #C(1 2) #C(3 -2)))"));
    /* (complex 5 0) collapses to the real 5 */
    TEST_EQ(eql, T_Symbol, e("(if (integerp (complex 5 0)) t nil)"));}

/* ---- Type predicates ----------------------------------------------------- */

static void test_type_predicates(void){
    TEST_EQ(eql, T_Symbol, e("(if (rationalp 1/3) t nil)"));
    TEST_EQ(eql, T_Symbol, e("(if (rationalp 5) t nil)"));
    TEST_EQ(eql, NIL_Symbol, e("(if (rationalp 1.5) t nil)"));
    TEST_EQ(eql, T_Symbol, e("(if (numberp #C(1 2)) t nil)"));
    TEST_EQ(eql, T_Symbol, e("(if (typep 1/3 'ratio) t nil)"));
    TEST_EQ(eql, T_Symbol, e("(if (typep #C(1 2) 'complex) t nil)"));
    TEST_EQ(eql, T_Symbol, e("(if (typep 1/3 'rational) t nil)"));}


TEST_INITIALIZE(test_slice33);
TEST_SUITE(test_slice33, s33_init, s33_fini)
    TEST_CASE("ratio read/predicate",       test_ratio_read)
    TEST_CASE("ratio reduction",            test_ratio_reduce)
    TEST_CASE("ratio arithmetic",           test_ratio_arith)
    TEST_CASE("ratio comparison",           test_ratio_compare)
    TEST_CASE("complex read/predicate",     test_complex_read)
    TEST_CASE("complex arithmetic",         test_complex_arith)
    TEST_CASE("rational/number predicates", test_type_predicates)
    TEST_FINALIZE()
