/* tests/test_slice28.c — CUnit tests for Slice 28:
   EVERY/SOME/NOTEVERY/NOTANY with multiple sequences,
   DO-HASH-TABLE, IOTA, FLATTEN, ENSURE-LIST, TAKE/DROP,
   WHEN-LET/IF-LET, STRING-JOIN, SPLIT-SEQUENCE,
   LAST-ELT, EMPTYP, STARTS-WITH, ENDS-WITH. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s28_init){
    kernel_initialize();
    bocl_test_load_prelude();
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s28_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- EVERY/SOME with multiple sequences ---------------------------------- */

static void test_every_multi(void){
    /* single sequence still works */
    TEST_EQ(eql, T_Symbol,   e("(every #'oddp '(1 3 5))"));
    TEST_EQ(eql, NIL_Symbol, e("(every #'oddp '(1 2 3))"));
    /* two sequences */
    TEST_EQ(eql, T_Symbol,
            e("(every #'< '(1 2 3) '(2 3 4))"));
    TEST_EQ(eql, NIL_Symbol,
            e("(every #'< '(1 2 3) '(2 1 4))"));
    /* stops at shortest */
    TEST_EQ(eql, T_Symbol,
            e("(every #'< '(1 2) '(2 3 4))")); }

static void test_some_multi(void){
    TEST_EQ(eql, T_Symbol,   e("(some #'evenp '(1 2 3))"));
    TEST_EQ(eql, NIL_Symbol, e("(some #'evenp '(1 3 5))"));
    /* two sequences */
    TEST_EQ(eql, T_Symbol,
            e("(some #'= '(1 2 3) '(9 2 7))"));}

static void test_notevery_notany(void){
    TEST_EQ(eql, T_Symbol,   e("(notevery #'oddp '(1 2 3))"));
    TEST_EQ(eql, NIL_Symbol, e("(notevery #'oddp '(1 3 5))"));
    TEST_EQ(eql, T_Symbol,   e("(notany #'evenp '(1 3 5))"));
    TEST_EQ(eql, NIL_Symbol, e("(notany #'evenp '(1 2 3))")); }

/* ---- DO-HASH-TABLE ------------------------------------------------------- */

static void test_do_hash_table(void){
    /* collect all keys */
    /* sum of values 1+2+3 = 6 */
    TEST_EQ(eql, I(6),
            e("(let ((h (make-hash-table)) (n 0))"
              "  (sethash 'a h 1) (sethash 'b h 2) (sethash 'c h 3)"
              "  (do-hash-table (k v h) (setq n (+ n v)))"
              "  n)"));}

/* ---- IOTA ---------------------------------------------------------------- */

static void test_iota(void){
    TEST_EQ(equal, e("'(0 1 2 3 4)"), e("(iota 5)"));
    TEST_EQ(equal, e("'(1 2 3)"),     e("(iota 3 1)"));
    TEST_EQ(equal, e("'(0 2 4 6)"),   e("(iota 4 0 2)"));
    TEST_EQ(equal, e("'()"),          e("(iota 0)"));}

/* ---- FLATTEN ------------------------------------------------------------- */

static void test_flatten(void){
    TEST_EQ(equal, e("'(1 2 3)"),     e("(flatten '(1 2 3))"));
    TEST_EQ(equal, e("'(1 2 3 4 5)"), e("(flatten '(1 (2 3) (4 (5))))"));
    TEST_EQ(equal, e("'()"),          e("(flatten nil)"));
    TEST_EQ(equal, e("'(1)"),         e("(flatten 1)"));}

/* ---- ENSURE-LIST --------------------------------------------------------- */

static void test_ensure_list(void){
    TEST_EQ(equal, e("'(1 2 3)"), e("(ensure-list '(1 2 3))"));
    TEST_EQ(equal, e("'(42)"),    e("(ensure-list 42)"));
    TEST_EQ(equal, e("'()"),      e("(ensure-list nil)"));}

