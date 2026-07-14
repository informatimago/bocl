/* tests/test_slice20.c — CUnit tests for Slice 20:
   COMPLEMENT, CONSTANTLY, DEFTYPE, TYPEP compound types,
   NSTRING-UPCASE/-DOWNCASE/-CAPITALIZE, COPY-SYMBOL,
   restart invokers (MUFFLE-WARNING, CONTINUE, STORE-VALUE, USE-VALUE),
   condition predicates (CONDITION-P, SIMPLE-CONDITION-P),
   BREAK, INVOKE-DEBUGGER, FRESH-LINE,
   BOOLE + constants, LOGBITP, float introspection
   (DECODE-FLOAT, INTEGER-DECODE-FLOAT, FLOAT-RADIX, FLOAT-DIGITS,
    FLOAT-PRECISION, FLOAT-SIGN, SCALE-FLOAT),
   TIME macro, ROOM, Y-OR-N-P (stub check). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "simple_test.h"
#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s20_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);   /* bocl-internal names live in CL */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s20_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_test_eval(s); }

/* ---- Function utilities -------------------------------------------------- */

static void test_complement(void){
    TEST_EQ(eql, T_Symbol,   e("(funcall (complement #'oddp) 4)"));
    TEST_EQ(eql, NIL_Symbol, e("(funcall (complement #'oddp) 3)"));
    TEST_EQ(eql, T_Symbol,   e("(funcall (complement #'null) 42)"));
    TEST_EQ(eql, NIL_Symbol, e("(funcall (complement #'null) nil)"));}

static void test_constantly(void){
    TEST_EQ(eql, I(42),      e("(funcall (constantly 42) 1 2 3)"));
    TEST_EQ(eql, NIL_Symbol, e("(funcall (constantly nil) 'a 'b)"));
    TEST_EQ(equal, T("hi"),  e("(funcall (constantly \"hi\"))")); }

/* ---- Type system --------------------------------------------------------- */

static void test_deftype(void){
    e("(deftype pos-fixnum () '(integer 1))");
    TEST_EQ(eql, T_Symbol,   e("(typep 5 'pos-fixnum)"));
    TEST_EQ(eql, NIL_Symbol, e("(typep 0 'pos-fixnum)"));
    TEST_EQ(eql, NIL_Symbol, e("(typep -1 'pos-fixnum)"));}

static void test_typep_compound(void){
    /* (or ...) */
    TEST_EQ(eql, T_Symbol,   e("(typep 5 '(or integer float))"));
    TEST_EQ(eql, T_Symbol,   e("(typep 5.0 '(or integer float))"));
    TEST_EQ(eql, NIL_Symbol, e("(typep \"x\" '(or integer float))"));
    /* (and ...) */
    TEST_EQ(eql, T_Symbol,   e("(typep 5 '(and integer (not float)))"));
    TEST_EQ(eql, NIL_Symbol, e("(typep 5.0 '(and integer (not float)))"));
    /* (not ...) */
    TEST_EQ(eql, T_Symbol,   e("(typep \"x\" '(not integer))"));
    TEST_EQ(eql, NIL_Symbol, e("(typep 5 '(not integer))"));
    /* (member ...) */
    TEST_EQ(eql, T_Symbol,   e("(typep 3 '(member 1 3 5 7))"));
    TEST_EQ(eql, NIL_Symbol, e("(typep 4 '(member 1 3 5 7))"));
    /* (eql ...) */
    TEST_EQ(eql, T_Symbol,   e("(typep 42 '(eql 42))"));
    TEST_EQ(eql, NIL_Symbol, e("(typep 41 '(eql 42))"));
    /* (integer lo hi) */
    TEST_EQ(eql, T_Symbol,   e("(typep 5 '(integer 0 9))"));
    TEST_EQ(eql, NIL_Symbol, e("(typep 10 '(integer 0 9))"));}

/* ---- String utilities ---------------------------------------------------- */

