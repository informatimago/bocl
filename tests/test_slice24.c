/* tests/test_slice24.c — CUnit tests for Slice 24: KERNEL-FORMAT-RUN
   registration, additional FORMAT directives (~& ~T ~* ~^ ~F ~G ~@D),
   and FORMAT destination=T (output to *standard-output*).

   The core FORMAT mechanism was already implemented in printer.c;
   Slice 24 registers KERNEL-FORMAT-RUN in the Lisp symbol table so that
   the Lisp-level FORMAT function can call it. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s24_init){
    kernel_initialize();
    bocl_test_load_prelude();
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s24_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- kernel-format-run is registered ------------------------------------ */

static void test_kfr_registered(void){
    /* fboundp should return T for kernel-format-run */
    TEST_EQ(eql, T_Symbol,
            e("(fboundp 'kernel-format-run)"));}

/* ---- format nil returns a string ---------------------------------------- */

static void test_format_nil_returns_string(void){
    TEST_EQ(eql, T_Symbol,
            e("(stringp (format nil \"hello ~A\" 42))"));
    TEST_EQ(equal, e("\"hello 42\""),
            e("(format nil \"hello ~A\" 42)"));}

/* ---- format t returns nil ----------------------------------------------- */

static void test_format_t_returns_nil(void){
    /* (format t ...) writes to *standard-output* and returns NIL. */
    TEST_EQ(eql, NIL_Symbol,
            e("(format t \"~A\" 1)"));}

/* ---- ~& fresh-line -------------------------------------------------------- */

static void test_format_fresh_line(void){
    /* ~& emits a newline (simplified: always emits one newline) */
    TEST_EQ(eql, I(1),
            e("(string-length (format nil \"~&\"))"));
    TEST_EQ(eql, I(10),
            e("(char-code (char (format nil \"~&\") 0))"));
    /* ~2& gives two newlines */
    TEST_EQ(eql, I(2),
            e("(string-length (format nil \"~2&\"))"));}

/* ---- ~T tab/spaces ------------------------------------------------------- */

static void test_format_tab(void){
    /* ~T emits 1 space by default */
    TEST_EQ(equal, e("\" \""),
            e("(format nil \"~T\")"));
    /* ~4T emits 4 spaces */
    TEST_EQ(equal, e("\"    \""),
            e("(format nil \"~4T\")"));
    /* Embedded in a string */
    TEST_EQ(equal, e("\"a  b\""),
            e("(format nil \"a~2Tb\")"));}

/* ---- ~* skip args ------------------------------------------------------- */

static void test_format_skip(void){
    /* ~* skips 1 arg */
    TEST_EQ(equal, e("\"2\""),
            e("(format nil \"~*~D\" 1 2)"));
    /* ~2* skips 2 args */
    TEST_EQ(equal, e("\"3\""),
            e("(format nil \"~2*~D\" 1 2 3)"));}

/* ---- ~^ escape upward in ~{} -------------------------------------------- */

static void test_format_escape(void){
    /* ~^ inside ~{} stops iteration when args run out.
       Each sub-iteration gets one element from the list; ~^ exits when done. */
    TEST_EQ(equal, e("\"1, 2, 3\""),
            e("(format nil \"~{~A~^, ~}\" '(1 2 3))"));}

/* ---- ~F / ~G float output ------------------------------------------------ */

static void test_format_float(void){
    /* ~F (simplified) falls through to princ — just verify no error */
    TEST_EQ(eql, T_Symbol,
            e("(stringp (format nil \"~F\" 3.14))"));
    TEST_EQ(eql, T_Symbol,
            e("(stringp (format nil \"~G\" 2.71828))"));
    /* Float zero */
    TEST_EQ(eql, T_Symbol,
            e("(stringp (format nil \"~F\" 0.0))"));}

/* ---- ~@D sign prefix ----------------------------------------------------- */

static void test_format_signed_decimal(void){
    TEST_EQ(equal, e("\"+42\""), e("(format nil \"~@D\" 42)"));
    TEST_EQ(equal, e("\"-7\""),  e("(format nil \"~@D\" -7)"));
    TEST_EQ(equal, e("\"+0\""),  e("(format nil \"~@D\" 0)"));}

/* ---- ~B binary ----------------------------------------------------------- */

static void test_format_binary(void){
    TEST_EQ(equal, e("\"1010\""), e("(format nil \"~B\" 10)"));
    TEST_EQ(equal, e("\"0\""),    e("(format nil \"~B\" 0)"));
    TEST_EQ(equal, e("\"11111111\""), e("(format nil \"~B\" 255)"));}

/* ---- ~nR radix ----------------------------------------------------------- */

static void test_format_radix(void){
    TEST_EQ(equal, e("\"10\""),   e("(format nil \"~8R\" 8)"));
    TEST_EQ(equal, e("\"1010\""), e("(format nil \"~2R\" 10)"));
    TEST_EQ(equal, e("\"A\""),    e("(format nil \"~16R\" 10)"));}

/* ---- Error signalling through format ------------------------------------ */

static void test_error_uses_format(void){
    /* error uses format internally; verify it does not crash with ~A */
    TEST_EQ(eql, T_Symbol,
            e("(handler-case (error \"value is ~A\" 42)"
              "  (error (c)"
              "    (and (stringp (princ-to-string c)) t)))"));}


TEST_INITIALIZE(test_slice24);
TEST_SUITE(test_slice24, s24_init, s24_fini)
    TEST_CASE("kernel-format-run registered",    test_kfr_registered)
    TEST_CASE("format nil returns string",       test_format_nil_returns_string)
    TEST_CASE("format t returns nil",            test_format_t_returns_nil)
    TEST_CASE("format ~&",                       test_format_fresh_line)
    TEST_CASE("format ~T",                       test_format_tab)
    TEST_CASE("format ~*",                       test_format_skip)
    TEST_CASE("format ~^",                       test_format_escape)
    TEST_CASE("format ~F/~G",                    test_format_float)
    TEST_CASE("format ~@D",                      test_format_signed_decimal)
    TEST_CASE("format ~B",                       test_format_binary)
    TEST_CASE("format ~nR",                      test_format_radix)
    TEST_CASE("error uses format",               test_error_uses_format)
    TEST_FINALIZE()
