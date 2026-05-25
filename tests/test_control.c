#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"

/* Slice 3 — control macros from bootstrap/prelude.lisp:
   CASE, ECASE, TYPECASE, ETYPECASE, MULTIPLE-VALUE-BIND,
   MULTIPLE-VALUE-LIST, NTH-VALUE, DO, DO*, IGNORE-ERRORS,
   ASSERT, CHECK-TYPE, RETURN, WITH-OPEN-FILE. */

TEST_SUITE_SETUP(ctrl_init){
    kernel_initialize();
    bocl_load_cstring("/Users/pjb/src/public/bocl/bootstrap/prelude.lisp");
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(ctrl_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){ return bocl_read_eval_cstring(text); }

static void test_case_single_keys(void){
    TEST_EQ(eql, K(TWO),  e("(case 2 (1 :one) (2 :two) (3 :three))"));
    TEST_EQ(eql, NIL_Symbol, e("(case 99 (1 :one) (2 :two))"));}

static void test_case_list_keys_and_otherwise(void){
    TEST_EQ(eql, K(MID),  e("(case 5 ((1 2 3) :small) ((4 5 6) :mid))"));
    TEST_EQ(eql, K(DEFAULT), e("(case 99 (1 :one) (otherwise :default))"));}

static void test_ecase_matched_and_unmatched(void){
    TEST_EQ(eql, K(TWO),  e("(ecase 2 (1 :one) (2 :two))"));
    /* Unmatched ECASE errors — caught by HANDLER-CASE. */
    TEST_EQ(eql, K(CAUGHT),
            e("(handler-case (ecase 99 (1 :one)) (error (c) :caught))"));}

static void test_typecase(void){
    TEST_EQ(eql, K(I), e("(typecase 42 (string :s) (integer :i) (t :other))"));
    TEST_EQ(eql, K(S), e("(typecase \"hi\" (integer :i) (string :s))"));
    TEST_EQ(eql, K(OTHER), e("(typecase 'sym (integer :i) (string :s) (t :other))"));}

static void test_multiple_value_bind(void){
    TEST_EQ(equal, list(I(10), I(20), NULL),
            e("(multiple-value-bind (a b) (values 10 20) (list a b))"));
    /* Missing values default to NIL via &optional. */
    TEST_EQ(equal, list(I(10), NIL_Symbol, NULL),
            e("(multiple-value-bind (a b) (values 10) (list a b))"));}

static void test_multiple_value_list_and_nth(void){
    TEST_EQ(equal, list(I(1), I(2), I(3), NULL),
            e("(multiple-value-list (values 1 2 3))"));
    TEST_EQ(eql, S(B), e("(nth-value 1 (values 'a 'b 'c))"));}

static void test_do_loop(void){
    /* Sum 1..5 via DO. */
    Ref r = e("(let ((sum 0))"
              "  (do ((i 1 (+ i 1)))"
              "      ((> i 5) sum)"
              "    (setq sum (+ sum i))))");
    TEST_EQ(eql, I(15), r);}

static void test_do_star_loop(void){
    /* Sum of squares 0..3 via DO*. */
    Ref r = e("(let ((acc 0))"
              "  (do* ((i 0 (+ i 1)))"
              "       ((>= i 4) acc)"
              "    (setq acc (+ acc (* i i)))))");
    TEST_EQ(eql, I(14), r);}

static void test_ignore_errors_no_error(void){
    TEST_EQ(eql, I(3), e("(ignore-errors (+ 1 2))"));}

static void test_ignore_errors_catches(void){
    TEST_EQ(eql, NIL_Symbol, e("(ignore-errors (error \"oops\"))"));}

static void test_assert_passes(void){
    /* (assert t) returns NIL and doesn't error. */
    Ref r = e("(progn (assert (= 2 2)) :ok)");
    TEST_EQ(eql, K(OK), r);}

static void test_check_type_passes(void){
    Ref r = e("(progn (check-type 5 integer) :ok)");
    TEST_EQ(eql, K(OK), r);}

static void test_return_in_block_nil(void){
    /* (RETURN x) is (RETURN-FROM NIL x). Works in implicit/explicit
       BLOCK NIL (which DO establishes; here we use an explicit block). */
    TEST_EQ(eql, I(42), e("(block nil (return 42) :unreached))"));}

TEST_INITIALIZE(test_control);
TEST_SUITE(test_control, ctrl_init, ctrl_clean)
    TEST_CASE( "CASE single keys",         test_case_single_keys)
    TEST_CASE( "CASE list keys / OTHER",   test_case_list_keys_and_otherwise)
    TEST_CASE( "ECASE matched / unmatched",test_ecase_matched_and_unmatched)
    TEST_CASE( "TYPECASE",                 test_typecase)
    TEST_CASE( "MULTIPLE-VALUE-BIND",      test_multiple_value_bind)
    TEST_CASE( "MULTIPLE-VALUE-LIST/NTH",  test_multiple_value_list_and_nth)
    TEST_CASE( "DO  loop sums 1..5",       test_do_loop)
    TEST_CASE( "DO* loop sums i^2",        test_do_star_loop)
    TEST_CASE( "IGNORE-ERRORS happy",      test_ignore_errors_no_error)
    TEST_CASE( "IGNORE-ERRORS catches",    test_ignore_errors_catches)
    TEST_CASE( "ASSERT passes",            test_assert_passes)
    TEST_CASE( "CHECK-TYPE passes",        test_check_type_passes)
    TEST_CASE( "RETURN from BLOCK NIL",    test_return_in_block_nil)
    TEST_FINALIZE()
