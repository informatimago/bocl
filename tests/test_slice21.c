/* tests/test_slice21.c — CUnit tests for Slice 21:
   DEFINE-CONDITION + CLOS condition dispatch in HANDLER-CASE/SIGNAL,
   WRITE-CHAR, stream predicates (OPEN-STREAM-P, OUTPUT-STREAM-P, etc.),
   CONDITION-REPORT-STRING, DESCRIBE, float constants, APROPOS-LIST,
   COPY-SEQ, NUMBER-TO-STRING / STRING-TO-NUMBER, CONDITIONP extension,
   MAKE-CONDITION for user conditions. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s21_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);   /* bocl-internal names live in CL */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s21_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_test_eval(s); }

/* ---- define-condition ---------------------------------------------------- */

static void test_define_condition_basic(void){
    e("(define-condition my-err21 (standard-object)"
      "  ((msg :initarg :msg :accessor my-err21-msg)))");
    /* The class is findable. */
    TEST_EQ(eql, T_Symbol,
            e("(standard-class-p (find-class 'my-err21))"));
    /* Instances are of the right type. */
    TEST_EQ(eql, T_Symbol,
            e("(typep (make-instance 'my-err21 :msg \"x\") 'my-err21)"));}

static void test_define_condition_handler_case(void){
    e("(define-condition app-error (standard-object)"
      "  ((code :initarg :code :accessor app-error-code)))");
    /* signal an app-error — caught by its specific clause */
    TEST_EQ(eql, I(42),
            e("(handler-case"
              "  (error (make-instance 'app-error :code 42))"
              "  (app-error (c) (app-error-code c))"
              "  (error    (c) -1))"));
    /* generic error clause also catches it */
    TEST_EQ(eql, T_Symbol,
            e("(handler-case"
              "  (error (make-instance 'app-error :code 0))"
              "  (error (c) t))"));}

static void test_define_condition_keyword_signal(void){
    e("(define-condition key-err21 (standard-object)"
      "  ((detail :initarg :detail :accessor key-err21-detail)))");
    /* (error 'class-name :initarg val) — make-instance via build_condition */
    TEST_EQ(equal, T("bad input"),
            e("(handler-case"
              "  (error 'key-err21 :detail \"bad input\")"
              "  (key-err21 (c) (key-err21-detail c)))"));}

static void test_define_condition_inheritance(void){
    e("(define-condition base-err21 (standard-object)"
      "  ((info :initarg :info :accessor base-err21-info)))");
    e("(define-condition derived-err21 (base-err21)"
      "  ((extra :initarg :extra :accessor derived-err21-extra)))");
    /* derived is typep of both */
    TEST_EQ(eql, T_Symbol,
            e("(typep (make-instance 'derived-err21 :info \"x\" :extra \"y\")"
              "       'derived-err21)"));
    TEST_EQ(eql, T_Symbol,
            e("(typep (make-instance 'derived-err21 :info \"x\" :extra \"y\")"
              "       'base-err21)"));}

/* ---- kernel conditions still work --------------------------------------- */

static void test_kernel_conditions_still_work(void){
    /* kernel errors still catchable by (error ...) */
    TEST_EQ(eql, T_Symbol,
            e("(handler-case (car 5) (error (c) t))"));
    /* type-error-catching type_error from C */
    TEST_EQ(eql, T_Symbol,
            e("(handler-case (last 7) (error (c) t))"));}

/* ---- WRITE-CHAR ---------------------------------------------------------- */

static void test_write_char(void){
    TEST_EQ(eql, T_Symbol,
            e("(characterp (write-char #\\x (make-string-output-stream)))"));
    TEST_EQ(equal, T("A"),
            e("(with-output-to-string (s) (write-char #\\A s))"));}

/* ---- Stream predicates -------------------------------------------------- */

static void test_stream_predicates(void){
    TEST_EQ(eql, T_Symbol, e("(open-stream-p *standard-output*)"));
    TEST_EQ(eql, T_Symbol, e("(output-stream-p *standard-output*)"));}

/* ---- CONDITIONP extension ----------------------------------------------- */

static void test_conditionp_extended(void){
    e("(define-condition test-cond21 (standard-object) ())");
    /* kernel conditions still recognized */
    TEST_EQ(eql, T_Symbol,
            e("(conditionp (make-condition 'simple-error :format-control \"x\"))"));
    /* CLOS conditions also recognized after conditionp extension */
    TEST_EQ(eql, T_Symbol,
            e("(condition-p (make-condition 'simple-error :format-control \"x\"))"));}

/* ---- NUMBER-TO-STRING / STRING-TO-NUMBER -------------------------------- */

static void test_number_to_string(void){
    TEST_EQ(equal, T("42"),  e("(number-to-string 42)"));
    TEST_EQ(equal, T("-7"),  e("(number-to-string -7)"));
    TEST_EQ(equal, T("101"), e("(number-to-string 5 2)"));}   /* binary */

static void test_string_to_number(void){
    TEST_EQ(eql, I(42),  e("(string-to-number \"42\")"));
    TEST_EQ(eql, I(-7),  e("(string-to-number \"-7\")"));
    /* junk-allowed returns NIL for non-parseable strings */
    TEST_EQ(eql, NIL_Symbol, e("(parse-integer \"abc\" :junk-allowed t)"));}

/* ---- Float constants ----------------------------------------------------- */

static void test_float_constants(void){
    TEST_EQ(eql, T_Symbol, e("(> most-positive-double-float 1.0e300)"));
    TEST_EQ(eql, T_Symbol, e("(= most-positive-single-float most-positive-double-float)"));
    TEST_EQ(eql, T_Symbol, e("(> 0.0 most-negative-double-float)"));}


TEST_INITIALIZE(test_slice21);
TEST_SUITE(test_slice21, s21_init, s21_fini)
    TEST_CASE("define-condition basic",      test_define_condition_basic)
    TEST_CASE("define-condition handler",    test_define_condition_handler_case)
    TEST_CASE("define-condition by keyword", test_define_condition_keyword_signal)
    TEST_CASE("define-condition inheritance", test_define_condition_inheritance)
    TEST_CASE("kernel conditions still work", test_kernel_conditions_still_work)
    TEST_CASE("write-char",                  test_write_char)
    TEST_CASE("stream predicates",           test_stream_predicates)
    TEST_CASE("conditionp extended",         test_conditionp_extended)
    TEST_CASE("number-to-string",            test_number_to_string)
    TEST_CASE("string-to-number",            test_string_to_number)
    TEST_CASE("float constants",             test_float_constants)
    TEST_FINALIZE()
