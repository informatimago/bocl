/* tests/test_slice15.c — CUnit tests for Slice 15:
   TYPE-OF, AREF/AREF-SET, CHAR/SCHAR, SLEEP, LISTEN, CLEAR-INPUT,
   DECODE/ENCODE-UNIVERSAL-TIME, WITH-OPEN-STREAM, SUBTYPEP,
   MAP, MAPL, NBUTLAST, LDIFF, GENTEMP, MULTIPLE-VALUE-LIST, NTH-VALUE. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s15_init){
    kernel_initialize();
    bocl_test_load_prelude();
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s15_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- TYPE-OF ---------------------------------------------------------- */

static void test_type_of(void){
    /* bocl type-of returns proper CL sexp type specifiers for numbers,
       and simple symbols for other types. */
    TEST_EQ(equal, S(INTEGER),      e("(first (type-of 42))"));
    TEST_EQ(equal, e("'double-float"), e("(type-of 1.0)"));
    TEST_EQ(equal, e("'simple-string"), e("(type-of \"hello\")"));
    TEST_EQ(equal, e("'symbol"),    e("(type-of 'foo)"));
    TEST_EQ(equal, e("'cons"),      e("(type-of '(1 2))"));
    TEST_EQ(equal, e("'null"),      e("(type-of nil)"));
    TEST_EQ(equal, e("'character"), e("(type-of #\\A)"));}

/* ---- AREF / (SETF AREF) ----------------------------------------------- */

static void test_aref(void){
    TEST_EQ(equal, e("2"),
            e("(let ((v (make-array 3 :initial-contents '(1 2 3))))"
              "  (aref v 1))"));
    /* setf aref */
    TEST_EQ(equal, e("99"),
            e("(let ((v (make-array 3 :initial-element 0)))"
              "  (setf (aref v 1) 99)"
              "  (aref v 1))"));
    /* first and last */
    TEST_EQ(equal, e("'(10 30)"),
            e("(let ((v (make-array 3 :initial-contents '(10 20 30))))"
              "  (list (aref v 0) (aref v 2)))")); }

/* ---- CHAR / SCHAR / (SETF CHAR) --------------------------------------- */

static void test_char(void){
    TEST_EQ(eql, C(h), e("(char \"hello\" 0)"));
    TEST_EQ(eql, C(o), e("(char \"hello\" 4)"));
    TEST_EQ(eql, C(e), e("(schar \"hello\" 1)"));
    /* setf char */
    TEST_EQ(equal, e("\"Hello\""),
            e("(let ((s (copy-seq \"hello\")))"
              "  (setf (char s 0) #\\H)"
              "  s)"));}

/* ---- MULTIPLE-VALUE-LIST / NTH-VALUE ---------------------------------- */

static void test_multiple_value_list(void){
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(multiple-value-list (values 1 2 3))"));
    TEST_EQ(equal, e("'(42)"),
            e("(multiple-value-list 42)"));}

static void test_nth_value(void){
    TEST_EQ(eql, I(1), e("(nth-value 0 (values 1 2 3))"));
    TEST_EQ(eql, I(2), e("(nth-value 1 (values 1 2 3))"));
    TEST_EQ(eql, I(3), e("(nth-value 2 (values 1 2 3))"));}

/* ---- SUBTYPEP --------------------------------------------------------- */

static void test_subtypep(void){
    /* identical → T,T */
    TEST_EQ(eql, T_Symbol, e("(nth-value 0 (subtypep 'integer 'integer))"));
    TEST_EQ(eql, T_Symbol, e("(nth-value 1 (subtypep 'integer 'integer))"));
    /* integer ⊆ number */
    TEST_EQ(eql, T_Symbol, e("(nth-value 0 (subtypep 'integer 'number))"));
    TEST_EQ(eql, T_Symbol, e("(nth-value 1 (subtypep 'integer 'number))"));
    /* float ⊆ number */
    TEST_EQ(eql, T_Symbol, e("(nth-value 0 (subtypep 'float 'number))"));
    /* cons ⊆ list */
    TEST_EQ(eql, T_Symbol, e("(nth-value 0 (subtypep 'cons 'list))"));
    /* nil ⊆ everything */
    TEST_EQ(eql, T_Symbol, e("(nth-value 0 (subtypep 'nil 't))"));
    /* X ⊆ T always */
    TEST_EQ(eql, T_Symbol, e("(nth-value 0 (subtypep 'string 't))"));
    /* string ⊄ number → NIL, T */
    TEST_EQ(equal, e("nil"), e("(nth-value 0 (subtypep 'string 'number))"));
    TEST_EQ(eql, T_Symbol,   e("(nth-value 1 (subtypep 'string 'number))"));}

/* ---- MAP -------------------------------------------------------------- */

static void test_map(void){
    /* list result */
    TEST_EQ(equal, e("'(2 4 6)"),
            e("(map 'list #'(lambda (x) (* x 2)) '(1 2 3))"));
    /* nil result — side effects only */
    TEST_EQ(equal, e("nil"),
            e("(map nil #'identity '(1 2 3))"));
    /* vector result */
    TEST_EQ(equal, e("3"),
            e("(vector-length (map 'vector #'identity '(a b c)))"));
    /* string result — char-upcase each char */
    TEST_EQ(equal, e("\"ABC\""),
            e("(map 'string #'char-upcase \"abc\")"));}

/* ---- MAPL ------------------------------------------------------------- */

