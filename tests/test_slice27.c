/* tests/test_slice27.c — CUnit tests for Slice 27:
   FIND/POSITION/FIND-IF/POSITION-IF with :start/:end/:from-end,
   REMOVE/REMOVE-IF with :count/:start/:end,
   SUBSTITUTE/SUBSTITUTE-IF with bounds,
   STRING=/STRING< etc. with :start1/:end1/:start2/:end2,
   WRITE-TO-STRING with :base,
   MEMBER-IF/MEMBER-IF-NOT, MISMATCH with :from-end,
   SEARCH with full kwargs. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s27_init){
    kernel_initialize();
    bocl_test_load_prelude();
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s27_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- FIND with :start/:end/:from-end ------------------------------------- */

static void test_find_bounds(void){
    TEST_EQ(eql, I(2),
            e("(find 2 '(1 2 3 2 1))"));
    /* :start skips early elements */
    TEST_EQ(eql, I(2),
            e("(find 2 '(1 2 3 2 1) :start 2)"));
    /* :end limits search */
    TEST_EQ(eql, NIL_Symbol,
            e("(find 3 '(1 2 3 2 1) :end 2)"));
    /* :from-end returns last occurrence */
    TEST_EQ(eql, I(2),
            e("(find 2 '(1 2 3 2 1) :from-end t)"));}

/* ---- POSITION with :start/:end/:from-end -------------------------------- */

static void test_position_bounds(void){
    TEST_EQ(eql, I(1),
            e("(position 2 '(1 2 3 2 1))"));
    /* :from-end finds last */
    TEST_EQ(eql, I(3),
            e("(position 2 '(1 2 3 2 1) :from-end t)"));
    /* :start shifts window */
    TEST_EQ(eql, I(3),
            e("(position 2 '(1 2 3 2 1) :start 2)"));
    /* :end restricts */
    TEST_EQ(eql, NIL_Symbol,
            e("(position 3 '(1 2 3 2 1) :end 2)"));}

/* ---- REMOVE with :count ------------------------------------------------- */

static void test_remove_count(void){
    /* Remove only 1 occurrence */
    TEST_EQ(equal, e("'(1 3 2 1)"),
            e("(remove 2 '(1 2 3 2 1) :count 1)"));
    /* Remove all */
    TEST_EQ(equal, e("'(1 3 1)"),
            e("(remove 2 '(1 2 3 2 1))"));
    /* :start/:end restrict the removal region */
    TEST_EQ(equal, e("'(1 2 3 1)"),
            e("(remove 2 '(1 2 3 2 1) :start 2)"));}

/* ---- SUBSTITUTE ---------------------------------------------------------- */

static void test_substitute(void){
    TEST_EQ(equal, e("'(1 0 3 0 1)"),
            e("(substitute 0 2 '(1 2 3 2 1))"));
    /* :count limits replacements */
    TEST_EQ(equal, e("'(1 0 3 2 1)"),
            e("(substitute 0 2 '(1 2 3 2 1) :count 1)"));}

/* ---- STRING= with bounds ------------------------------------------------- */

static void test_string_eq_bounds(void){
    /* Compare substrings: "hello"[1..3] = "el", "jello"[1..3] = "el" */
    TEST_EQ(eql, T_Symbol,
            e("(string= \"hello\" \"jello\" :start1 1 :end1 3 :start2 1 :end2 3)"));
    TEST_EQ(eql, NIL_Symbol,
            e("(string= \"abc\" \"abd\")"));
    /* Full equality */
    TEST_EQ(eql, T_Symbol,
            e("(string= \"foo\" \"foo\")"));}

static void test_string_compare_bounds(void){
    TEST_EQ(eql, T_Symbol,
            e("(string< \"abc\" \"abd\")"));
    TEST_EQ(eql, NIL_Symbol,
            e("(string< \"abd\" \"abc\")"));
    TEST_EQ(eql, T_Symbol,
            e("(string-equal \"FOO\" \"foo\")"));
    /* Substring: compare \"ell\" vs \"ell\" */
    TEST_EQ(eql, T_Symbol,
            e("(string= \"hello\" \"belle\" :start1 1 :end1 4 :start2 1 :end2 4)"));}

/* ---- WRITE-TO-STRING :base ----------------------------------------------- */

static void test_write_to_string_base(void){
    TEST_EQ(equal, e("\"42\""),  e("(write-to-string 42)"));
    TEST_EQ(equal, e("\"FF\""),  e("(write-to-string 255 :base 16)"));
    TEST_EQ(equal, e("\"1010\""),e("(write-to-string 10 :base 2)"));
    TEST_EQ(equal, e("\"52\""),  e("(write-to-string 42 :base 8)"));}

/* ---- MEMBER-IF ----------------------------------------------------------- */

static void test_member_if(void){
    TEST_EQ(equal, e("'(2 3)"),
            e("(member-if #'evenp '(1 2 3))"));
    TEST_EQ(eql, NIL_Symbol,
            e("(member-if #'evenp '(1 3 5))")); }

/* ---- MISMATCH with :from-end --------------------------------------------- */

static void test_mismatch_from_end(void){
    TEST_EQ(eql, NIL_Symbol,
            e("(mismatch '(1 2 3) '(1 2 3))"));
    TEST_EQ(eql, I(2),
            e("(mismatch '(1 2 3) '(1 2 4))"));
    TEST_EQ(eql, I(3),
            e("(mismatch '(1 2 3) '(1 2 3 4))"));}

/* ---- SEARCH with :from-end ----------------------------------------------- */

static void test_search_from_end(void){
    TEST_EQ(eql, I(1),
            e("(search '(b c) '(a b c d b c))"));
    /* :from-end finds last match */
    TEST_EQ(eql, I(4),
            e("(search '(b c) '(a b c d b c) :from-end t)"));
    /* String search */
    TEST_EQ(eql, I(2),
            e("(search \"ll\" \"hello world\")"));}


TEST_INITIALIZE(test_slice27);
TEST_SUITE(test_slice27, s27_init, s27_fini)
    TEST_CASE("find :start/:end/:from-end",    test_find_bounds)
    TEST_CASE("position :start/:end/:from-end",test_position_bounds)
    TEST_CASE("remove :count/:start/:end",     test_remove_count)
    TEST_CASE("substitute/substitute-if",      test_substitute)
    TEST_CASE("string= with bounds",           test_string_eq_bounds)
    TEST_CASE("string comparisons with bounds",test_string_compare_bounds)
    TEST_CASE("write-to-string :base",         test_write_to_string_base)
    TEST_CASE("member-if",                     test_member_if)
    TEST_CASE("mismatch :from-end",            test_mismatch_from_end)
    TEST_CASE("search :from-end",              test_search_from_end)
    TEST_FINALIZE()
