/* tests/test_list_utils.c — CUnit tests for Slice 12 list utilities */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(list_utils_init){
    kernel_initialize();
    bocl_load_cstring("/Users/pjb/src/public/bocl/bootstrap/prelude.lisp");
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(list_utils_fini){
    return CUE_SUCCESS;}

/* ---- helpers ---- */
static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- ASSOC ---- */
static void test_assoc(void){
    TEST_EQ(equal, e("'(b . 2)"),
            e("(assoc 'b '((a . 1) (b . 2) (c . 3)))"));
    TEST_EQ(equal, e("nil"),
            e("(assoc 'd '((a . 1) (b . 2) (c . 3)))"));
    TEST_EQ(equal, e("nil"),
            e("(assoc 'a '())"));
    /* :test override */
    TEST_EQ(equal, e("'(1 . x)"),
            e("(assoc 1.0 '((1 . x) (2 . y)) :test #'=)"));
    /* :key */
    TEST_EQ(equal, e("'((a b) . 1)"),
            e("(assoc 'a '(((a b) . 1) ((c d) . 2)) :key #'car)"));}

static void test_assoc_if(void){
    TEST_EQ(equal, e("'(2 . b)"),
            e("(assoc-if #'evenp '((1 . a) (2 . b) (3 . c)))"));
    TEST_EQ(equal, e("nil"),
            e("(assoc-if #'evenp '((1 . a) (3 . c)))"));}

static void test_assoc_if_not(void){
    TEST_EQ(equal, e("'(1 . a)"),
            e("(assoc-if-not #'evenp '((1 . a) (2 . b) (3 . c)))"));}

/* ---- RASSOC ---- */
static void test_rassoc(void){
    TEST_EQ(equal, e("'(a . 1)"),
            e("(rassoc 1 '((a . 1) (b . 2) (c . 1)))"));
    TEST_EQ(equal, e("nil"),
            e("(rassoc 9 '((a . 1) (b . 2)))"));
    /* :test */
    TEST_EQ(equal, e("'(a . 1)"),
            e("(rassoc 1.0 '((a . 1) (b . 2)) :test #'=)"));}

static void test_rassoc_if(void){
    TEST_EQ(equal, e("'(b . 2)"),
            e("(rassoc-if #'evenp '((a . 1) (b . 2) (c . 3)))"));}

/* ---- LIST* ---- */
static void test_list_star(void){
    TEST_EQ(equal, e("42"),
            e("(list* 42)"));
    TEST_EQ(equal, e("'(1 . 2)"),
            e("(list* 1 2)"));
    TEST_EQ(equal, e("'(1 2 . 3)"),
            e("(list* 1 2 3)"));
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(list* 1 2 '(3))"));}

/* ---- COPY-TREE ---- */
static void test_copy_tree(void){
    TEST_EQ(equal, e("'(1 (2 3) 4)"),
            e("(copy-tree '(1 (2 3) 4))"));
    /* atoms are returned as-is */
    TEST_EQ(equal, e("42"),
            e("(copy-tree 42)"));
    /* fresh cells — mutating original's car doesn't affect copy */
    TEST_EQ(equal, e("'(1 (2 3) 4)"),
            e("(let* ((orig (list 1 (list 2 3) 4))"
              "       (cpy  (copy-tree orig)))"
              "  (rplaca orig 99)"       /* mutate orig in place */
              "  cpy)"));               /* copy unchanged */}

/* ---- TAILP ---- */
static void test_tailp(void){
    TEST_EQ(equal, e("t"),
            e("(let ((l '(a b c))) (tailp (cddr l) l))"));
    TEST_EQ(equal, e("t"),
            e("(tailp nil '(a b))"));
    TEST_EQ(equal, e("nil"),
            e("(tailp '(x) '(a b c))"));}

/* ---- NCONC (variadic) ---- */
static void test_nconc(void){
    TEST_EQ(equal, e("nil"),
            e("(nconc)"));
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(nconc '(1 2) '(3))"));
    TEST_EQ(equal, e("'(1 2 3 4 5)"),
            e("(nconc '(1 2) '(3 4) '(5))"));
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(nconc nil '(1 2 3))"));
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(nconc '(1 2 3) nil)"));}

/* ---- DELETE-IF ---- */
static void test_delete_if(void){
    TEST_EQ(equal, e("'(1 3 5)"),
            e("(delete-if #'evenp '(1 2 3 4 5))"));
    TEST_EQ(equal, e("nil"),
            e("(delete-if #'oddp '(1 3 5))"));
    /* key returns even only for x=2, so only 2 is removed → (1 3) */
    TEST_EQ(equal, e("'(1 3)"),
            e("(delete-if #'evenp '(1 2 3) :key (lambda (x) (if (= x 2) 2 1)))"));}

/* ---- REMOVE-DUPLICATES / DELETE-DUPLICATES ---- */
static void test_remove_duplicates(void){
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(remove-duplicates '(1 2 1 3 2))"));
    TEST_EQ(equal, e("nil"),
            e("(remove-duplicates nil)"));
    TEST_EQ(equal, e("'(a b c)"),
            e("(remove-duplicates '(a b a c b))")
            );}

/* ---- SUBST ---- */
static void test_subst(void){
    TEST_EQ(equal, e("'(new b (new c))"),
            e("(subst 'new 'old '(old b (old c)))"));
    /* no match */
    TEST_EQ(equal, e("'(a b c)"),
            e("(subst 'z 'x '(a b c))"));
    /* :test override — use eq (safe on any type; = would error on cons nodes) */
    TEST_EQ(equal, e("'(new b new)"),
            e("(subst 'new 'old '(old b old) :test #'eq)"));}

static void test_subst_if(void){
    /* predicate must be safe on non-atoms too — wrap with numberp */
    TEST_EQ(equal, e("'(0 b 0)"),
            e("(subst-if 0 (lambda (x) (and (numberp x) (oddp x))) '(1 b 3))"));
    TEST_EQ(equal, e("'(1 b even)"),
            e("(subst-if 'even (lambda (x) (and (numberp x) (evenp x))) '(1 b 2))")
            /* only 2 is even → replaced; 1 is odd, b is not a number */ );}

/* ---- TREE-EQUAL ---- */
static void test_tree_equal(void){
    TEST_EQ(equal, e("t"),
            e("(tree-equal '(1 (2 3)) '(1 (2 3)))"));
    TEST_EQ(equal, e("nil"),
            e("(tree-equal '(1 2) '(1 3))"));
    TEST_EQ(equal, e("t"),
            e("(tree-equal 42 42)"));
    TEST_EQ(equal, e("nil"),
            e("(tree-equal '(1 2) '(1 2 3))"));}

/* ---- GET / REMPROP ---- */
static void test_get_remprop(void){
    TEST_EQ(equal, e("nil"),
            e("(get 'my-sym 'color)"));
    TEST_EQ(equal, e("'blue"),
            e("(progn (setf (get 'my-sym 'color) 'blue)"
              "       (get 'my-sym 'color))"));
    TEST_EQ(equal, e("t"),
            e("(remprop 'my-sym 'color)"));
    TEST_EQ(equal, e("nil"),
            e("(get 'my-sym 'color)"));
    /* remprop on absent key */
    TEST_EQ(equal, e("nil"),
            e("(remprop 'my-sym 'no-such-key)"));}

TEST_INITIALIZE(test_list_utils);
TEST_SUITE(test_list_utils, list_utils_init, list_utils_fini)
    TEST_CASE("assoc",              test_assoc)
    TEST_CASE("assoc-if",           test_assoc_if)
    TEST_CASE("assoc-if-not",       test_assoc_if_not)
    TEST_CASE("rassoc",             test_rassoc)
    TEST_CASE("rassoc-if",          test_rassoc_if)
    TEST_CASE("list*",              test_list_star)
    TEST_CASE("copy-tree",          test_copy_tree)
    TEST_CASE("tailp",              test_tailp)
    TEST_CASE("nconc variadic",     test_nconc)
    TEST_CASE("delete-if",          test_delete_if)
    TEST_CASE("remove-duplicates",  test_remove_duplicates)
    TEST_CASE("subst",              test_subst)
    TEST_CASE("subst-if",           test_subst_if)
    TEST_CASE("tree-equal",         test_tree_equal)
    TEST_CASE("get / remprop",      test_get_remprop)
    TEST_FINALIZE()
