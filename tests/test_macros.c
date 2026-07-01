#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"

/* Phase 5.1 macro tests: DEFMACRO, &rest / &body lambda-list keywords,
   backquote/comma/comma-at, and a small Lisp file loaded through bocl_load
   that uses macros it defined earlier in the same file. */

TEST_SUITE_SETUP(macros_init){
    kernel_initialize();
    bocl_test_load_prelude();   /* uses prelude-level operators (DEFMACRO/DEFPARAMETER/…) */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(macros_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){ return bocl_read_eval_cstring(text); }

static void test_defmacro_basic(void){
    e("(defmacro twice (x) (list 'progn x x))");
    e("(defparameter *tm-c* 0)");
    e("(twice (setq *tm-c* (+ *tm-c* 1)))");
    TEST_EQ(eql, I(2), e("*tm-c*"));}

static void test_defmacro_body_keyword(void){
    e("(defmacro when2 (test &body body)"
      "  (list 'if test (cons 'progn body)))");
    TEST_EQ(eql, I(99),       e("(when2 t 1 2 99)"));
    TEST_EQ(eql, NIL_Symbol,  e("(when2 nil 1 2 99)"));}

static void test_defmacro_rest_keyword(void){
    e("(defmacro my-list (&rest args) (cons 'list args))");
    TEST_EQ(equal,
            list(I(1), I(2), I(3), I(4), I(5), NULL),
            e("(my-list 1 2 3 4 5)"));}

static void test_backquote_basic(void){
    /* `,x → x's value; `(a ,x b) → (a <x's value> b). */
    e("(setq tm-x 42)");
    TEST_EQ(eql, I(42), e("`,tm-x"));
    TEST_EQ(equal,
            list(S(A), I(42), S(B), NULL),
            e("`(a ,tm-x b)"));}

static void test_backquote_splicing(void){
    /* `(a ,@lst b) splices lst. */
    e("(setq tm-l (list 1 2 3))");
    TEST_EQ(equal,
            list(S(A), I(1), I(2), I(3), S(B), NULL),
            e("`(a ,@tm-l b)"));}

static void test_defmacro_with_backquote(void){
    e("(defmacro setp (var val) `(setq ,var ,val))");
    e("(setp tm-y 7)");
    TEST_EQ(eql, I(7), e("tm-y"));}

static void test_defmacro_when3_full(void){
    /* Idiomatic CL implementation of WHEN using backquote + ,@. */
    e("(defmacro when3 (test &body body)"
      "  `(if ,test (progn ,@body)))");
    TEST_EQ(eql, I(99), e("(when3 t 1 2 99)"));
    TEST_EQ(eql, NIL_Symbol, e("(when3 nil 1 2 99)"));
    /* Side effects from body. */
    e("(setq tm-c 0)");
    e("(when3 t (setq tm-c (+ tm-c 10)) (setq tm-c (+ tm-c 20)))");
    TEST_EQ(eql, I(30), e("tm-c"));}

static void test_append_builtin(void){
    /* APPEND is the kernel built-in backquote expansion depends on. */
    TEST_EQ(equal,
            list(I(1), I(2), I(3), I(4), I(5), NULL),
            e("(append (list 1 2) (list 3 4) (list 5))"));
    TEST_EQ(eql, NIL_Symbol, e("(append)"));
    TEST_EQ(equal, list(I(1), NULL), e("(append nil (list 1) nil)"));}

static void test_load_lisp_file_with_macro(void){
    /* Write a fixture, load it, then use the macro it defines. This is the
       Phase 5 happy-path: a Lisp file defines and uses macros via LOAD. */
    const char* path = "/tmp/bocl-test-macros.lisp";
    FILE* f = fopen(path, "w");
    fputs("(defmacro double-it (n) `(* ,n 2))\n"
          "(defparameter *loaded* 21)\n"
          "(defparameter *result* (double-it *loaded*))\n", f);
    fclose(f);
    bocl_load_cstring(path);
    TEST_EQ(eql, I(42), e("*result*"));
    unlink(path);}

TEST_INITIALIZE(test_macros);
TEST_SUITE(test_macros, macros_init, macros_clean)
    TEST_CASE( "defmacro basic (twice)",     test_defmacro_basic)
    TEST_CASE( "defmacro &body",             test_defmacro_body_keyword)
    TEST_CASE( "defmacro &rest",             test_defmacro_rest_keyword)
    TEST_CASE( "backquote basic",            test_backquote_basic)
    TEST_CASE( "backquote ,@ splicing",      test_backquote_splicing)
    TEST_CASE( "defmacro with backquote",    test_defmacro_with_backquote)
    TEST_CASE( "WHEN macro round-trip",      test_defmacro_when3_full)
    TEST_CASE( "APPEND built-in",            test_append_builtin)
    TEST_CASE( "load file with macros",      test_load_lisp_file_with_macro)
    TEST_FINALIZE()