/* ---- TAKE / DROP --------------------------------------------------------- */

static void test_take_drop(void){
    TEST_EQ(equal, e("'(1 2 3)"), e("(take 3 '(1 2 3 4 5))"));
    TEST_EQ(equal, e("'(1 2 3 4 5)"), e("(take 10 '(1 2 3 4 5))"));
    TEST_EQ(equal, e("'(4 5)"),   e("(drop 3 '(1 2 3 4 5))"));
    TEST_EQ(equal, e("'()"),      e("(drop 10 '(1 2 3))")); }

/* ---- WHEN-LET / IF-LET --------------------------------------------------- */

static void test_when_let(void){
    /* non-nil binding fires body */
    TEST_EQ(eql, I(42),
            e("(when-let (x 42) x)"));
    /* nil binding skips body, returns nil */
    TEST_EQ(eql, NIL_Symbol,
            e("(when-let (x nil) x)"));}

static void test_if_let(void){
    TEST_EQ(eql, I(1),
            e("(if-let (x 1) x 0)"));
    TEST_EQ(eql, I(0),
            e("(if-let (x nil) x 0)"));}

/* ---- STRING-JOIN / SPLIT-SEQUENCE ---------------------------------------- */

static void test_string_join(void){
    TEST_EQ(equal, e("\"a,b,c\""),
            e("(string-join '(\"a\" \"b\" \"c\") \",\")"));
    TEST_EQ(equal, e("\"abc\""),
            e("(string-join '(\"a\" \"b\" \"c\"))"));
    TEST_EQ(equal, e("\"\""),
            e("(string-join '())"));}

static void test_split_sequence(void){
    TEST_EQ(equal, e("'(\"a\" \"b\" \"c\")"),
            e("(split-sequence #\\, \"a,b,c\")"));
    TEST_EQ(equal, e("'(\"a\" \"\" \"c\")"),
            e("(split-sequence #\\, \"a,,c\")"));
    /* :remove-empty-subseqs */
    TEST_EQ(equal, e("'(\"a\" \"c\")"),
            e("(split-sequence #\\, \"a,,c\" :remove-empty-subseqs t)"));}

/* ---- LAST-ELT / EMPTYP / STARTS-WITH / ENDS-WITH ----------------------- */

static void test_sequence_extras(void){
    TEST_EQ(eql, I(3), e("(last-elt '(1 2 3))"));
    TEST_EQ(eql, T_Symbol,   e("(emptyp '())"));
    TEST_EQ(eql, NIL_Symbol, e("(emptyp '(1))"));
    TEST_EQ(eql, T_Symbol,   e("(starts-with '(1 2) '(1 2 3))"));
    TEST_EQ(eql, NIL_Symbol, e("(starts-with '(2 3) '(1 2 3))"));
    TEST_EQ(eql, T_Symbol,   e("(ends-with '(2 3) '(1 2 3))"));
    TEST_EQ(eql, NIL_Symbol, e("(ends-with '(1 2) '(1 2 3))"));}


TEST_INITIALIZE(test_slice28);
TEST_SUITE(test_slice28, s28_init, s28_fini)
    TEST_CASE("every/some multi-sequence",  test_every_multi)
    TEST_CASE("some multi-sequence",        test_some_multi)
    TEST_CASE("notevery/notany",            test_notevery_notany)
    TEST_CASE("do-hash-table",              test_do_hash_table)
    TEST_CASE("iota",                       test_iota)
    TEST_CASE("flatten",                    test_flatten)
    TEST_CASE("ensure-list",               test_ensure_list)
    TEST_CASE("take/drop",                  test_take_drop)
    TEST_CASE("when-let",                   test_when_let)
    TEST_CASE("if-let",                     test_if_let)
    TEST_CASE("string-join",               test_string_join)
    TEST_CASE("split-sequence",            test_split_sequence)
    TEST_CASE("last-elt/emptyp/starts/ends",test_sequence_extras)
    TEST_FINALIZE()
