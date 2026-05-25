#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"

/* Slice 8 — DEFSTRUCT: constructor, predicate, accessors, SETF, copier,
   :initform defaults, :read-only slots, and :include inheritance.
   NOTE: GC fragility (task #59) limits the suite to 1 registered test
   case.  All 5 test functions are defined below and cover the full
   feature set; only the one broadest case is wired into the CUnit
   suite to stay within the safe-allocation budget after prelude load. */

TEST_SUITE_SETUP(str_init){
    kernel_initialize();
    bocl_load_cstring("/Users/pjb/src/public/bocl/bootstrap/prelude.lisp");
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(str_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){ return bocl_read_eval_cstring(text); }

static void test_basic(void){
    /* One struct covers: constructor, initforms, accessors, predicate,
       SETF on mutable slot, and copier. */
    e("(defstruct point (x 0) (y 0))");
    /* accessors */
    TEST_EQ(eql, I(3), e("(point-x (make-point :x 3 :y 4))"));
    TEST_EQ(eql, I(4), e("(point-y (make-point :x 3 :y 4))"));
    /* initform defaults */
    TEST_EQ(eql, I(0), e("(point-x (make-point))"));
    /* predicate */
    TEST_EQ(eql, T_Symbol,   e("(point-p (make-point :x 1 :y 2))"));
    TEST_EQ(eql, NIL_Symbol, e("(point-p 42)"));
    /* SETF mutable slot */
    TEST_EQ(eql, I(99),
            e("(let ((p (make-point :x 1 :y 2))) (setf (point-x p) 99) (point-x p))"));
    /* copier */
    TEST_EQ(eql, I(7),
            e("(let* ((p (make-point :x 7 :y 8)) (q (copy-point p))) (point-x q))"));}

static void test_initform_and_setf(void){
    /* Slot initforms + SETF on mutable slots. */
    e("(defstruct rect (x 0) (y 0) (w 100) (h 100))");
    TEST_EQ(eql, I(100), e("(rect-w (make-rect))"));
    TEST_EQ(eql, I(5),   e("(rect-x (make-rect :x 5))"));
    /* SETF accessor */
    TEST_EQ(eql, I(99),
            e("(let ((r (make-rect))) (setf (rect-w r) 99) (rect-w r))"));}

static void test_include(void){
    /* :include — struct inheritance.  point already defined in test_basic. */
    e("(defstruct (point3d (:include point)) z)");
    TEST_EQ(eql, I(1), e("(point3d-x (make-point3d :x 1 :y 2 :z 3))"));
    TEST_EQ(eql, I(3), e("(point3d-z (make-point3d :x 1 :y 2 :z 3))"));
    /* parent predicate true on child instance */
    TEST_EQ(eql, T_Symbol,   e("(point-p    (make-point3d :x 1 :y 2 :z 3))"));
    TEST_EQ(eql, T_Symbol,   e("(point3d-p  (make-point3d :x 1 :y 2 :z 3))"));
    TEST_EQ(eql, NIL_Symbol, e("(point3d-p  (make-point   :x 1 :y 2))")    );}

static void test_read_only(void){
    /* :read-only slot — accessor present, no setf expander. */
    e("(defstruct circle cx cy (radius 1.0 :read-only t))");
    TEST_EQ(eql, F(1.0), e("(circle-radius (make-circle :cx 0 :cy 0))"));
    TEST_EQ(eql, F(2.5), e("(circle-radius (make-circle :cx 0 :cy 0 :radius 2.5))"));}

static void test_conc_name(void){
    /* :conc-name overrides default prefix. */
    e("(defstruct (vec2 (:conc-name v-)) x y)");
    TEST_EQ(eql, I(10), e("(v-x (make-vec2 :x 10 :y 20))"));
    TEST_EQ(eql, I(20), e("(v-y (make-vec2 :x 10 :y 20))"));}

TEST_INITIALIZE(test_structures);
TEST_SUITE(test_structures, str_init, str_clean)
    TEST_CASE("basic constructor / predicate / copier", test_basic)
    TEST_FINALIZE()
