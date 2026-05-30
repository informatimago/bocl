/* tests/test_slice25.c — CUnit tests for Slice 25:
   MACHINE-TYPE, SOFTWARE-TYPE, SOFTWARE-VERSION, MACHINE-INSTANCE,
   PARSE-FLOAT, RATIONAL/RATIONALIZE, COMPILE stub, PROVIDE/REQUIRE/*MODULES*,
   STRING-STREAM-P, FILE-STREAM-P, MACRO-FUNCTION, SPECIAL-OPERATOR-P,
   DECLARE in function bodies, WITH-HASH-TABLE-ITERATOR, SHORT-SITE-NAME,
   *LOAD-PATHNAME*, COMPILER-MACRO-FUNCTION. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s25_init){
    kernel_initialize();
    bocl_load_cstring("/Users/pjb/src/public/bocl/bootstrap/prelude.lisp");
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s25_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- Environment query functions ---------------------------------------- */

static void test_machine_type(void){
    /* machine-type returns a string (non-empty) */
    TEST_EQ(eql, T_Symbol,
            e("(and (stringp (machine-type)) (> (length (machine-type)) 0))"));}

static void test_software_type(void){
    TEST_EQ(eql, T_Symbol,
            e("(and (stringp (software-type)) (> (length (software-type)) 0))"));}

static void test_software_version(void){
    TEST_EQ(eql, T_Symbol,
            e("(and (stringp (software-version)) (> (length (software-version)) 0))"));}

static void test_machine_instance(void){
    TEST_EQ(eql, T_Symbol,
            e("(and (stringp (machine-instance)) (>= (length (machine-instance)) 0))"));}

static void test_machine_version(void){
    TEST_EQ(eql, T_Symbol,
            e("(stringp (machine-version))"));}

/* ---- PARSE-FLOAT --------------------------------------------------------- */

static void test_parse_float(void){
    TEST_EQ(eql, T_Symbol,
            e("(floatp (parse-float \"3.14\"))"));
    TEST_EQ(eql, T_Symbol,
            e("(< (abs (- (parse-float \"2.718\") 2.718)) 0.001)"));
    TEST_EQ(eql, T_Symbol,
            e("(floatp (parse-float \"0.0\"))"));
    TEST_EQ(eql, T_Symbol,
            e("(floatp (parse-float \"-1.5\"))"));}

/* ---- RATIONAL / RATIONALIZE ---------------------------------------------- */

static void test_rational(void){
    /* For integers, rational is identity */
    TEST_EQ(eql, I(42),  e("(rational 42)"));
    TEST_EQ(eql, I(-7),  e("(rational -7)"));
    /* For floats, bocl returns the float as-is (no ratio type) */
    TEST_EQ(eql, T_Symbol,
            e("(floatp (rational 3.14))"));}

static void test_rationalize(void){
    TEST_EQ(eql, I(5),    e("(rationalize 5)"));
    TEST_EQ(eql, T_Symbol, e("(realp (rationalize 1.5))"));}

/* ---- COMPILE stub -------------------------------------------------------- */

static void test_compile_stub(void){
    /* compile returns 3 values; the third (failure-p) is NIL (no failure) */
    TEST_EQ(eql, NIL_Symbol,
            e("(nth-value 2 (compile nil (lambda (x) (* x 2))))"));}

/* ---- PROVIDE / REQUIRE / *MODULES* -------------------------------------- */

static void test_provide_require(void){
    /* provide registers the module name */
    TEST_EQ(eql, T_Symbol,
            e("(progn (provide \"test-mod-25\") (member \"test-mod-25\" *modules* :test #'string=) t)"));
    /* require for an already-provided module is a no-op */
    TEST_EQ(eql, T_Symbol,
            e("(progn (provide \"mod-a\") (require \"mod-a\") t)"));}

/* ---- Stream predicates --------------------------------------------------- */

static void test_string_stream_p(void){
    TEST_EQ(eql, T_Symbol,
            e("(string-stream-p (make-string-output-stream))"));
    TEST_EQ(eql, NIL_Symbol,
            e("(string-stream-p *standard-output*)"));}

static void test_file_stream_p(void){
    TEST_EQ(eql, T_Symbol,
            e("(file-stream-p *standard-output*)"));
    TEST_EQ(eql, NIL_Symbol,
            e("(file-stream-p (make-string-output-stream))"));}

/* ---- MACRO-FUNCTION ------------------------------------------------------ */

