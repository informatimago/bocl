/* tests/test_slice23.c — CUnit tests for Slice 23:
   MAKE-LIST, AREF/AREF-SET (via make-array :initial-contents),
   GET-UNIVERSAL-TIME, GET-INTERNAL-REAL-TIME, GET-INTERNAL-RUN-TIME,
   WRITE-STRING, SLEEP, CLEAR-INPUT, RANDOM, MAKE-RANDOM-STATE,
   DECODE-UNIVERSAL-TIME, ENCODE-UNIVERSAL-TIME, NTHCDR. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s23_init){
    kernel_initialize();
    bocl_test_load_prelude();
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s23_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- MAKE-LIST ---------------------------------------------------------- */

static void test_make_list(void){
    TEST_EQ(equal, e("'(nil nil nil)"),  e("(make-list 3)"));
    TEST_EQ(equal, e("'(0 0 0 0)"),      e("(make-list 4 :initial-element 0)"));
    TEST_EQ(equal, NIL_Symbol,           e("(make-list 0)"));
    TEST_EQ(eql,   I(5),                 e("(length (make-list 5))")); }

/* ---- AREF / (SETF AREF) with :initial-contents ------------------------- */

static void test_aref_initial_contents(void){
    TEST_EQ(equal, e("2"),
            e("(aref (make-array 3 :initial-contents '(1 2 3)) 1)"));
    TEST_EQ(equal, e("'(10 20 30)"),
            e("(let ((v (make-array 3 :initial-contents '(10 20 30))))"
              "  (list (aref v 0) (aref v 1) (aref v 2)))"));
    /* setf aref */
    TEST_EQ(equal, e("99"),
            e("(let ((v (make-array 3 :initial-element 0)))"
              "  (setf (aref v 2) 99)"
              "  (aref v 2))"));}

/* ---- GET-UNIVERSAL-TIME ------------------------------------------------- */

static void test_get_universal_time(void){
    /* Universal time > 3900000000 (after year 2023) */
    TEST_EQ(eql, T_Symbol,
            e("(> (get-universal-time) 3900000000)"));}

/* ---- GET-INTERNAL-REAL-TIME / GET-INTERNAL-RUN-TIME ------------------- */

static void test_get_internal_time(void){
    TEST_EQ(eql, T_Symbol,
            e("(> (get-internal-real-time) 0)"));
    TEST_EQ(eql, T_Symbol,
            e("(> (get-internal-run-time) 0)"));
    TEST_EQ(eql, I(1000000),
            e("internal-time-units-per-second"));}

/* ---- WRITE-STRING ------------------------------------------------------- */

static void test_write_string(void){
    TEST_EQ(equal, T("hello"),
            e("(with-output-to-string (s) (write-string \"hello\" s))"));
    TEST_EQ(equal, T("hello world"),
            e("(with-output-to-string (s)"
              "  (write-string \"hello\" s)"
              "  (write-string \" world\" s))"));}

/* ---- SLEEP -------------------------------------------------------------- */

static void test_sleep(void){
    /* sleep 0 returns NIL without error */
    TEST_EQ(eql, NIL_Symbol, e("(sleep 0)"));
    TEST_EQ(eql, NIL_Symbol, e("(sleep 0.0)"));}

/* ---- RANDOM / MAKE-RANDOM-STATE ---------------------------------------- */

static void test_random(void){
    /* Result in [0, limit) */
    TEST_EQ(eql, T_Symbol,
            e("(let ((r (random 10))) (and (>= r 0) (< r 10)))"));
    /* Float limit */
    TEST_EQ(eql, T_Symbol,
            e("(let ((r (random 1.0))) (and (>= r 0.0) (< r 1.0))))"));
    /* MAKE-RANDOM-STATE returns T (our global PRNG sentinel) */
    TEST_EQ(eql, T_Symbol, e("(make-random-state)"));}

/* ---- DECODE-UNIVERSAL-TIME / ENCODE-UNIVERSAL-TIME ------------------- */

static void test_decode_encode(void){
    /* 2000-01-01 00:00:00 UTC = 3155673600 */
    TEST_EQ(eql, I(0),    e("(nth-value 0 (decode-universal-time 3155673600 0))"));
    TEST_EQ(eql, I(0),    e("(nth-value 1 (decode-universal-time 3155673600 0))"));
    TEST_EQ(eql, I(0),    e("(nth-value 2 (decode-universal-time 3155673600 0))"));
    TEST_EQ(eql, I(1),    e("(nth-value 3 (decode-universal-time 3155673600 0))"));
    TEST_EQ(eql, I(1),    e("(nth-value 4 (decode-universal-time 3155673600 0))"));
    TEST_EQ(eql, I(2000), e("(nth-value 5 (decode-universal-time 3155673600 0))"));
    /* encode roundtrip */
    TEST_EQ(eql, I(3155673600),
            e("(encode-universal-time 0 0 0 1 1 2000 0)"));}

/* ---- NTHCDR ------------------------------------------------------------- */

static void test_nthcdr(void){
    TEST_EQ(equal, e("'(3 4)"),  e("(nthcdr 2 '(1 2 3 4))"));
    TEST_EQ(equal, e("'(1 2)"),  e("(nthcdr 0 '(1 2))"));
    TEST_EQ(equal, NIL_Symbol,   e("(nthcdr 5 '(1 2 3))"));}


TEST_INITIALIZE(test_slice23);
TEST_SUITE(test_slice23, s23_init, s23_fini)
    TEST_CASE("make-list",              test_make_list)
    TEST_CASE("aref+initial-contents",  test_aref_initial_contents)
    TEST_CASE("get-universal-time",     test_get_universal_time)
    TEST_CASE("get-internal-time",      test_get_internal_time)
    TEST_CASE("write-string",           test_write_string)
    TEST_CASE("sleep",                  test_sleep)
    TEST_CASE("random",                 test_random)
    TEST_CASE("decode/encode-time",     test_decode_encode)
    TEST_CASE("nthcdr",                 test_nthcdr)
    TEST_FINALIZE()
