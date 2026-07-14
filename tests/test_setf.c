#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

/* Slice 2 — SETF tests. Loads bootstrap/prelude.lisp first (which defines
   SETF as a macro over the kernel built-ins), then exercises each shipped
   place expander plus the PUSH/POP/INCF/DECF macros that depend on it. */

TEST_SUITE_SETUP(setf_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);   /* expected symbols are CL (symbol_intern_cstring) */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(setf_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){ return bocl_test_eval(text); }

static void test_setf_symbol(void){
    e("(setf ts-x 7)");
    TEST_EQ(eql, I(7), e("ts-x"));}

static void test_setf_car_cdr(void){
    e("(setf ts-p (cons 1 2))");
    e("(setf (car ts-p) 10)");
    e("(setf (cdr ts-p) 20)");
    TEST_EQ(equal, cons(I(10), I(20)), e("ts-p"));}

static void test_setf_gethash(void){
    e("(setf ts-h (make-hash-table :test 'equal))");
    e("(setf (gethash \"k\" ts-h) 42)");
    TEST_EQ(eql, I(42), e("(gethash \"k\" ts-h)"));}

static void test_setf_nth(void){
    e("(setf ts-l (list 10 20 30 40))");
    e("(setf (nth 2 ts-l) 999)");
    TEST_EQ(equal, list(I(10), I(20), I(999), I(40), NULL), e("ts-l"));}

static void test_setf_svref(void){
    e("(setf ts-v (vector 'a 'b 'c))");
    e("(setf (svref ts-v 1) 'mid)");
    TEST_EQ(eql, S(MID), e("(svref ts-v 1)"));}

static void test_setf_multiple_pairs(void){
    e("(setf ts-a 1 ts-b 2 ts-c 3)");
    TEST_EQ(equal, list(I(1), I(2), I(3), NULL), e("(list ts-a ts-b ts-c)"));}

static void test_push_on_generic_place(void){
    e("(setf ts-p2 (cons nil 'tail))");
    e("(push 'x (car ts-p2))");
    e("(push 'y (car ts-p2))");
    TEST_EQ(equal, list(S(Y), S(X), NULL), e("(car ts-p2)"));}

static void test_pop_on_generic_place(void){
    /* TEST_EQ evaluates its `expression` argument twice (for the assert
       message). So we MUST capture side-effecting forms (POP) into a
       local before calling TEST_EQ — otherwise pop runs twice and ts-p3
       loses two elements. */
    e("(setf ts-p3 (cons (list 'a 'b 'c) nil))");
    Ref popped = e("(pop (car ts-p3))");
    TEST_EQ(eql, S(A), popped);
    Ref tail = e("(car ts-p3)");
    TEST_EQ(equal, list(S(B), S(C), NULL), tail);}

static void test_incf_decf_on_hash_place(void){
    e("(setf ts-h2 (make-hash-table))");
    e("(setf (gethash 'k ts-h2) 0)");
    e("(incf (gethash 'k ts-h2))");
    e("(incf (gethash 'k ts-h2) 10)");
    e("(decf (gethash 'k ts-h2) 1)");
    TEST_EQ(eql, I(10), e("(gethash 'k ts-h2)"));}

static void test_setf_symbol_function(void){
    e("(defun ts-original () :original)");
    e("(setf (symbol-function 'ts-original) (lambda () :replaced))");
    TEST_EQ(eql, K(REPLACED), e("(ts-original)"));}

TEST_INITIALIZE(test_setf);
TEST_SUITE(test_setf, setf_init, setf_clean)
    TEST_CASE( "setf symbol",              test_setf_symbol)
    TEST_CASE( "setf car / cdr",           test_setf_car_cdr)
    TEST_CASE( "setf gethash",             test_setf_gethash)
    TEST_CASE( "setf nth",                 test_setf_nth)
    TEST_CASE( "setf svref",               test_setf_svref)
    TEST_CASE( "setf multiple pairs",      test_setf_multiple_pairs)
    TEST_CASE( "push on (car ...)",        test_push_on_generic_place)
    TEST_CASE( "pop on (car ...)",         test_pop_on_generic_place)
    TEST_CASE( "incf/decf on gethash",     test_incf_decf_on_hash_place)
    TEST_CASE( "setf symbol-function",     test_setf_symbol_function)
    TEST_FINALIZE()
