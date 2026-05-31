/* tests/test_slice29.c — CUnit tests for Slice 29:
   &AUX in lambda lists, PRINT-UNREADABLE-OBJECT, COUNT/REDUCE with bounds,
   HASH-TABLE-COUNT, CLRHASH, STRING-CONCAT, CHAR-CODE-LIMIT, SUBTYPEP,
   LOOP-FOR-HASH, SXHASH. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s29_init){
    kernel_initialize();
    bocl_load_cstring("/Users/pjb/src/public/bocl/bootstrap/prelude.lisp");
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s29_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- &AUX in lambda lists ----------------------------------------------- */

static void test_aux_lambda(void){
    /* Simple &AUX binding: y = x*2 = 6, result = x+y = 3+6 = 9 */
    TEST_EQ(eql, I(9),
            e("(funcall (lambda (x &aux (y (* x 2))) (+ x y)) 3)"));
    /* &AUX without init (defaults to NIL) */
    TEST_EQ(eql, NIL_Symbol,
            e("(funcall (lambda (x &aux y) y) 42)"));
    /* &AUX in DEFUN */
    e("(defun double-plus (x &aux (y (* x 2))) (+ x y))");
    TEST_EQ(eql, I(9),
            e("(double-plus 3)"));}

/* ---- PRINT-UNREADABLE-OBJECT -------------------------------------------- */

static void test_print_unreadable_object(void){
    /* Basic: should produce #<...> form */
    TEST_EQ(eql, T_Symbol,
            e("(let ((s (with-output-to-string (out)"
              "          (print-unreadable-object (42 out) (write-string \"foo\" out)))))"
              "  (and (> (length s) 3)"
              "       (char= (char s 0) #\\#)"
              "       (char= (char s 1) #\\<)))"));}

/* ---- COUNT with :start/:end --------------------------------------------- */

static void test_count_bounds(void){
    TEST_EQ(eql, I(2), e("(count 2 '(1 2 3 2 1))"));
    /* restrict to [1,4) */
    TEST_EQ(eql, I(2), e("(count 2 '(1 2 3 2 1) :start 1 :end 4)"));
    /* restrict so only one 2 is in range */
    TEST_EQ(eql, I(1), e("(count 2 '(1 2 3 2 1) :start 1 :end 3)"));
    /* count-if with bounds */
    TEST_EQ(eql, I(2), e("(count-if #'evenp '(1 2 3 4 5) :start 1 :end 4)"));}

/* ---- REDUCE with :start/:end -------------------------------------------- */

static void test_reduce_bounds(void){
    TEST_EQ(eql, I(15), e("(reduce #'+ '(1 2 3 4 5))"));
    /* only sum [1,4) = 2+3+4 = 9 */
    TEST_EQ(eql, I(9),  e("(reduce #'+ '(1 2 3 4 5) :start 1 :end 4)"));
    /* from-end: (list 1 (list 2 (list 3 4))) = (1 (2 (3 4))) */
    TEST_EQ(equal, e("\"(1 (2 (3 4)))\""),
            e("(princ-to-string (reduce #'list '(1 2 3 4) :from-end t))"));}

/* ---- HASH-TABLE-COUNT / CLRHASH ----------------------------------------- */

static void test_hash_table_ops(void){
    TEST_EQ(eql, I(3),
            e("(let ((h (make-hash-table)))"
              "  (sethash 'a h 1) (sethash 'b h 2) (sethash 'c h 3)"
              "  (hash-table-count h))"));
    TEST_EQ(eql, I(0),
            e("(let ((h (make-hash-table)))"
              "  (sethash 'a h 1) (sethash 'b h 2)"
              "  (clrhash h)"
              "  (hash-table-count h))"));}

/* ---- STRING-CONCAT ------------------------------------------------------- */

static void test_string_concat(void){
    TEST_EQ(equal, e("\"hello world\""),
            e("(string-concat \"hello\" \" \" \"world\")"));
    TEST_EQ(equal, e("\"\""),
            e("(string-concat)"));}

/* ---- CHAR-CODE-LIMIT ---------------------------------------------------- */

static void test_char_code_limit(void){
    TEST_EQ(eql, I(256), e("char-code-limit"));}

/* ---- SUBTYPEP ------------------------------------------------------------ */

static void test_subtypep(void){
    TEST_EQ(eql, T_Symbol, e("(subtypep 'integer 'number)"));
    TEST_EQ(eql, T_Symbol, e("(subtypep 'string 'sequence)"));
    TEST_EQ(eql, T_Symbol, e("(subtypep 'cons 'list)"));
    TEST_EQ(eql, T_Symbol, e("(subtypep 'integer 'integer)"));
    TEST_EQ(eql, T_Symbol, e("(subtypep nil t)")); }

/* ---- LOOP-FOR-HASH ------------------------------------------------------- */

static void test_loop_for_hash(void){
    TEST_EQ(eql, I(6),
            e("(let ((h (make-hash-table)) (n 0))"
              "  (sethash :a h 1) (sethash :b h 2) (sethash :c h 3)"
              "  (loop-for-hash (k v h) (setq n (+ n v)))"
              "  n)"));}


TEST_INITIALIZE(test_slice29);
TEST_SUITE(test_slice29, s29_init, s29_fini)
    TEST_CASE("&aux in lambda lists",           test_aux_lambda)
    TEST_CASE("print-unreadable-object",        test_print_unreadable_object)
    TEST_CASE("count with :start/:end",         test_count_bounds)
    TEST_CASE("reduce with :start/:end",        test_reduce_bounds)
    TEST_CASE("hash-table-count/clrhash",       test_hash_table_ops)
    TEST_CASE("string-concat",                  test_string_concat)
    TEST_CASE("char-code-limit",               test_char_code_limit)
    TEST_CASE("subtypep",                       test_subtypep)
    TEST_CASE("loop-for-hash",                  test_loop_for_hash)
    TEST_FINALIZE()