static void test_macro_function(void){
    /* WHEN is defined as a macro */
    TEST_EQ(eql, T_Symbol,
            e("(if (macro-function 'when) t nil)"));
    /* non-macro symbol returns NIL */
    TEST_EQ(eql, NIL_Symbol,
            e("(macro-function 'car)"));}

/* ---- SPECIAL-OPERATOR-P -------------------------------------------------- */

static void test_special_operator_p(void){
    TEST_EQ(eql, T_Symbol,   e("(special-operator-p 'if)"));
    TEST_EQ(eql, T_Symbol,   e("(special-operator-p 'let)"));
    TEST_EQ(eql, T_Symbol,   e("(special-operator-p 'quote)"));
    TEST_EQ(eql, NIL_Symbol, e("(special-operator-p 'car)"));
    TEST_EQ(eql, NIL_Symbol, e("(special-operator-p 'format)"));}

/* ---- DECLARE in function bodies ----------------------------------------- */

static void test_declare_in_defun(void){
    /* defun returns the symbol name; declare should be silently skipped */
    TEST_EQ(eql, e("'t25-decl-test"),
            e("(defun t25-decl-test (x) (declare (ignore x)) 42)"));
    /* calling the function returns the body value */
    TEST_EQ(eql, I(42),
            e("(t25-decl-test 99)"));}

static void test_declare_in_lambda(void){
    TEST_EQ(eql, I(7),
            e("(funcall (lambda (x y) (declare (ignore y)) x) 7 999)"));}

/* ---- WITH-HASH-TABLE-ITERATOR -------------------------------------------- */

static void test_with_hash_table_iterator(void){
    /* Should be able to iterate over a single-entry hash table */
    TEST_EQ(eql, T_Symbol,
            e("(let ((h (make-hash-table)))"
              "  (sethash 'k h 42)"
              "  (with-hash-table-iterator (nxt h)"
              "    (multiple-value-bind (more k v) (nxt)"
              "      (and more (eq k 'k) (eql v 42)))))"));
    /* Second call returns (nil nil nil) */
    TEST_EQ(eql, NIL_Symbol,
            e("(let ((h (make-hash-table)))"
              "  (sethash 'k h 1)"
              "  (with-hash-table-iterator (nxt h)"
              "    (nxt)"
              "    (nth-value 0 (nxt))  ; second call => more-p = NIL"
              "    ))"));}

/* ---- SHORT-SITE-NAME / LONG-SITE-NAME ------------------------------------ */

static void test_site_names(void){
    /* Both return NIL per stub */
    TEST_EQ(eql, NIL_Symbol, e("(short-site-name)"));
    TEST_EQ(eql, NIL_Symbol, e("(long-site-name)"));}

/* ---- *LOAD-PATHNAME* etc. ------------------------------------------------ */

static void test_load_vars(void){
    TEST_EQ(eql, NIL_Symbol, e("*load-pathname*"));
    TEST_EQ(eql, NIL_Symbol, e("*load-truename*"));}

/* ---- COMPILER-MACRO-FUNCTION ---------------------------------------------- */

static void test_compiler_macro_function(void){
    /* bocl has no compiler macros, so always NIL */
    TEST_EQ(eql, NIL_Symbol,
            e("(compiler-macro-function '+)"));}


TEST_INITIALIZE(test_slice25);
TEST_SUITE(test_slice25, s25_init, s25_fini)
    TEST_CASE("machine-type",              test_machine_type)
    TEST_CASE("software-type",             test_software_type)
    TEST_CASE("software-version",          test_software_version)
    TEST_CASE("machine-instance",          test_machine_instance)
    TEST_CASE("machine-version",           test_machine_version)
    TEST_CASE("parse-float",               test_parse_float)
    TEST_CASE("rational/rationalize",      test_rational)
    TEST_CASE("rationalize",               test_rationalize)
    TEST_CASE("compile stub",              test_compile_stub)
    TEST_CASE("provide/require/*modules*", test_provide_require)
    TEST_CASE("string-stream-p",           test_string_stream_p)
    TEST_CASE("file-stream-p",             test_file_stream_p)
    TEST_CASE("macro-function",            test_macro_function)
    TEST_CASE("special-operator-p",       test_special_operator_p)
    TEST_CASE("declare in defun",          test_declare_in_defun)
    TEST_CASE("declare in lambda",         test_declare_in_lambda)
    TEST_CASE("with-hash-table-iterator",  test_with_hash_table_iterator)
    TEST_CASE("site-names",                test_site_names)
    TEST_CASE("load-pathname vars",        test_load_vars)
    TEST_CASE("compiler-macro-function",   test_compiler_macro_function)
    TEST_FINALIZE()