static void test_mapl(void){
    /* Returns original first list (eq) */
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(let ((lst '(1 2 3))) (mapl #'car lst))"));
    /* Side effects: accumulate sublists */
    TEST_EQ(equal, e("'((1 2 3) (2 3) (3))"),
            e("(let ((acc nil))"
              "  (mapl (lambda (tail) (push tail acc)) '(1 2 3))"
              "  (nreverse acc))")); }

/* ---- NBUTLAST --------------------------------------------------------- */

static void test_nbutlast(void){
    TEST_EQ(equal, e("'(1 2)"),  e("(nbutlast (list 1 2 3))"));
    TEST_EQ(equal, e("'(1)"),    e("(nbutlast (list 1 2 3) 2)"));
    TEST_EQ(equal, e("nil"),     e("(nbutlast (list 1 2 3) 3)"));
    TEST_EQ(equal, e("nil"),     e("(nbutlast (list 1 2 3) 99)"));
    /* One element — drop 1 → NIL */
    TEST_EQ(equal, e("nil"),     e("(nbutlast (list 42))")); }

/* ---- LDIFF ------------------------------------------------------------ */

static void test_ldiff(void){
    TEST_EQ(equal, e("'(1 2)"),
            e("(let* ((lst '(1 2 3 4))"
              "       (tail (cddr lst)))"
              "  (ldiff lst tail))"));
    /* object not a tail → whole list copy */
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(ldiff '(1 2 3) nil)"));
    /* object is the whole list → NIL */
    TEST_EQ(equal, e("nil"),
            e("(let ((lst '(1 2 3))) (ldiff lst lst))")); }

/* ---- GENTEMP ---------------------------------------------------------- */

static void test_gentemp(void){
    /* gentemp returns a symbol */
    TEST_EQ(equal, e("t"),
            e("(symbolp (gentemp))"));
    /* Two calls return distinct symbols */
    TEST_EQ(equal, e("t"),
            e("(not (eq (gentemp) (gentemp)))"));
    /* Custom prefix — check first three chars individually using eql */
    TEST_EQ(equal, e("t"),
            e("(let* ((sym (gentemp \"FOO\"))"
              "       (nm  (symbol-name sym)))"
              "  (and (symbolp sym)"
              "       (> (string-length nm) 3)"
              "       (eql (char nm 0) #\\F)"
              "       (eql (char nm 1) #\\O)"
              "       (eql (char nm 2) #\\O)))"));}

/* ---- WITH-OPEN-STREAM ------------------------------------------------- */

static void test_with_open_stream(void){
    /* Write to a string stream and recover the string. */
    TEST_EQ(equal, e("\"hello world\""),
            e("(with-open-stream (s (make-string-output-stream))"
              "  (write-string \"hello\" s)"
              "  (write-string \" world\" s)"
              "  (get-output-stream-string s))"));}

/* ---- SLEEP ------------------------------------------------------------ */

static void test_sleep(void){
    /* sleep 0 should return NIL without error. */
    TEST_EQ(equal, e("nil"), e("(sleep 0)"));}

/* ---- DECODE-UNIVERSAL-TIME / ENCODE-UNIVERSAL-TIME -------------------- */

static void test_decode_universal_time(void){
    /* 2000-01-01 00:00:00 UTC = universal time 3155673600.
       Decode with TZ=0 should give (0 0 0 1 1 2000 ...). */
    TEST_EQ(eql, I(0),    e("(nth-value 0 (decode-universal-time 3155673600 0))"));
    TEST_EQ(eql, I(0),    e("(nth-value 1 (decode-universal-time 3155673600 0))"));
    TEST_EQ(eql, I(0),    e("(nth-value 2 (decode-universal-time 3155673600 0))"));
    TEST_EQ(eql, I(1),    e("(nth-value 3 (decode-universal-time 3155673600 0))"));
    TEST_EQ(eql, I(1),    e("(nth-value 4 (decode-universal-time 3155673600 0))"));
    TEST_EQ(eql, I(2000), e("(nth-value 5 (decode-universal-time 3155673600 0))"));}

static void test_encode_universal_time(void){
    /* Roundtrip: encode then decode. */
    TEST_EQ(eql, I(3155673600),
            e("(encode-universal-time 0 0 0 1 1 2000 0)"));
    /* encode→decode roundtrip */
    TEST_EQ(eql, T_Symbol,
            e("(let* ((ut (encode-universal-time 30 15 12 25 12 2023 0))"
              "       (sec (nth-value 0 (decode-universal-time ut 0)))"
              "       (min (nth-value 1 (decode-universal-time ut 0)))"
              "       (hr  (nth-value 2 (decode-universal-time ut 0)))"
              "       (day (nth-value 3 (decode-universal-time ut 0)))"
              "       (mon (nth-value 4 (decode-universal-time ut 0)))"
              "       (yr  (nth-value 5 (decode-universal-time ut 0))))"
              "  (and (= sec 30) (= min 15) (= hr 12)"
              "       (= day 25) (= mon 12) (= yr 2023)))"));}

TEST_INITIALIZE(test_slice15);
TEST_SUITE(test_slice15, s15_init, s15_fini)
    TEST_CASE("type-of",                    test_type_of)
    TEST_CASE("aref/aref-set",             test_aref)
    TEST_CASE("char/schar",                test_char)
    TEST_CASE("multiple-value-list",        test_multiple_value_list)
    TEST_CASE("nth-value",                  test_nth_value)
    TEST_CASE("subtypep",                   test_subtypep)
    TEST_CASE("map",                        test_map)
    TEST_CASE("mapl",                       test_mapl)
    TEST_CASE("nbutlast",                   test_nbutlast)
    TEST_CASE("ldiff",                      test_ldiff)
    TEST_CASE("gentemp",                    test_gentemp)
    TEST_CASE("with-open-stream",           test_with_open_stream)
    TEST_CASE("sleep",                      test_sleep)
    TEST_CASE("decode-universal-time",      test_decode_universal_time)
    TEST_CASE("encode-universal-time",      test_encode_universal_time)
    TEST_FINALIZE()
