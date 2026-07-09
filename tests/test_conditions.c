#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

/* Phase 4.2 condition-system tests. HANDLER-CASE + ERROR + SIGNAL + WARN +
   MAKE-CONDITION + CONDITIONP. HANDLER-BIND / restarts are deferred. */

TEST_SUITE_SETUP(cond_init){
    kernel_initialize();
    bocl_test_load_prelude();   /* uses prelude-level operators (HANDLER-CASE/…) */
    bocl_in_package(COMMON_LISP_Package);   /* expected symbols are CL (symbol_intern_cstring) */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(cond_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){ return bocl_read_eval_cstring(text); }

static void test_handler_case_catches_error(void){
    /* (handler-case (error "boom") (error (c) :got-error))
       returns :got-error (the value of the handler body). */
    Ref r = e("(handler-case (error \"boom\") (error (c) :got-error))");
    TEST_EQ(eql, K(GOT-ERROR), r);}

static void test_handler_case_passes_through(void){
    /* When the form doesn't signal, the form's value is returned. */
    TEST_EQ(eql, I(42), e("(handler-case 42 (error (c) :unreached))"));}

static void test_handler_case_binds_condition(void){
    /* The handler's var is bound to the condition; we can read its
       format-control. */
    Ref r = e("(handler-case (error \"boom\")"
              "  (error (c) (simple-condition-format-control c)))");
    TEST_EQ(equal, T("boom"), r);}

static void test_handler_case_type_dispatch(void){
    /* The clause must match the condition's type. CONDITION matches all,
       ERROR matches errors but not (in CL) plain SIGNAL of CONDITION.
       Here we use SIMPLE-CONDITION to be safe. */
    Ref r = e("(handler-case (signal \"warn\")"
              "  (condition (c) :got-condition))");
    TEST_EQ(eql, K(GOT-CONDITION), r);}

static void test_handler_case_clause_order(void){
    /* Clauses are tried in order; the first matching one fires. */
    Ref r = e("(handler-case (error \"x\")"
              "  (warning   (c) :warning)"
              "  (error     (c) :error)"
              "  (condition (c) :condition))");
    TEST_EQ(eql, K(ERROR), r);}

static void test_signal_returns_nil_when_unhandled(void){
    /* SIGNAL returns NIL if no handler matches. */
    TEST_EQ(eql, NIL_Symbol, e("(signal \"unhandled\")"));}

static void test_make_condition_and_conditionp(void){
    Ref c = e("(make-condition 'simple-error :format-control \"hi\")");
    TEST_EQ(eql, T_Symbol, as_boolean(typep(c, CONDITION_Class)));
    TEST_EQ(eql, T_Symbol,
            e("(typep (make-condition 'simple-error :format-control \"hi\") 'condition)"));
    TEST_EQ(eql, NIL_Symbol, e("(typep 42 'condition)"));
    TEST_EQ(eql, NIL_Symbol, e("(typep nil 'condition)"));}

static void test_signal_existing_condition(void){
    /* SIGNAL/ERROR accept an already-built condition. */
    Ref r = e("(handler-case"
              "  (error (make-condition 'simple-error :format-control \"pre-built\"))"
              "  (error (c) (simple-condition-format-control c)))");
    TEST_EQ(equal, T("pre-built"), r);}

static void test_handler_case_catches_type_error_from_car(void){
    /* type_error() in C built-ins signals SIMPLE-TYPE-ERROR, which is a
       subclass of ERROR.  A (handler-case ... (error ...)) clause must
       intercept it rather than falling through to the C exit path. */
    Ref r = e("(handler-case (car 5) (error (e) 'caught))");
    TEST_EQ(eql, S(CAUGHT), r);}

static void test_handler_case_catches_type_error_from_last(void){
    Ref r = e("(handler-case (last 7) (error (e) 'caught))");
    TEST_EQ(eql, S(CAUGHT), r);}

/* BOCL:OUT-OF-STACK-MEMORY-ERROR class (issue precise-condition-subclasses,
   item 1).  The guard actually SIGNALLING it on deep recursion is verified in the
   full bocl binary (this test harness does not arm the C-stack guard, so real
   recursion here would segfault rather than signal). */
static void test_out_of_stack_memory_error(void){
    /* a STORAGE-CONDITION / BOCL:OUT-OF-MEMORY-ERROR subclass */
    TEST_EQ(eql, T_Symbol,
            e("(subtypep 'bocl:out-of-stack-memory-error 'storage-condition)"));
    TEST_EQ(eql, T_Symbol,
            e("(subtypep 'bocl:out-of-stack-memory-error 'bocl:out-of-memory-error)"));
    /* make-condition carries the limit / used slots */
    TEST_EQ(eql, T_Symbol,
            e("(let ((c (make-condition 'bocl:out-of-stack-memory-error :limit 100 :used 90)))"
              "  (and (typep c 'storage-condition)"
              "       (= 100 (bocl:out-of-memory-limit c))"
              "       (= 90  (bocl:out-of-memory-used c))))"));}

TEST_INITIALIZE(test_conditions);
TEST_SUITE(test_conditions, cond_init, cond_clean)
    TEST_CASE( "out-of-stack-memory-error",  test_out_of_stack_memory_error)
    TEST_CASE( "handler-case catches",      test_handler_case_catches_error)
    TEST_CASE( "handler-case pass-through", test_handler_case_passes_through)
    TEST_CASE( "handler-case binds var",    test_handler_case_binds_condition)
    TEST_CASE( "handler-case type match",   test_handler_case_type_dispatch)
    TEST_CASE( "handler-case clause order", test_handler_case_clause_order)
    TEST_CASE( "signal unhandled -> nil",   test_signal_returns_nil_when_unhandled)
    TEST_CASE( "make-condition + p",        test_make_condition_and_conditionp)
    TEST_CASE( "signal pre-built",          test_signal_existing_condition)
    TEST_CASE( "type-error from car",       test_handler_case_catches_type_error_from_car)
    TEST_CASE( "type-error from last",      test_handler_case_catches_type_error_from_last)
    TEST_FINALIZE()