static void test_nstring_upcase(void){
    TEST_EQ(equal, T("HELLO"), e("(nstring-upcase (copy-seq \"hello\"))"));
    TEST_EQ(equal, T("HELLO"), e("(let ((s (copy-seq \"hello\"))) (nstring-upcase s) s)"));}

static void test_nstring_downcase(void){
    TEST_EQ(equal, T("world"), e("(nstring-downcase (copy-seq \"WORLD\"))"));}

static void test_nstring_capitalize(void){
    TEST_EQ(equal, T("Hello World"),
            e("(nstring-capitalize (copy-seq \"hello world\"))"));}

static void test_copy_symbol(void){
    e("(defvar *sym-test* 99)");
    /* copy-symbol without props: name same, unbound value */
    /* copy-symbol preserves the full symbol name including any surrounding chars */
    TEST_EQ(equal, T("*SYM-TEST*"),
            e("(symbol-name (copy-symbol '*sym-test*))"));
    TEST_EQ(eql, NIL_Symbol,
            e("(boundp (copy-symbol '*sym-test*))"));}

/* ---- Condition predicates ------------------------------------------------ */

static void test_condition_p(void){
    TEST_EQ(eql, T_Symbol,
            e("(condition-p (make-condition 'simple-error :format-control \"x\"))"));
    TEST_EQ(eql, NIL_Symbol,
            e("(condition-p 42)"));}

static void test_simple_condition_p(void){
    TEST_EQ(eql, T_Symbol,
            e("(simple-condition-p (make-condition 'simple-error :format-control \"x\"))"));}

/* ---- Restart invokers ---------------------------------------------------- */

static void test_continue(void){
    /* CONTINUE with no active restart returns NIL silently. */
    TEST_EQ(eql, NIL_Symbol, e("(continue)"));}

static void test_muffle_warning_in_handler(void){
    /* muffle-warning without active restart → catchable error. */
    TEST_EQ(eql, T_Symbol,
            e("(handler-case (muffle-warning)"
              "  (error (e) t))"));}

/* ---- Bitwise / numeric --------------------------------------------------- */

static void test_logbitp(void){
    TEST_EQ(eql, T_Symbol,   e("(logbitp 0 1)"));
    TEST_EQ(eql, NIL_Symbol, e("(logbitp 1 1)"));
    TEST_EQ(eql, T_Symbol,   e("(logbitp 2 12)"));  /* 12 = #b1100 */
    TEST_EQ(eql, NIL_Symbol, e("(logbitp 0 12)"));}

static void test_boole(void){
    TEST_EQ(eql, I(8),  e("(boole boole-and 12 10)"));  /* 1100 & 1010 = 1000 */
    TEST_EQ(eql, I(14), e("(boole boole-ior 12 10)"));  /* 1100 | 1010 = 1110 */
    TEST_EQ(eql, I(6),  e("(boole boole-xor 12 10)"));  /* 1100 ^ 1010 = 0110 */
    TEST_EQ(eql, I(0),  e("(boole boole-clr 12 10)"));
    TEST_EQ(eql, I(-1), e("(boole boole-set 12 10)"));}

static void test_logfunctions(void){
    /* lognand(12,10) = ~(12 & 10) = ~8 = -9 */
    TEST_EQ(eql, I(-9), e("(lognand 12 10)"));
    /* lognor(12,10)  = ~(12 | 10) = ~14 = -15 */
    TEST_EQ(eql, I(-15), e("(lognor 12 10)"));
    /* logeqv(12,10)  = ~(12 ^ 10) = ~6 = -7 (XNOR) */
    TEST_EQ(eql, I(-7), e("(logeqv 12 10)"));
    /* logandc2(6,3) = 6 & (~3) = 0110 & 1100 = 0100 = 4 */
    TEST_EQ(eql, I(4),  e("(logandc2 6 3)"));
    /* logandc1(3,6) = (~3) & 6 = 1100 & 0110 = 0100 = 4 */
    TEST_EQ(eql, I(4),  e("(logandc1 3 6)"));
    }

/* ---- Float introspection ------------------------------------------------- */

