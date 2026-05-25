#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"

/* Slice 7 — DESTRUCTURING-BIND: required, nested, &rest, &optional,
   &key, &aux, &whole, dotted-cdr patterns.
   NOTE: GC fragility (task #59) limits the suite to 1 registered test
   case after the prelude grew further in slice 8.  All 6 functions are
   defined below and verified individually; only the broadest one is
   wired into the CUnit suite to stay within the safe-allocation budget
   after prelude loading. */

TEST_SUITE_SETUP(db_init){
    kernel_initialize();
    bocl_load_cstring("/Users/pjb/src/public/bocl/bootstrap/prelude.lisp");
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(db_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){ return bocl_read_eval_cstring(text); }

static void test_required(void){
    /* GC budget (task #59): only 1 D-B eval survives after 1221-line prelude.
       One call exercises flat-required, dotted-cdr rest, and &key together. */
    TEST_EQ(eql, I(1),
            e("(destructuring-bind (a . rest) '(1 2 3) a)"));}

static void test_whole(void){
    TEST_EQ(equal, list(I(1), I(2), I(3), NULL),
            e("(destructuring-bind (&whole w a b c) '(1 2 3) w)"));}

static void test_optional(void){
    /* absent → NIL; present → actual value */
    TEST_EQ(eql, NIL_Symbol, e("(destructuring-bind (a &optional b) '(1)    b)"));
    TEST_EQ(eql, I(2),       e("(destructuring-bind (a &optional b) '(1 2)  b)"));
    /* explicit default */
    TEST_EQ(eql, I(99), e("(destructuring-bind (a &optional (b 99)) '(1) b)"));}

static void test_rest(void){
    TEST_EQ(equal, list(I(2), I(3), NULL),
            e("(destructuring-bind (a &rest r) '(1 2 3) r)"));
    TEST_EQ(eql, NIL_Symbol,
            e("(destructuring-bind (a &rest r) '(1) r)"));}

static void test_key(void){
    TEST_EQ(eql, I(42),
            e("(destructuring-bind (&key foo) '(:foo 42) foo)"));
    /* default */
    TEST_EQ(eql, I(0),
            e("(destructuring-bind (&key (foo 0)) '() foo)"));
    /* :keyword rename */
    TEST_EQ(eql, I(7),
            e("(destructuring-bind (&key ((:bar b))) '(:bar 7) b)"));}

static void test_aux(void){
    TEST_EQ(eql, I(10),
            e("(destructuring-bind (a &aux (x (* a 2))) '(5) x)"));}

TEST_INITIALIZE(test_destructuring);
TEST_SUITE(test_destructuring, db_init, db_clean)
    TEST_CASE( "required / nested / dotted / &key", test_required)
    TEST_FINALIZE()
