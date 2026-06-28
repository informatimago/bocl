/* tests/test_slice17.c — CUnit tests for Slice 17:
   generic sequence functions (every, some, find-if, find, position-if, position,
   count-if, count, remove-if, remove on strings/vectors),
   -if-not variants (find-if-not, position-if-not, count-if-not, remove-if-not),
   sort/stable-sort on vectors,
   pathname component accessors (pathname-name, pathname-type, pathname-directory,
   directory-namestring, make-pathname, merge-pathnames),
   map-into,
   with-standard-io-syntax. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s17_init){
    kernel_initialize();
    bocl_test_load_prelude();
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s17_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- EVERY generic ------------------------------------------------------- */

static void test_every_generic(void){
    /* every on a string: all alphabetic */
    TEST_EQ(eql, T_Symbol, e("(every #'alpha-char-p \"abc\")"));
    /* fails on digit */
    TEST_EQ(equal, e("nil"), e("(every #'alpha-char-p \"abc1\")"));
    /* still works on lists */
    TEST_EQ(eql, T_Symbol, e("(every #'numberp '(1 2 3))"));
    TEST_EQ(equal, e("nil"), e("(every #'numberp '(1 2 x))")); }

/* ---- SOME generic -------------------------------------------------------- */

static void test_some_generic(void){
    /* some on a string */
    TEST_EQ(eql, T_Symbol, e("(if (some #'digit-char-p \"hello3\") t nil)"));
    TEST_EQ(equal, e("nil"), e("(some #'digit-char-p \"hello\")"));
    /* still works on lists */
    TEST_EQ(eql, T_Symbol, e("(if (some #'evenp '(1 3 4)) t nil)")); }

/* ---- FIND generic -------------------------------------------------------- */

static void test_find_generic(void){
    /* find on a string */
    TEST_EQ(eql, C(l), e("(find #\\l \"hello\")"));
    TEST_EQ(equal, e("nil"), e("(find #\\z \"hello\")"));
    /* find on a vector */
    TEST_EQ(eql, I(3), e("(find 3 #(1 2 3 4))"));
    TEST_EQ(equal, e("nil"), e("(find 9 #(1 2 3 4))")); }

/* ---- FIND-IF-NOT --------------------------------------------------------- */

static void test_find_if_not(void){
    TEST_EQ(eql, I(1), e("(find-if-not #'evenp '(2 4 1 6))"));
    TEST_EQ(equal, e("nil"), e("(find-if-not #'evenp '(2 4 6))")); }

/* ---- POSITION generic ---------------------------------------------------- */

static void test_position_generic(void){
    /* position on a string — first #\l is at index 2 in "hello" */
    TEST_EQ(eql, I(2), e("(position #\\l \"hello\")"));
    TEST_EQ(equal, e("nil"), e("(position #\\z \"hello\")"));
    /* position on a vector */
    TEST_EQ(eql, I(2), e("(position 3 #(1 2 3 4))")); }

/* ---- POSITION-IF-NOT ----------------------------------------------------- */

static void test_position_if_not(void){
    /* first non-even is at index 2 in (2 4 1 6) */
    TEST_EQ(eql, I(2), e("(position-if-not #'evenp '(2 4 1 6))"));
    TEST_EQ(equal, e("nil"), e("(position-if-not #'evenp '(2 4 6))")); }

/* ---- COUNT generic ------------------------------------------------------- */

static void test_count_generic(void){
    /* count #\l in "hello" → 2 */
    TEST_EQ(eql, I(2), e("(count #\\l \"hello\")"));
    TEST_EQ(eql, I(0), e("(count #\\z \"hello\")"));
    /* count on a vector */
    TEST_EQ(eql, I(2), e("(count 1 #(1 2 1 3))")); }

/* ---- COUNT-IF-NOT -------------------------------------------------------- */

static void test_count_if_not(void){
    /* odd numbers in (1 2 3 4) → 2 */
    TEST_EQ(eql, I(2), e("(count-if-not #'evenp '(1 2 3 4))")); }

/* ---- REMOVE-IF-NOT (list) ------------------------------------------------ */

static void test_remove_generic_list(void){
    TEST_EQ(equal, e("'(2 4)"), e("(remove-if-not #'evenp '(1 2 3 4))")); }

/* ---- REMOVE generic on string -------------------------------------------- */

static void test_remove_generic_string(void){
    /* remove all #\l from "hello" → "heo" */
    TEST_EQ(equal, e("\"heo\""), e("(remove #\\l \"hello\")"));
    /* result is still a string */
    TEST_EQ(eql, I(3), e("(string-length (remove #\\l \"hello\"))")); }

/* ---- SORT on vector ------------------------------------------------------ */

