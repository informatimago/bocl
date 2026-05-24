#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"
#include "eval.h"

/* Phase 1 special-operator + apply tests. Each case round-trips a source
   string through bocl_read_eval_cstring and asserts the result. */

TEST_SUITE_SETUP(eval_init){
    kernel_initialize();
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(eval_clean){
    return CUE_SUCCESS;}


static Ref eval_cstring(const char* text){
    return bocl_read_eval_cstring(text);}

static void test_quote(void){
    TEST_EQ(eql,    I(42),       eval_cstring("(quote 42)"));
    TEST_EQ(eql,    S(FOO),      eval_cstring("(quote foo)"));
    TEST_EQ(equal,  list(S(A),S(B),NULL),
                                 eval_cstring("(quote (a b))"));}

static void test_self_evaluating(void){
    TEST_EQ(eql,    I(7),        eval_cstring("7"));
    TEST_EQ(eql,    NIL_Symbol,  eval_cstring("nil"));
    TEST_EQ(eql,    T_Symbol,    eval_cstring("t"));}

static void test_if(void){
    TEST_EQ(eql,    I(1),        eval_cstring("(if t 1 2)"));
    TEST_EQ(eql,    I(2),        eval_cstring("(if nil 1 2)"));
    TEST_EQ(eql,    NIL_Symbol,  eval_cstring("(if nil 1)"));}

static void test_progn(void){
    TEST_EQ(eql,    I(3),        eval_cstring("(progn 1 2 3)"));
    TEST_EQ(eql,    NIL_Symbol,  eval_cstring("(progn)"));}

static void test_setq_global(void){
    TEST_EQ(eql,    I(7),        eval_cstring("(setq tq-x 7)"));
    TEST_EQ(eql,    I(7),        eval_cstring("tq-x"));
    TEST_EQ(eql,    I(99),       eval_cstring("(setq tq-x 99)"));
    TEST_EQ(eql,    I(99),       eval_cstring("tq-x"));}

static void test_setq_multiple_pairs(void){
    /* (setq a 1 b 2 c 3) → 3, with all three globals set. */
    TEST_EQ(eql,    I(3),        eval_cstring("(setq tq-a 1 tq-b 2 tq-c 3)"));
    TEST_EQ(eql,    I(1),        eval_cstring("tq-a"));
    TEST_EQ(eql,    I(2),        eval_cstring("tq-b"));
    TEST_EQ(eql,    I(3),        eval_cstring("tq-c"));}

static void test_let_lexical(void){
    TEST_EQ(eql,    I(11),       eval_cstring("(let ((x 11)) x)"));
    TEST_EQ(eql,    I(2),        eval_cstring("(let ((x 1) (y 2)) y)"));}

static void test_defparameter(void){
    TEST_EQ(eql,    S(*MYVAR*),  eval_cstring("(defparameter *myvar* 42)"));
    TEST_EQ(eql,    I(42),       eval_cstring("*myvar*"));}

static void test_defun_identity(void){
    TEST_EQ(eql,    S(MY-ID),    eval_cstring("(defun my-id (n) n)"));
    TEST_EQ(eql,    I(5),        eval_cstring("(my-id 5)"));}

static void test_defun_let_body(void){
    /* Phase 1 milestone form: function whose body uses LET to reach its arg. */
    TEST_EQ(eql,    S(MY-DOUBLE),
                                 eval_cstring("(defun my-double (n) (let ((x n)) x))"));
    TEST_EQ(eql,    I(4),        eval_cstring("(my-double 4)"));}

static void test_lambda_immediate(void){
    TEST_EQ(eql,    I(42),       eval_cstring("((lambda (q) q) 42)"));
    TEST_EQ(eql,    I(7),        eval_cstring("((lambda (a b) a) 7 99)"));
    TEST_EQ(eql,    I(99),       eval_cstring("((lambda (a b) b) 7 99)"));}

static void test_function_special(void){
    /* (function foo) on a globally-bound function should return the function obj. */
    eval_cstring("(defun fs-fn (x) x)");
    Ref f = eval_cstring("(function fs-fn)");
    TEST_EQ(eql, T_Symbol, as_boolean(functionp(f)));}

static void test_flet(void){
    TEST_EQ(eql,    I(7),        eval_cstring("(flet ((g (x) x)) (g 7))"));
    /* FLET local doesn't see itself recursively — but here we don't test that;
       we just test it shadows a global. */
    eval_cstring("(defun fs-shadow (x) (list x x))");
    TEST_EQ(eql,    I(11),       eval_cstring("(flet ((fs-shadow (x) x)) (fs-shadow 11))"));}

static void test_labels(void){
    /* LABELS allows mutual references. Use a single-fn case here. */
    TEST_EQ(eql,    I(8),        eval_cstring("(labels ((h (x) x)) (h 8))"));}

static void test_block_simple(void){
    /* (block out (return-from out 13)) → 13 */
    TEST_EQ(eql,    I(13),       eval_cstring("(block out (return-from out 13))"));
    /* No return-from: result is last expression. */
    TEST_EQ(eql,    I(2),        eval_cstring("(block b 1 2)"));}

static void test_block_inside_progn(void){
    /* return-from from inside a nested progn unwinds the block. */
    TEST_EQ(eql,    I(99),
            eval_cstring("(block out (progn 1 (return-from out 99) 2))"));}

static void test_catch_throw(void){
    TEST_EQ(eql,    I(21),       eval_cstring("(catch (quote tag) (throw (quote tag) 21))"));}


TEST_INITIALIZE(test_eval);
TEST_SUITE(test_eval, eval_init, eval_clean)
    TEST_CASE( "quote",                 test_quote)
    TEST_CASE( "self-evaluating",       test_self_evaluating)
    TEST_CASE( "if",                    test_if)
    TEST_CASE( "progn",                 test_progn)
    TEST_CASE( "setq global",           test_setq_global)
    TEST_CASE( "setq multiple pairs",   test_setq_multiple_pairs)
    TEST_CASE( "let lexical",           test_let_lexical)
    TEST_CASE( "defparameter",          test_defparameter)
    TEST_CASE( "defun identity",        test_defun_identity)
    TEST_CASE( "defun let body",        test_defun_let_body)
    TEST_CASE( "lambda immediate",      test_lambda_immediate)
    TEST_CASE( "(function fn)",         test_function_special)
    TEST_CASE( "flet",                  test_flet)
    TEST_CASE( "labels",                test_labels)
    TEST_CASE( "block simple",          test_block_simple)
    TEST_CASE( "block inside progn",    test_block_inside_progn)
    TEST_CASE( "catch / throw",         test_catch_throw)
    TEST_FINALIZE()
