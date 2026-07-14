#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"

/* Phase 4.1 hash-table tests. */

TEST_SUITE_SETUP(ht_init){
    kernel_initialize();
    bocl_test_load_prelude();   /* uses prelude-level operators (DEFPARAMETER/…) */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(ht_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){ return bocl_test_eval(text); }

static void test_make_and_p(void){
    e("(defparameter th-h (make-hash-table))");
    TEST_EQ(eql, T_Symbol,   e("(hash-table-p th-h)"));
    TEST_EQ(eql, NIL_Symbol, e("(hash-table-p 42)"));
    TEST_EQ(eql, NIL_Symbol, e("(hash-table-p nil)"));}

static void test_default_test_is_eql(void){
    e("(defparameter th-h1 (make-hash-table))");
    TEST_EQ(eql, S(EQL), e("(hash-table-test th-h1)"));}

static void test_get_set_count(void){
    e("(defparameter th-h2 (make-hash-table))");
    TEST_EQ(eql, I(0),  e("(hash-table-count th-h2)"));
    TEST_EQ(eql, I(7),  e("(setf (gethash 1 th-h2) 7)"));
    TEST_EQ(eql, I(8),  e("(setf (gethash 2 th-h2) 8)"));
    TEST_EQ(eql, I(2),  e("(hash-table-count th-h2)"));
    TEST_EQ(eql, I(7),  e("(gethash 1 th-h2)"));
    TEST_EQ(eql, I(8),  e("(gethash 2 th-h2)"));
    /* Overwriting an existing key does not bump count. */
    TEST_EQ(eql, I(70), e("(setf (gethash 1 th-h2) 70)"));
    TEST_EQ(eql, I(2),  e("(hash-table-count th-h2)"));
    TEST_EQ(eql, I(70), e("(gethash 1 th-h2)"));}

static void test_gethash_missing_default(void){
    e("(defparameter th-h3 (make-hash-table))");
    TEST_EQ(eql, NIL_Symbol, e("(gethash :missing th-h3)"));
    TEST_EQ(eql, I(99),      e("(gethash :missing th-h3 99)"));}

static void test_remhash(void){
    e("(defparameter th-h4 (make-hash-table))");
    e("(setf (gethash :a th-h4) 1)");
    e("(setf (gethash :b th-h4) 2)");
    TEST_EQ(eql, I(2),       e("(hash-table-count th-h4)"));
    TEST_EQ(eql, T_Symbol,   e("(remhash :a th-h4)"));
    TEST_EQ(eql, I(1),       e("(hash-table-count th-h4)"));
    TEST_EQ(eql, NIL_Symbol, e("(gethash :a th-h4)"));
    TEST_EQ(eql, I(2),       e("(gethash :b th-h4)"));
    /* Removing a missing key returns NIL. */
    TEST_EQ(eql, NIL_Symbol, e("(remhash :missing th-h4)"));}

static void test_clrhash(void){
    e("(defparameter th-h5 (make-hash-table))");
    e("(setf (gethash :a th-h5) 1)");
    e("(setf (gethash :b th-h5) 2)");
    e("(setf (gethash :c th-h5) 3)");
    TEST_EQ(eql, I(3), e("(hash-table-count th-h5)"));
    e("(clrhash th-h5)");
    TEST_EQ(eql, I(0), e("(hash-table-count th-h5)"));
    TEST_EQ(eql, NIL_Symbol, e("(gethash :a th-h5)"));}

static void test_test_eq(void){
    /* :test EQ: only object-identity equal keys collide. */
    e("(defparameter th-eq (make-hash-table :test 'eq))");
    e("(setf (gethash 'foo th-eq) 1)");
    /* The same symbol is interned in KERNEL_obarray so EQ holds. */
    TEST_EQ(eql, I(1), e("(gethash 'foo th-eq)"));}

static void test_test_equal_strings(void){
    /* :test EQUAL: two STRING objects with the same characters collide. */
    e("(defparameter th-equal (make-hash-table :test 'equal))");
    e("(setf (gethash \"hello\" th-equal) 1)");
    TEST_EQ(eql, I(1), e("(gethash \"hello\" th-equal)"));
    /* The literal "hello" in the source is a fresh STRING each time, so
       only EQUAL (not EQL) can find it. */}

static void test_test_equalp_case_insensitive(void){
    e("(defparameter th-equalp (make-hash-table :test 'equalp))");
    e("(setf (gethash \"Hello\" th-equalp) 1)");
    TEST_EQ(eql, I(1), e("(gethash \"HELLO\" th-equalp)"));
    TEST_EQ(eql, I(1), e("(gethash \"hello\" th-equalp)"));}

static void test_maphash_visits_all(void){
    e("(defparameter th-map (make-hash-table))");
    e("(setf (gethash 1 th-map) 10)");
    e("(setf (gethash 2 th-map) 20)");
    e("(setf (gethash 3 th-map) 30)");
    e("(defparameter th-sum 0)");
    e("(maphash (lambda (k v) (setq th-sum (+ th-sum k v))) th-map)");
    /* 1+10 + 2+20 + 3+30 = 66 */
    TEST_EQ(eql, I(66), e("th-sum"));}

static void test_eq_eql_equalp_builtins(void){
    /* EQ on the same symbol Ref. */
    TEST_EQ(eql, T_Symbol,   e("(eq 'foo 'foo)"));
    /* Distinct INTEGERs with the same value: EQ may or may not hold for
       small ints (interned), but EQL does, and EQUALP does. */
    TEST_EQ(eql, T_Symbol,   e("(eql 5 5)"));
    TEST_EQ(eql, T_Symbol,   e("(equalp 5 5.0)"));   /* cross-type */
    TEST_EQ(eql, T_Symbol,   e("(equalp \"Hi\" \"hi\")"));
    TEST_EQ(eql, NIL_Symbol, e("(equal \"Hi\" \"hi\")"));}

TEST_INITIALIZE(test_hash_table);
TEST_SUITE(test_hash_table, ht_init, ht_clean)
    TEST_CASE( "make + hash-table-p",     test_make_and_p)
    TEST_CASE( "default test is EQL",     test_default_test_is_eql)
    TEST_CASE( "get / set / count",       test_get_set_count)
    TEST_CASE( "gethash missing/default", test_gethash_missing_default)
    TEST_CASE( "remhash",                 test_remhash)
    TEST_CASE( "clrhash",                 test_clrhash)
    TEST_CASE( ":test EQ",                test_test_eq)
    TEST_CASE( ":test EQUAL strings",     test_test_equal_strings)
    TEST_CASE( ":test EQUALP case-ins",   test_test_equalp_case_insensitive)
    TEST_CASE( "maphash iterates all",    test_maphash_visits_all)
    TEST_CASE( "EQ / EQUALP built-ins",   test_eq_eql_equalp_builtins)
    TEST_FINALIZE()