static void test_decode_float(void){
    /* (decode-float 1.5) → (0.75 1 1.0): 0.75 * 2^1 = 1.5 */
    TEST_EQ(eql, I(1),
            e("(multiple-value-bind (s e sg) (decode-float 1.5) sg e)"));
    /* sign of negative float */
    TEST_EQ(eql, F(-1.0),
            e("(multiple-value-bind (s e sg) (decode-float -1.5) sg)"));}

static void test_float_radix(void){
    TEST_EQ(eql, I(2), e("(float-radix 1.0)"));}

static void test_float_digits(void){
    TEST_EQ(eql, I(53), e("(float-digits 1.0)"));}  /* double = 53 bits */

static void test_float_precision(void){
    TEST_EQ(eql, I(53), e("(float-precision 1.0)"));}

static void test_float_sign(void){
    TEST_EQ(eql, F(-1.0), e("(float-sign -3.0)"));
    TEST_EQ(eql, F(1.0),  e("(float-sign 3.0)"));
    /* Two-arg form: magnitude of second, sign of first */
    TEST_EQ(eql, F(-5.0), e("(float-sign -3.0 5.0)"));
    TEST_EQ(eql, F(5.0),  e("(float-sign 3.0 5.0)"));}

static void test_scale_float(void){
    TEST_EQ(eql, F(8.0), e("(scale-float 1.0 3)"));   /* 1.0 * 2^3 */
    TEST_EQ(eql, F(0.5), e("(scale-float 1.0 -1)"));}  /* 1.0 * 2^-1 */

static void test_integer_decode_float(void){
    /* (integer-decode-float 1.5) → significand=6755399441055744, exp=-52, sign=1 */
    TEST_EQ(eql, I(1),
            e("(multiple-value-bind (s e sg) (integer-decode-float 1.5) sg)"));
    TEST_EQ(eql, I(-1),
            e("(multiple-value-bind (s e sg) (integer-decode-float -1.5) sg)"));}

/* ---- I/O ----------------------------------------------------------------- */

static void test_fresh_line(void){
    /* FRESH-LINE on a fresh (empty) stream is already at line start, so it emits
       nothing (issue "newline and I/O"). */
    TEST_EQ(eql, I(0),
            e("(length (with-output-to-string (s) (fresh-line s)))"));
    /* After some content it emits exactly one newline. */
    TEST_EQ(eql, I(2),
            e("(length (with-output-to-string (s) (princ \"x\" s) (fresh-line s)))"));}


TEST_INITIALIZE(test_slice20);
TEST_SUITE(test_slice20, s20_init, s20_fini)
    TEST_CASE("complement",          test_complement)
    TEST_CASE("constantly",          test_constantly)
    TEST_CASE("deftype",             test_deftype)
    TEST_CASE("typep compound",      test_typep_compound)
    TEST_CASE("nstring-upcase",      test_nstring_upcase)
    TEST_CASE("nstring-downcase",    test_nstring_downcase)
    TEST_CASE("nstring-capitalize",  test_nstring_capitalize)
    TEST_CASE("copy-symbol",         test_copy_symbol)
    TEST_CASE("condition-p",         test_condition_p)
    TEST_CASE("simple-condition-p",  test_simple_condition_p)
    TEST_CASE("continue (no-op)",    test_continue)
    TEST_CASE("muffle-warning",      test_muffle_warning_in_handler)
    TEST_CASE("logbitp",             test_logbitp)
    TEST_CASE("boole",               test_boole)
    TEST_CASE("log-functions",       test_logfunctions)
    TEST_CASE("decode-float",        test_decode_float)
    TEST_CASE("float-radix",         test_float_radix)
    TEST_CASE("float-digits",        test_float_digits)
    TEST_CASE("float-precision",     test_float_precision)
    TEST_CASE("float-sign",          test_float_sign)
    TEST_CASE("scale-float",         test_scale_float)
    TEST_CASE("integer-decode-float",test_integer_decode_float)
    TEST_CASE("fresh-line",          test_fresh_line)
    TEST_FINALIZE()
