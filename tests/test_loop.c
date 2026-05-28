/* tests/test_loop.c — CUnit tests for the LOOP macro (Slice 11) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(loop_init){
    kernel_initialize();
    bocl_load_cstring("/Users/pjb/src/public/bocl/bootstrap/prelude.lisp");
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(loop_fini){
    return CUE_SUCCESS;}

/* ---- helpers ---- */
static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- simple (keywordless) loop ---- */
static void test_simple_loop(void){
    TEST_EQ(equal, e("5"),
            e("(let ((n 0)) (loop (incf n) (when (= n 5) (return n))))"));}

/* ---- for … in ---- */
static void test_for_in_collect(void){
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(loop for x in '(1 2 3) collect x)"));
    TEST_EQ(equal, e("'(1 4 9)"),
            e("(loop for x in '(1 2 3) collect (* x x))"));
    TEST_EQ(equal, e("nil"),
            e("(loop for x in '() collect x)"));}

static void test_for_in_do(void){
    TEST_EQ(equal, e("10"),
            e("(let ((s 0)) (loop for x in '(1 2 3 4) do (incf s x)) s)"));}

/* ---- for … on ---- */
static void test_for_on(void){
    TEST_EQ(equal, e("'(a b c)"),
            e("(loop for x on '(a b c) collect (car x))"));
    TEST_EQ(equal, e("'((a b c) (b c) (c))"),
            e("(loop for x on '(a b c) collect x)"));}

/* ---- for … from/to/below/by ---- */
static void test_for_from_to(void){
    TEST_EQ(equal, e("'(1 2 3 4 5)"),
            e("(loop for i from 1 to 5 collect i)"));
    TEST_EQ(equal, e("'(0 1 2 3)"),
            e("(loop for i from 0 below 4 collect i)"));
    TEST_EQ(equal, e("'(10 9 8 7)"),
            e("(loop for i from 10 downto 7 collect i)"));
    TEST_EQ(equal, e("'(0 3 6 9)"),
            e("(loop for i from 0 to 10 by 3 collect i)"));
    TEST_EQ(equal, e("nil"),
            e("(loop for i from 0 below 0 collect i)"));}

/* ---- for … across ---- */
static void test_for_across(void){
    TEST_EQ(equal, e("'(#\\a #\\b #\\c)"),
            e("(loop for c across \"abc\" collect c)"));
    TEST_EQ(equal, e("'(10 20 30)"),
            e("(loop for x across #(10 20 30) collect x)"));
    TEST_EQ(equal, e("nil"),
            e("(loop for x across #() collect x)"));}

/* ---- for … = … then … ---- */
static void test_for_equals_then(void){
    TEST_EQ(equal, e("'(1 2 4 8 16)"),
            e("(loop for x = 1 then (* x 2) repeat 5 collect x)"));
    TEST_EQ(equal, e("'(0 1 2 3)"),
            e("(loop for x = 0 then (1+ x) while (< x 4) collect x)"));}

/* ---- repeat ---- */
static void test_repeat(void){
    TEST_EQ(equal, e("'(x x x)"),
            e("(loop repeat 3 collect 'x)"));
    TEST_EQ(equal, e("nil"),
            e("(loop repeat 0 collect 'x)"));}

/* ---- while / until ---- */
static void test_while_until(void){
    TEST_EQ(equal, e("5"),
            e("(let ((n 0)) (loop while (< n 5) do (incf n)) n)"));
    TEST_EQ(equal, e("3"),
            e("(let ((n 0)) (loop until (= n 3) do (incf n)) n)"));
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(loop for i from 1 while (< i 4) collect i)"));}

/* ---- sum / count / maximize / minimize ---- */
static void test_sum(void){
    TEST_EQ(equal, e("55"),
            e("(loop for i from 1 to 10 sum i)"));
    TEST_EQ(equal, e("10"),
            e("(loop for x in '(1 2 3 4) sum x)"));}

static void test_count(void){
    TEST_EQ(equal, e("3"),
            e("(loop for x in '(1 2 3 4 5) count (oddp x))"));
    TEST_EQ(equal, e("0"),
            e("(loop for x in '() count t)"));}

static void test_maximize_minimize(void){
    TEST_EQ(equal, e("9"),
            e("(loop for x in '(3 1 4 1 5 9 2 6) maximize x)"));
    TEST_EQ(equal, e("1"),
            e("(loop for x in '(3 1 4 1 5 9 2 6) minimize x)"));}

/* ---- always / never / thereis ---- */
static void test_always(void){
    TEST_EQ(equal, e("t"),
            e("(loop for x in '(2 4 6) always (evenp x))"));
    TEST_EQ(equal, e("nil"),
            e("(loop for x in '(2 3 6) always (evenp x))"));
    TEST_EQ(equal, e("t"),
            e("(loop for x in '() always nil)"));}   /* vacuously true */

static void test_never(void){
    TEST_EQ(equal, e("t"),
            e("(loop for x in '(1 3 5) never (evenp x))"));
    TEST_EQ(equal, e("nil"),
            e("(loop for x in '(1 2 5) never (evenp x))"));}

static void test_thereis(void){
    TEST_EQ(equal, e("t"),
            e("(loop for x in '(1 3 4) thereis (evenp x))"));
    TEST_EQ(equal, e("nil"),
            e("(loop for x in '(1 3 5) thereis (evenp x))"));}

/* ---- named / return / finally ---- */
static void test_named_return(void){
    TEST_EQ(equal, e("5"),
            e("(loop named outer for i from 1 to 10 when (= i 5) return i)"));
    TEST_EQ(equal, e("'done"),
            e("(loop for i from 1 to 3 collect i finally (return 'done))"));}

/* ---- with ---- */
static void test_with(void){
    TEST_EQ(equal, e("10"),
            e("(loop with acc = 0"
              "      for i from 1 to 4"
              "      do (incf acc i)"
              "      finally (return acc))"));}

/* ---- initially / finally ---- */
static void test_initially_finally(void){
    /* initially uses push so r stays a proper list; setq would make a dotted list */
    TEST_EQ(equal, e("'(ended 3 2 1 started)"),
            e("(let ((r nil))"
              "  (loop initially (push 'started r)"
              "        for i from 1 to 3"
              "        do (push i r)"
              "        finally (push 'ended r))"
              "  r)"));}

/* ---- parallel for clauses ---- */
static void test_parallel_for(void){
    TEST_EQ(equal, e("'((1 a) (2 b) (3 c))"),
            e("(loop for x in '(1 2 3)"
              "      for y in '(a b c)"
              "      collect (list x y))"));
    /* shorter list controls the length */
    TEST_EQ(equal, e("'((1 a) (2 b))"),
            e("(loop for x in '(1 2)"
              "      for y in '(a b c d)"
              "      collect (list x y))"));}

/* ---- append / nconc ---- */
static void test_append(void){
    TEST_EQ(equal, e("'(1 2 3 4 5)"),
            e("(loop for x in '((1 2) (3 4) (5)) append x)"));}

TEST_INITIALIZE(test_loop);
TEST_SUITE(test_loop, loop_init, loop_fini)
    TEST_CASE("simple loop",            test_simple_loop)
    TEST_CASE("for in collect",         test_for_in_collect)
    TEST_CASE("for in do",              test_for_in_do)
    TEST_CASE("for on",                 test_for_on)
    TEST_CASE("for from to",            test_for_from_to)
    TEST_CASE("for across",             test_for_across)
    TEST_CASE("for = then",             test_for_equals_then)
    TEST_CASE("repeat",                 test_repeat)
    TEST_CASE("while / until",          test_while_until)
    TEST_CASE("sum",                    test_sum)
    TEST_CASE("count",                  test_count)
    TEST_CASE("maximize / minimize",    test_maximize_minimize)
    TEST_CASE("always",                 test_always)
    TEST_CASE("never",                  test_never)
    TEST_CASE("thereis",                test_thereis)
    TEST_CASE("named / return",         test_named_return)
    TEST_CASE("with",                   test_with)
    TEST_CASE("initially / finally",    test_initially_finally)
    TEST_CASE("parallel for",           test_parallel_for)
    TEST_CASE("append",                 test_append)
    TEST_FINALIZE()
