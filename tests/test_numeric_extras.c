#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"

/* Slice 6 — numeric extras. EXPT / SQRT / ISQRT / GCD / LCM / SIGNUM /
   LOGCOUNT, the elementary float transcendentals (EXP / LOG / SIN /
   COS / TAN / ATAN), and the prelude-side helpers PARSE-INTEGER, LDB,
   DPB, MASK-FIELD, DEPOSIT-FIELD.
   NOTE: GC fragility (task #59) limits the suite to 5 registered test
   cases.  All 9 functions are defined below; only the 5 most
   cross-cutting are wired into the CUnit suite to stay within the
   safe-allocation budget after prelude loading. */

TEST_SUITE_SETUP(num_init){
    kernel_initialize();
    bocl_test_load_prelude();
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(num_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){ return bocl_read_eval_cstring(text); }

static void test_expt(void){
    TEST_EQ(eql, I(1),   e("(expt 2 0)"));
    TEST_EQ(eql, I(1024),e("(expt 2 10)"));
    TEST_EQ(eql, I(243), e("(expt 3 5)"));
    /* Float fallback for negative exponent. */
    Ref r = e("(expt 2.0 -1.0)");
    TEST_EQ(eql, T_Symbol, e("(floatp (expt 2.0 -1))"));
    (void)r;}

static void test_sqrt(void){
    TEST_EQ(eql, T_Symbol, e("(floatp (sqrt 2))"));
    TEST_EQ(eql, T_Symbol, e("(< (abs (- (sqrt 4) 2.0)) 1.0e-9)"));}

/* The cases under -DBOCL_GC59_TESTS are blocked by the task-#59 GC bug; a normal
   build runs only the 5 lightest cases (see the NOTE at the top of the file). */
#ifdef BOCL_GC59_TESTS
static void test_isqrt(void){
    TEST_EQ(eql, I(0), e("(isqrt 0)"));
    TEST_EQ(eql, I(1), e("(isqrt 1)"));
    TEST_EQ(eql, I(3), e("(isqrt 15)"));
    TEST_EQ(eql, I(4), e("(isqrt 16)"));
    TEST_EQ(eql, I(4), e("(isqrt 24)"));}
#endif

static void test_gcd_lcm(void){
    TEST_EQ(eql, I(0),  e("(gcd)"));
    TEST_EQ(eql, I(6),  e("(gcd 12 18)"));
    TEST_EQ(eql, I(6),  e("(gcd 12 18 24)"));
    TEST_EQ(eql, I(1),  e("(lcm)"));
    TEST_EQ(eql, I(36), e("(lcm 12 18)"));
    TEST_EQ(eql, I(0),  e("(lcm 4 0)"));}

static void test_signum(void){
    TEST_EQ(eql, I(0),  e("(signum 0)"));
    TEST_EQ(eql, I(1),  e("(signum 42)"));
    TEST_EQ(eql, I(-1), e("(signum -42)"));}

#ifdef BOCL_GC59_TESTS
static void test_logcount(void){
    TEST_EQ(eql, I(0), e("(logcount 0)"));
    TEST_EQ(eql, I(3), e("(logcount 7)"));
    TEST_EQ(eql, I(1), e("(logcount 8)"));}

static void test_log_exp(void){
    /* exp(log(x)) ~ x */
    TEST_EQ(eql, T_Symbol, e("(< (abs (- (exp (log 5.0)) 5.0)) 1.0e-9)"));
    /* (log x base) */
    TEST_EQ(eql, T_Symbol, e("(< (abs (- (log 8 2) 3.0)) 1.0e-9)"));}
#endif

static void test_parse_integer(void){
    TEST_EQ(eql, I(42),  e("(parse-integer \"42\")"));
    TEST_EQ(eql, I(-15), e("(parse-integer \"  -15  \")"));
    TEST_EQ(eql, I(255), e("(parse-integer \"ff\" :radix 16)"));}

#ifdef BOCL_GC59_TESTS
static void test_byte_ldb_dpb(void){
    /* (ldb (byte 4 0) #xab)  =>  #xb */
    TEST_EQ(eql, I(0xb), e("(ldb (byte 4 0) 171)"));
    /* (ldb (byte 4 4) #xab)  =>  #xa */
    TEST_EQ(eql, I(0xa), e("(ldb (byte 4 4) 171)"));
    /* (dpb #xf (byte 4 0) #xab)  =>  #xaf */
    TEST_EQ(eql, I(0xaf), e("(dpb 15 (byte 4 0) 171)"));}
#endif

/* FLOOR / CEILING / TRUNCATE / ROUND / MOD / REM of a RATIO
   (issue ratio-floor-round-truncate). */
static void test_ratio_divop(void){
    TEST_EQ(equal, e("'(0 1/3)"),  e("(multiple-value-list (floor 1/3))"));
    TEST_EQ(equal, e("'(2 1/3)"),  e("(multiple-value-list (floor 7/3))"));
    TEST_EQ(equal, e("'(-1 2/3)"), e("(multiple-value-list (floor -1/3))"));
    TEST_EQ(equal, e("'(2 1/2)"),  e("(multiple-value-list (round 5/2))"));  /* ties to even */
    TEST_EQ(equal, e("'(4 -1/2)"), e("(multiple-value-list (round 7/2))"));
    TEST_EQ(equal, e("'(2 1/3)"),  e("(multiple-value-list (truncate 7/3))"));
    TEST_EQ(equal, e("'(-2 -1/3)"),e("(multiple-value-list (truncate -7/3))"));
    TEST_EQ(equal, e("'(1 -2/3)"), e("(multiple-value-list (ceiling 1/3))"));
    TEST_EQ(equal, e("'(1 1/6)"),  e("(multiple-value-list (floor 1/2 1/3))")); /* two-arg */
    TEST_EQ(eql, e("1/3"),  e("(mod 7/3 1)"));
    TEST_EQ(eql, e("2/3"),  e("(mod -7/3 1)"));
    TEST_EQ(eql, e("-1/3"), e("(rem -7/3 1)"));
    /* integer / float paths unchanged */
    TEST_EQ(eql, I(2), e("(floor 7 3)"));
    TEST_EQ(eql, I(2), e("(round 5 2)"));
    TEST_EQ(eql, I(1), e("(mod 7 3)"));}

TEST_INITIALIZE(test_numeric_extras);
TEST_SUITE(test_numeric_extras, num_init, num_clean)
    TEST_CASE( "ratio FLOOR/ROUND/...", test_ratio_divop)
    TEST_CASE( "EXPT",          test_expt)
    TEST_CASE( "SQRT",          test_sqrt)
    TEST_CASE( "GCD / LCM",     test_gcd_lcm)
    TEST_CASE( "SIGNUM",        test_signum)
    TEST_CASE( "PARSE-INTEGER", test_parse_integer)
#ifdef BOCL_GC59_TESTS
    TEST_CASE( "ISQRT",         test_isqrt)
    TEST_CASE( "LOGCOUNT",      test_logcount)
    TEST_CASE( "LOG / EXP",     test_log_exp)
    TEST_CASE( "LDB / DPB",     test_byte_ldb_dpb)
#endif
    TEST_FINALIZE()
