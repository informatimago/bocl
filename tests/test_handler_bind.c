#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"

/* Slice 9a — HANDLER-BIND: handler called in the dynamic context of the
   signal (no stack unwind), multiple handlers, outer HANDLER-CASE fallback,
   and non-local exit from handler.
   NOTE: does NOT load prelude.lisp — all test forms run at the global
   environment level so the GC budget is not strained. */

TEST_SUITE_SETUP(hb_init){
    kernel_initialize();
    bocl_test_load_prelude();   /* uses prelude-level operators (DEFPARAMETER/HANDLER-BIND/…) */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(hb_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){ return bocl_read_eval_cstring(text); }

/* handler-bind: handler is called, if it returns signaling continues to
   the outer handler-case. */
static void test_handler_bind_calls_handler(void){
    /* The handler stores a side effect; the signal is then caught by the
       outer handler-case which returns :caught.  We verify both happened. */
    e("(defparameter *hb-log* nil)");
    Ref r = e("(handler-case"
              "  (handler-bind ((error (lambda (c)"
              "                    (setq *hb-log* :saw-it))))"
              "    (error \"boom\"))"
              "  (error (c) :caught))");
    TEST_EQ(eql, K(CAUGHT),  r);
    TEST_EQ(eql, K(SAW-IT), e("*hb-log*"));}

/* handler-bind: handler receives the condition object. */
static void test_handler_bind_receives_condition(void){
    e("(defparameter *hb-cond* nil)");
    e("(handler-case"
      "  (handler-bind ((error (lambda (c) (setq *hb-cond* c))))"
      "    (error \"test-msg\"))"
      "  (error (c) nil))");
    TEST_EQ(eql, T_Symbol,
            e("(typep *hb-cond* 'condition)"));}

/* handler-bind: non-local exit from the handler prevents outer handler-case
   from running; instead the block named 'escape captures the exit. */
static void test_handler_bind_nonlocal_exit(void){
    Ref r = e("(block escape"
              "  (handler-bind ((error (lambda (c)"
              "                    (return-from escape :escaped))))"
              "    (error \"trigger\")))");
    TEST_EQ(eql, K(ESCAPED), r);}

/* handler-bind: multiple bindings — first matching one fires. */
static void test_handler_bind_multiple_bindings(void){
    e("(defparameter *hb-which* nil)");
    e("(handler-case"
      "  (handler-bind"
      "    ((warning (lambda (c) (setq *hb-which* :warning)))"
      "     (error   (lambda (c) (setq *hb-which* :error))))"
      "    (error \"e\"))"
      "  (error (c) nil))");
    TEST_EQ(eql, K(ERROR), e("*hb-which*"));}

/* handler-bind: body returns normally when no signal is raised. */
static void test_handler_bind_no_signal(void){
    TEST_EQ(eql, I(42),
            e("(handler-bind ((error (lambda (c) :unreached)))"
              "  42)"));}

/* handler-bind: nested binds — inner fires first, then outer.
   Both handlers run and record themselves; the list order proves inner-first. */
static void test_handler_bind_ordering(void){
    e("(defparameter *hb-order* nil)");
    e("(handler-case"
      "  (handler-bind ((error (lambda (c)"
      "                    (setq *hb-order* (cons :outer *hb-order*)))))"
      "    (handler-bind ((error (lambda (c)"
      "                    (setq *hb-order* (cons :inner *hb-order*)))))"
      "      (error \"x\")))"
      "  (error (c) nil))");
    /* inner fires first → cons :inner → (:inner)
       outer fires next  → cons :outer → (:outer :inner) */
    TEST_EQ(equal, list(K(OUTER), K(INNER), NULL), e("*hb-order*"));}

TEST_INITIALIZE(test_handler_bind);
TEST_SUITE(test_handler_bind, hb_init, hb_clean)
    TEST_CASE("handler called, signal continues",  test_handler_bind_calls_handler)
    TEST_CASE("handler receives condition",         test_handler_bind_receives_condition)
    TEST_CASE("non-local exit from handler",        test_handler_bind_nonlocal_exit)
    TEST_CASE("multiple bindings — first match",   test_handler_bind_multiple_bindings)
    TEST_CASE("no signal → normal return",         test_handler_bind_no_signal)
    TEST_CASE("inner fires first, outer fires too", test_handler_bind_ordering)
    TEST_FINALIZE()
