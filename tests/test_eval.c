#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"
#include "eval.h"
#include "packages.h"

/* Phase 1 special-operator + apply tests. Each case round-trips a source
   string through bocl_read_eval_cstring and asserts the result. */

TEST_SUITE_SETUP(eval_init){
    kernel_initialize();
    bocl_test_load_prelude();   /* uses prelude-level operators (DEFMACRO/DEFPARAMETER/…) */
    /* Compare against COMMON-LISP symbols (the expected values are built with
       symbol_intern_cstring, which interns in CL); the prelude leaves *PACKAGE*
       = CL-USER, where read/eval'd bare symbols would be CL-USER::FOO. */
    bocl_in_package(COMMON_LISP_Package);
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(eval_clean){
    return CUE_SUCCESS;}


static Ref eval_cstring(const char* text){
    return bocl_test_eval(text);}

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

/* ------------------------------------------------------------------ */
/*  Lambda-list keywords: &OPTIONAL, &REST/&BODY, &KEY (Phase 5.3).    */
/* ------------------------------------------------------------------ */

static void test_optional(void){
    eval_cstring("(defun lo-1 (a &optional (b 10) c) (list a b c))");
    TEST_EQ(equal, list(I(1), I(10), NIL_Symbol, NULL),
                                 eval_cstring("(lo-1 1)"));
    TEST_EQ(equal, list(I(1), I(2), NIL_Symbol, NULL),
                                 eval_cstring("(lo-1 1 2)"));
    TEST_EQ(equal, list(I(1), I(2), I(3), NULL),
                                 eval_cstring("(lo-1 1 2 3)"));}

static void test_rest(void){
    eval_cstring("(defun lo-2 (a &rest r) (list a r))");
    TEST_EQ(equal, list(I(1), NIL_Symbol, NULL),
                                 eval_cstring("(lo-2 1)"));
    TEST_EQ(equal, list(I(1), list(I(2), I(3), I(4), NULL), NULL),
                                 eval_cstring("(lo-2 1 2 3 4)"));}

static void test_key(void){
    eval_cstring("(defun lo-3 (a &key b (c 99)) (list a b c))");
    TEST_EQ(equal, list(I(1), NIL_Symbol, I(99), NULL),
                                 eval_cstring("(lo-3 1)"));
    TEST_EQ(equal, list(I(1), I(2), I(99), NULL),
                                 eval_cstring("(lo-3 1 :b 2)"));
    TEST_EQ(equal, list(I(1), I(2), I(3), NULL),
                                 eval_cstring("(lo-3 1 :b 2 :c 3)"));
    /* Order of keyword args doesn't matter. */
    TEST_EQ(equal, list(I(1), I(2), I(3), NULL),
                                 eval_cstring("(lo-3 1 :c 3 :b 2)"));}

static void test_key_with_external_keyword(void){
    /* ((:external internal) init) — the user passes :external, the
       function sees `internal'. */
    eval_cstring("(defun lo-4 (&key ((:foo bar) 17)) bar)");
    TEST_EQ(eql, I(17), eval_cstring("(lo-4)"));
    TEST_EQ(eql, I(42), eval_cstring("(lo-4 :foo 42)"));}

static void test_rest_and_key(void){
    /* &REST and &KEY together: rest captures everything; key still binds. */
    eval_cstring("(defun lo-5 (&rest r &key (x 0)) (list r x))");
    TEST_EQ(equal, list(NIL_Symbol, I(0), NULL),
                                 eval_cstring("(lo-5)"));
    TEST_EQ(equal, list(list(K(X), I(7), NULL), I(7), NULL),
                                 eval_cstring("(lo-5 :x 7)"));}

/* ------------------------------------------------------------------ */
/*  The four newer special operators (slice 1 of the post-5.5 plan):  */
/*  LET*, MACROLET, SYMBOL-MACROLET, LOAD-TIME-VALUE.                  */
/* ------------------------------------------------------------------ */

static void test_let_star_sequential(void){
    /* Each init sees the previous bindings — would fail with LET. */
    TEST_EQ(equal, list(I(1), I(2), I(6), NULL),
            eval_cstring("(let* ((a 1) (b (* a 2)) (c (* b 3))) (list a b c))"));
    /* Inner LET* shadows outer. */
    TEST_EQ(equal, list(I(15), I(22), NULL),
            eval_cstring("(let* ((x 10)) (let* ((x (+ x 5)) (y (+ x 7))) (list x y)))"));}

static void test_macrolet_basic(void){
    /* A local macro defined with MACROLET. */
    TEST_EQ(eql, I(49),
            eval_cstring("(macrolet ((sqr (x) `(* ,x ,x))) (sqr 7))"));}

static void test_macrolet_shadows_global(void){
    /* A MACROLET binding shadows a global DEFMACRO of the same name.
       Each macro body returns ''sym so the expansion is (QUOTE sym) and
       eval of that yields the symbol. */
    eval_cstring("(defmacro mle-outer () ''globally)");
    TEST_EQ(eql, S(LOCALLY),
            eval_cstring("(macrolet ((mle-outer () ''locally)) (mle-outer))"));
    /* Outside the MACROLET, the global is back. */
    TEST_EQ(eql, S(GLOBALLY), eval_cstring("(mle-outer)"));}

static void test_symbol_macrolet(void){
    /* A bare symbol bound to an expansion. */
    TEST_EQ(eql, I(42),
            eval_cstring("(symbol-macrolet ((sm-x 42)) sm-x)"));
    /* In a context that uses arithmetic. */
    TEST_EQ(eql, I(50),
            eval_cstring("(symbol-macrolet ((sm-y 42)) (+ sm-y 8))"));}

static void test_load_time_value(void){
    /* Just evaluates the form once and returns the value. */
    TEST_EQ(eql, I(6), eval_cstring("(load-time-value (+ 1 2 3))"));}


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
    TEST_CASE( "lambda-list &optional", test_optional)
    TEST_CASE( "lambda-list &rest",     test_rest)
    TEST_CASE( "lambda-list &key",      test_key)
    TEST_CASE( "lambda-list &key (:ext intern)", test_key_with_external_keyword)
    TEST_CASE( "lambda-list &rest + &key",       test_rest_and_key)
    TEST_CASE( "LET* sequential",                test_let_star_sequential)
    TEST_CASE( "MACROLET",                       test_macrolet_basic)
    TEST_CASE( "MACROLET shadows global",        test_macrolet_shadows_global)
    TEST_CASE( "SYMBOL-MACROLET",                test_symbol_macrolet)
    TEST_CASE( "LOAD-TIME-VALUE",                test_load_time_value)
    TEST_FINALIZE()
