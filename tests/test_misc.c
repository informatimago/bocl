/* tests/test_misc.c — CUnit tests for Slice 14 miscellaneous utilities:
   MAKE-ARRAY, array ops, COPY-SEQ, MAKE-LIST, RANDOM, file-system,
   timing constants, FILL, REPLACE, MISMATCH, and write-to-string fix. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "simple_test.h"
#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(misc_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);   /* bocl-internal names live in CL */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(misc_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- MAKE-ARRAY -------------------------------------------------------- */
static void test_make_array(void){
    /* default fill is NIL */
    TEST_EQ(equal, e("3"),
            e("(let ((v (make-array 3))) (vector-length v))"));
    /* :initial-element */
    TEST_EQ(equal, e("'(0 0 0)"),
            e("(let* ((v (make-array 3 :initial-element 0)))"
              "  (list (svref v 0) (svref v 1) (svref v 2)))"));
    /* :initial-contents */
    TEST_EQ(equal, e("'(a b c)"),
            e("(let* ((v (make-array 3 :initial-contents '(a b c))))"
              "  (list (svref v 0) (svref v 1) (svref v 2)))"));
    /* empty array */
    TEST_EQ(equal, e("0"),
            e("(vector-length (make-array 0))"));}

/* ---- ARRAY-DIMENSIONS / ARRAY-TOTAL-SIZE / ARRAY-RANK ----------------- */
static void test_array_info(void){
    TEST_EQ(equal, e("'(4)"),
            e("(array-dimensions (make-array 4))"));
    TEST_EQ(equal, e("4"),
            e("(array-total-size (make-array 4))"));
    TEST_EQ(equal, e("1"),
            e("(array-rank (make-array 4))"));}

/* ---- ARRAY-IN-BOUNDS-P ------------------------------------------------ */
static void test_array_in_bounds(void){
    TEST_EQ(equal, e("t"),
            e("(array-in-bounds-p (make-array 5) 0)"));
    TEST_EQ(equal, e("t"),
            e("(array-in-bounds-p (make-array 5) 4)"));
    TEST_EQ(equal, e("nil"),
            e("(array-in-bounds-p (make-array 5) 5)"));
    TEST_EQ(equal, e("nil"),
            e("(array-in-bounds-p (make-array 5) -1)"));}

/* ---- ADJUSTABLE-ARRAY-P / ARRAY-HAS-FILL-POINTER-P ------------------- */
static void test_array_predicates(void){
    TEST_EQ(equal, e("nil"),
            e("(adjustable-array-p (make-array 3))"));
    TEST_EQ(equal, e("nil"),
            e("(array-has-fill-pointer-p (make-array 3))"));
    TEST_EQ(equal, e("t"),
            e("(simple-vector-p (vector 1 2 3))"));
    TEST_EQ(equal, e("nil"),
            e("(simple-vector-p '(1 2 3))"));}

/* ---- MAKE-LIST -------------------------------------------------------- */
static void test_make_list(void){
    TEST_EQ(equal, e("'(nil nil nil)"),
            e("(make-list 3)"));
    TEST_EQ(equal, e("'(0 0 0 0)"),
            e("(make-list 4 :initial-element 0)"));
    TEST_EQ(equal, e("nil"),
            e("(make-list 0)"));}

/* ---- COPY-SEQ --------------------------------------------------------- */
static void test_copy_seq(void){
    /* list — check that modifying the copy doesn't affect the original */
    TEST_EQ(equal, e("'(1 99)"),
            e("(let* ((x (list 1 2 3)) (y (copy-seq x)))"
              "  (rplaca y 99) (list (car x) (car y)))"));
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(copy-seq '(1 2 3))"));
    /* vector */
    TEST_EQ(equal, e("3"),
            e("(vector-length (copy-seq (vector 10 20 30)))"));
    /* string */
    TEST_EQ(equal, e("\"hello\""),
            e("(copy-seq \"hello\")"));}

/* ---- RANDOM ----------------------------------------------------------- */
static void test_random(void){
    /* Result must be in [0, limit) */
    TEST_EQ(equal, e("t"),
            e("(let ((r (random 10))) (and (>= r 0) (< r 10)))"));
    /* Float limit */
    TEST_EQ(equal, e("t"),
            e("(let ((r (random 1.0))) (and (>= r 0.0) (< r 1.0))))"));
    /* Make-random-state returns T (our sentinel) */
    TEST_EQ(equal, e("t"),
            e("(make-random-state)"));}

/* ---- PROBE-FILE ------------------------------------------------------- */
static void test_probe_file(void){
    /* /dev/null always exists */
    TEST_EQ(equal, e("t"),
            e("(if (probe-file \"/dev/null\") t nil)"));
    /* Non-existent path */
    TEST_EQ(equal, e("nil"),
            e("(probe-file \"/bocl_no_such_file_should_exist_12345\")"));}

/* ---- DELETE-FILE / RENAME-FILE --------------------------------------- */
static void test_file_ops(void){
    /* Create a temp file, probe, rename, probe, delete, probe. */
    TEST_EQ(equal, e("t"),
            e("(progn"
              "  (with-open-file (f \"/tmp/bocl_test_misc_1.tmp\" :direction :output"
              "                      :if-exists :supersede)"
              "    (write-string \"hello\" f))"
              "  (if (probe-file \"/tmp/bocl_test_misc_1.tmp\") t nil))"));
    TEST_EQ(equal, e("t"),
            e("(progn"
              "  (rename-file \"/tmp/bocl_test_misc_1.tmp\""
              "               \"/tmp/bocl_test_misc_2.tmp\")"
              "  (if (probe-file \"/tmp/bocl_test_misc_2.tmp\") t nil))"));
    TEST_EQ(equal, e("nil"),
            e("(probe-file \"/tmp/bocl_test_misc_1.tmp\")"));
    /* delete-file returns T; probe-file after deletion returns NIL */
    TEST_EQ(equal, e("t"), e("(delete-file \"/tmp/bocl_test_misc_2.tmp\")"));
    TEST_EQ(equal, e("nil"),
            e("(probe-file \"/tmp/bocl_test_misc_2.tmp\")"));}

/* ---- TIMING ----------------------------------------------------------- */
static void test_timing(void){
    /* internal-time-units-per-second should be 1000000 */
    TEST_EQ(equal, e("1000000"),
            e("internal-time-units-per-second"));
    /* get-universal-time returns an integer > 3900000000 (past year 2023) */
    TEST_EQ(equal, e("t"),
            e("(> (get-universal-time) 3900000000)"));
    /* get-internal-real-time returns a positive integer */
    TEST_EQ(equal, e("t"),
            e("(> (get-internal-real-time) 0)"));
    /* get-internal-run-time returns a positive integer */
    TEST_EQ(equal, e("t"),
            e("(> (get-internal-run-time) 0)"));}

/* ---- FILL ------------------------------------------------------------- */
static void test_fill(void){
    TEST_EQ(equal, e("'(0 0 0)"),
            e("(let ((x (list 1 2 3))) (fill x 0))"));
    TEST_EQ(equal, e("'(1 0 0)"),
            e("(let ((x (list 1 2 3))) (fill x 0 :start 1) x)"));
    TEST_EQ(equal, e("'(1 0 3)"),
            e("(let ((x (list 1 2 3))) (fill x 0 :start 1 :end 2) x)"));}

/* ---- REPLACE ---------------------------------------------------------- */
static void test_replace(void){
    TEST_EQ(equal, e("'(4 5 6)"),
            e("(let ((x (list 1 2 3))) (replace x '(4 5 6)))"));
    TEST_EQ(equal, e("'(1 4 5)"),
            e("(let ((x (list 1 2 3))) (replace x '(4 5 6) :start1 1) x)"));
    TEST_EQ(equal, e("'(1 4 3)"),
            e("(let ((x (list 1 2 3))) (replace x '(4 5 6) :start1 1 :end1 2) x)"));}

/* ---- MISMATCH --------------------------------------------------------- */
static void test_mismatch(void){
    TEST_EQ(equal, e("nil"),
            e("(mismatch '(1 2 3) '(1 2 3))"));
    TEST_EQ(equal, e("2"),
            e("(mismatch '(1 2 3) '(1 2 4))"));
    TEST_EQ(equal, e("0"),
            e("(mismatch '(1 2 3) '(9 2 3))"));
    /* length mismatch */
    TEST_EQ(equal, e("3"),
            e("(mismatch '(1 2 3) '(1 2 3 4))"));}

/* ---- WRITE-TO-STRING (should honour :escape) -------------------------- */
static void test_write_to_string(void){
    /* Default: :escape t  → printed with prin1 quoting */
    TEST_EQ(equal, e("\"\\\"hello\\\"\""),
            e("(write-to-string \"hello\")"));
    /* :escape nil → princ style */
    TEST_EQ(equal, e("\"hello\""),
            e("(write-to-string \"hello\" :escape nil)"));}

/* Profiling interface (issue hot-1-profile).  The default build has
   -DBOCL_PROFILE OFF, so ENABLED-P is NIL and the entry points are no-ops. */
static void test_profile_interface(void){
    TEST_EQ(eql, NIL_Symbol, e("(bocl:profile-enabled-p)"));   /* off in default build */
    /* WITH-PROFILE-PHASE runs BODY and returns its value even when off */
    TEST_EQ(eql, I(42), e("(bocl:with-profile-phase :macroexpand 42)"));
    TEST_EQ(eql, I(3),  e("(bocl:with-profile-phase :eval (+ 1 2))"));
    /* RESET / REPORT don't error (report to a string stream) */
    TEST_EQ(eql, NIL_Symbol, e("(bocl:profile-reset)"));
    TEST_EQ(eql, T_Symbol,
            e("(progn (with-output-to-string (s) (bocl:profile-report s)) t)"));}

TEST_INITIALIZE(test_misc);
TEST_SUITE(test_misc, misc_init, misc_fini)
    TEST_CASE("profile interface",   test_profile_interface)
    TEST_CASE("make-array",          test_make_array)
    TEST_CASE("array-info",          test_array_info)
    TEST_CASE("array-in-bounds-p",   test_array_in_bounds)
    TEST_CASE("array-predicates",    test_array_predicates)
    TEST_CASE("make-list",           test_make_list)
    TEST_CASE("copy-seq",            test_copy_seq)
    TEST_CASE("random",              test_random)
    TEST_CASE("probe-file",          test_probe_file)
    TEST_CASE("file-ops",            test_file_ops)
    TEST_CASE("timing",              test_timing)
    TEST_CASE("fill",                test_fill)
    TEST_CASE("replace",             test_replace)
    TEST_CASE("mismatch",            test_mismatch)
    TEST_CASE("write-to-string",     test_write_to_string)
    TEST_FINALIZE()
