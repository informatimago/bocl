/* tests/test_conformance.c — CLHS conformance fixes from the
   asdf-loading-cesarum issue: reader conditionals in dotted tails,
   PROBE-FILE/TRUENAME pathnames, DEFINE-MODIFY-MACRO, DEFSTRUCT
   docstrings, *COMPILE-FILE-PATHNAME*, non-keyword method qualifiers,
   short-form method combinations, DEFPACKAGE option ordering. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(conf_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(conf_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_test_eval(s); }

/* ---- reader conditionals in a dotted tail (asdf.asd idiom) -------------- */
static void test_reader_dotted_conditionals(void){
    /* feature ABSENT: #-x keeps its form as the tail, #+x consumed+discarded */
    TEST_EQ(equal, e("'(1 2)"),
            e("(read-from-string \"(1 2 . #-nofeat () #+nofeat (3 4))\")"));
    /* feature PRESENT: #-x suppressed (NOT an object), #+x form is the tail */
    TEST_EQ(equal, e("'(1 2 3 4)"),
            e("(progn (pushnew :conf-feat *features*)"
              "  (read-from-string \"(1 2 . #-conf-feat () #+conf-feat (3 4))\"))"));
    /* element position unchanged */
    TEST_EQ(equal, e("'(1 2)"), e("(read-from-string \"(1 2 #+nofeat 3)\")"));
    TEST_EQ(equal, e("'(1 2 3)"), e("(read-from-string \"(1 2 #-nofeat 3)\")"));}

/* ---- PROBE-FILE / TRUENAME return pathnames ----------------------------- */
static void test_probe_file_pathname(void){
    TEST_EQ(eql, T_Symbol, e("(pathnamep (probe-file \"/dev/null\"))"));
    TEST_EQ(eql, T_Symbol, e("(pathnamep (truename \"/dev/null\"))"));
    TEST_EQ(eql, NIL_Symbol, e("(probe-file \"/bocl-no-such-file-zz\")"));}

/* ---- DEFINE-MODIFY-MACRO: &rest, &optional, single evaluation ----------- */
static void test_define_modify_macro(void){
    e("(define-modify-macro conf-appf (&rest lists) append)");
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(let ((x (list 1))) (conf-appf x (list 2) (list 3)) x)"));
    e("(define-modify-macro conf-inc (&optional (delta 1)) +)");
    TEST_EQ(eql, I(16), e("(let ((n 5)) (conf-inc n) (conf-inc n 10) n)"));
    /* CLHS: the place's subforms are evaluated exactly once */
    TEST_EQ(equal, e("'((5 6) 0)"),
            e("(let ((i -1) (v (vector nil)))"
              "  (conf-appf (aref v (incf i)) (list 5 6))"
              "  (list (aref v 0) i))"));}

/* ---- DEFSTRUCT accepts a documentation string --------------------------- */
static void test_defstruct_docstring(void){
    TEST_EQ(eql, I(3),
            e("(progn (defstruct conf-ds \"Doc string.\" x) "
              "(conf-ds-x (make-conf-ds :x 3)))"));
    TEST_EQ(eql, T_Symbol,
            e("(progn (defstruct conf-ds2 \"Only a doc.\") "
              "(conf-ds2-p (make-conf-ds2)))"));}

/* ---- COMPILE-FILE binds *COMPILE-FILE-PATHNAME* ------------------------- */
static void test_compile_file_pathname_bound(void){
    e("(with-open-file (o \"{TMP}/conf-cfp.lisp\" :direction :output"
      "                   :if-exists :supersede)"
      "  (print '(eval-when (:compile-toplevel)"
      "            (setf (symbol-value 'cl-user::*conf-cfp-seen*)"
      "                  (and *compile-file-pathname* t))) o))");
    e("(compile-file \"{TMP}/conf-cfp.lisp\")");
    TEST_EQ(eql, T_Symbol, e("cl-user::*conf-cfp-seen*"));
    e("(ignore-errors (delete-file \"{TMP}/conf-cfp.lisp\"))");
    e("(ignore-errors (delete-file (compile-file-pathname \"{TMP}/conf-cfp.lisp\")))");}

/* ---- non-keyword qualifiers + short-form method combinations ------------ */
static void test_method_combinations(void){
    e("(defclass conf-a () ())");
    e("(defclass conf-b (conf-a) ())");
    e("(defgeneric conf-slots (o)"
      "  (:method-combination append)"
      "  (:method append ((o conf-a)) (list :a 1))"
      "  (:method append ((o conf-b)) (list :b 2)))");
    /* most-specific first, results appended */
    TEST_EQ(equal, e("'(:b 2 :a 1)"), e("(conf-slots (make-instance 'conf-b))"));
    TEST_EQ(equal, e("'(:a 1)"),      e("(conf-slots (make-instance 'conf-a))"));
    e("(defgeneric conf-tot (o)"
      "  (:method + ((o conf-a)) 10)"
      "  (:method + ((o conf-b)) 32))");
    TEST_EQ(eql, I(42), e("(conf-tot (make-instance 'conf-b))"));
    /* standard combination unaffected */
    e("(defgeneric conf-std (o))");
    e("(defmethod conf-std ((o conf-a)) :base)");
    e("(defmethod conf-std :around ((o conf-b)) (list :around (call-next-method)))");
    TEST_EQ(equal, e("'(:around :base)"), e("(conf-std (make-instance 'conf-b))"));}

/* ---- DEFPACKAGE: shadowing imports before :use -------------------------- */
static void test_defpackage_shadowing_order(void){
    /* Using a package that exports a CL-colliding name works when the
       collision is resolved by :shadowing-import-from — which DEFPACKAGE
       must process BEFORE establishing :use inheritance. */
    e("(defpackage \"CONF-SET\" (:use) (:export \"UNION\" \"MERGE\"))");
    TEST_EQ(equal, e("\"CONF-SET\""),
            e("(progn (defpackage \"CONF-IX\""
              "         (:use \"COMMON-LISP\" \"CONF-SET\")"
              "         (:shadowing-import-from \"CONF-SET\" \"UNION\" \"MERGE\"))"
              "  (package-name (symbol-package (find-symbol \"UNION\" \"CONF-IX\"))))"));}

TEST_INITIALIZE(test_conformance);
TEST_SUITE(test_conformance, conf_init, conf_fini)
    TEST_CASE("reader dotted conditionals", test_reader_dotted_conditionals)
    TEST_CASE("probe-file pathname",        test_probe_file_pathname)
    TEST_CASE("define-modify-macro",        test_define_modify_macro)
    TEST_CASE("defstruct docstring",        test_defstruct_docstring)
    TEST_CASE("compile-file-pathname",      test_compile_file_pathname_bound)
    TEST_CASE("method combinations",        test_method_combinations)
    TEST_CASE("defpackage shadowing order", test_defpackage_shadowing_order)
    TEST_FINALIZE()