static void test_sort_vector(void){
    /* sort vector in place, coerce back to list to compare */
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(let ((v (make-array 3 :initial-contents '(3 1 2)))) (sort v #'<) (coerce v 'list))"));
    /* verify in-place mutation */
    TEST_EQ(eql, I(1),
            e("(let ((v (make-array 3 :initial-contents '(3 1 2)))) (sort v #'<) (aref v 0))")); }

/* ---- PATHNAME-NAME ------------------------------------------------------- */

static void test_pathname_name(void){
    TEST_EQ(equal, e("\"foo\""), e("(pathname-name \"/usr/local/foo.lisp\")"));
    TEST_EQ(equal, e("\"README\""), e("(pathname-name \"/home/user/README\")"));
    TEST_EQ(equal, e("\"bar\""), e("(pathname-name \"bar.c\")"));
    TEST_EQ(equal, e("nil"),   e("(pathname-name \"\")")); }

/* ---- PATHNAME-TYPE ------------------------------------------------------- */

static void test_pathname_type(void){
    TEST_EQ(equal, e("\"lisp\""), e("(pathname-type \"/usr/local/foo.lisp\")"));
    TEST_EQ(equal, e("nil"),     e("(pathname-type \"/home/user/README\")"));
    TEST_EQ(equal, e("\"c\""),   e("(pathname-type \"bar.c\")")); }

/* ---- DIRECTORY-NAMESTRING ------------------------------------------------ */

static void test_directory_namestring(void){
    TEST_EQ(equal, e("\"/usr/local/\""), e("(directory-namestring \"/usr/local/foo.lisp\")"));
    TEST_EQ(equal, e("\"\""),            e("(directory-namestring \"foo.lisp\")")); }

/* ---- MAKE-PATHNAME ------------------------------------------------------- */

static void test_make_pathname(void){
    TEST_EQ(equal,
            e("\"/usr/local/foo.lisp\""),
            e("(namestring (make-pathname :directory '(:absolute \"usr\" \"local\") :name \"foo\" :type \"lisp\"))"));
    TEST_EQ(equal,
            e("\"foo.lisp\""),
            e("(namestring (make-pathname :name \"foo\" :type \"lisp\"))")); }

/* ---- MERGE-PATHNAMES ----------------------------------------------------- */

static void test_merge_pathnames(void){
    TEST_EQ(equal,
            e("\"/usr/local/foo.lisp\""),
            e("(namestring (merge-pathnames \"foo.lisp\" \"/usr/local/\"))"));
    /* absolute path is returned unchanged */
    TEST_EQ(equal,
            e("\"/abs/path.lisp\""),
            e("(namestring (merge-pathnames \"/abs/path.lisp\" \"/other/\"))")); }

/* ---- MAP-INTO ------------------------------------------------------------ */

static void test_map_into(void){
    TEST_EQ(equal, e("'(2 4 6)"),
            e("(let ((r (list 0 0 0))) (map-into r (lambda (x) (* x 2)) '(1 2 3)) r)"));
    /* stops at shortest */
    TEST_EQ(equal, e("'(2 4)"),
            e("(let ((r (list 0 0))) (map-into r (lambda (x) (* x 2)) '(1 2 3)) r)")); }

/* ---- WITH-STANDARD-IO-SYNTAX -------------------------------------------- */

static void test_with_standard_io_syntax(void){
    TEST_EQ(equal, e("\"42\""), e("(with-standard-io-syntax (write-to-string 42))"));
    TEST_EQ(equal, e("\"NIL\""), e("(with-standard-io-syntax (write-to-string nil))")); }

TEST_INITIALIZE(test_slice17);
TEST_SUITE(test_slice17, s17_init, s17_fini)
    TEST_CASE("every-generic",          test_every_generic)
    TEST_CASE("some-generic",           test_some_generic)
    TEST_CASE("find-generic",           test_find_generic)
    TEST_CASE("find-if-not",            test_find_if_not)
    TEST_CASE("position-generic",       test_position_generic)
    TEST_CASE("position-if-not",        test_position_if_not)
    TEST_CASE("count-generic",          test_count_generic)
    TEST_CASE("count-if-not",           test_count_if_not)
    TEST_CASE("remove-if-not-list",     test_remove_generic_list)
    TEST_CASE("remove-generic-string",  test_remove_generic_string)
    TEST_CASE("sort-vector",            test_sort_vector)
    TEST_CASE("pathname-name",          test_pathname_name)
    TEST_CASE("pathname-type",          test_pathname_type)
    TEST_CASE("directory-namestring",   test_directory_namestring)
    TEST_CASE("make-pathname",          test_make_pathname)
    TEST_CASE("merge-pathnames",        test_merge_pathnames)
    TEST_CASE("map-into",               test_map_into)
    TEST_CASE("with-standard-io-syntax",test_with_standard_io_syntax)
    TEST_FINALIZE()
