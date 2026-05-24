#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"

/* Phase 5.2 — FUNCALL, APPLY, MAPCAR, MAPC, EVERY, SOME, NOTANY, NOTEVERY. */

TEST_SUITE_SETUP(seq_init){
    kernel_initialize();
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(seq_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){ return bocl_read_eval_cstring(text); }

static void test_funcall(void){
    TEST_EQ(eql, I(1),  e("(funcall #'car (list 1 2 3))"));
    TEST_EQ(eql, I(49), e("(funcall (lambda (x) (* x x)) 7)"));
    TEST_EQ(eql, I(6),  e("(funcall #'+ 1 2 3)"));}

static void test_apply(void){
    TEST_EQ(eql, I(15), e("(apply #'+ 1 2 (list 3 4 5))"));
    TEST_EQ(eql, I(15), e("(apply #'+ (list 1 2 3 4 5))"));
    TEST_EQ(equal,
            list(I(3), I(2), I(1), NULL),
            e("(apply (lambda (a b c) (list c b a)) (list 1 2 3))"));}

static void test_mapcar_single(void){
    TEST_EQ(equal,
            list(I(1), I(4), I(9), I(16), NULL),
            e("(mapcar (lambda (x) (* x x)) (list 1 2 3 4))"));}

static void test_mapcar_multi(void){
    TEST_EQ(equal,
            list(I(11), I(22), I(33), NULL),
            e("(mapcar #'+ (list 1 2 3) (list 10 20 30))"));
    /* Stops at the shortest list. */
    TEST_EQ(equal,
            list(I(11), I(22), NULL),
            e("(mapcar #'+ (list 1 2 3 4) (list 10 20))"));}

static void test_mapc_returns_first(void){
    /* MAPC returns the first list, not the collected results. */
    TEST_EQ(equal,
            list(I(1), I(2), I(3), NULL),
            e("(mapc (lambda (x) x) (list 1 2 3))"));}

static void test_every(void){
    TEST_EQ(eql, T_Symbol,   e("(every #'integerp (list 1 2 3))"));
    TEST_EQ(eql, NIL_Symbol, e("(every #'integerp (list 1 2.0 3))"));
    TEST_EQ(eql, T_Symbol,   e("(every #'< (list 1 2 3) (list 2 3 4))"));}

static void test_some(void){
    TEST_EQ(eql, NIL_Symbol, e("(some #'floatp (list 1 2 3))"));
    /* SOME returns the actual non-NIL value, not just T. */
    Ref r = e("(some #'identity (list nil nil 7 nil))");
    TEST_EQ(eql, I(7), r);}

static void test_notany_notevery(void){
    TEST_EQ(eql, T_Symbol,   e("(notany  #'floatp   (list 1 2 3))"));
    TEST_EQ(eql, NIL_Symbol, e("(notany  #'integerp (list 1 2 3))"));
    TEST_EQ(eql, T_Symbol,   e("(notevery #'integerp (list 1 2.0 3))"));
    TEST_EQ(eql, NIL_Symbol, e("(notevery #'integerp (list 1 2 3))"));}

static void test_funcall_closure(void){
    /* FUNCALL on a closure preserves the captured environment. */
    e("(defun make-adder (n) (lambda (x) (+ x n)))");
    e("(defparameter add5 (make-adder 5))");
    TEST_EQ(eql, I(15), e("(funcall add5 10)"));
    TEST_EQ(eql, I(7),  e("(funcall add5 2)"));}

TEST_INITIALIZE(test_sequences);
TEST_SUITE(test_sequences, seq_init, seq_clean)
    TEST_CASE( "FUNCALL",              test_funcall)
    TEST_CASE( "APPLY",                test_apply)
    TEST_CASE( "MAPCAR single list",   test_mapcar_single)
    TEST_CASE( "MAPCAR multi-list",    test_mapcar_multi)
    TEST_CASE( "MAPC returns first",   test_mapc_returns_first)
    TEST_CASE( "EVERY",                test_every)
    TEST_CASE( "SOME",                 test_some)
    TEST_CASE( "NOTANY / NOTEVERY",    test_notany_notevery)
    TEST_CASE( "FUNCALL on closure",   test_funcall_closure)
    TEST_FINALIZE()
