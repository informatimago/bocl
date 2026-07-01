/* tests/test_slice26.c — CUnit tests for Slice 26:
   STRING-UPCASE/DOWNCASE/CAPITALIZE with :start/:end,
   CHAR-WHITESPACE-P / CHAR-ALPHABETIC-P / CHAR-NUMERIC-P / CHAR-INT,
   CHAR-UPPER-CASE-P / CHAR-LOWER-CASE-P,
   REMOVE-DUPLICATES for all sequence types,
   FORMAT ~[~;~] conditional, FORMAT ~? indirect,
   PRINC keyword fix (colon prefix), STANDARD-CHAR-P / GRAPHIC-CHAR-P,
   variadic char comparisons. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s26_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);   /* bocl-internal names live in CL */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s26_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- STRING-UPCASE with :start/:end ------------------------------------- */

static void test_string_upcase(void){
    TEST_EQ(equal, e("\"HELLO\""), e("(string-upcase \"hello\")"));
    /* Partial: only chars [1,3) upcased */
    TEST_EQ(equal, e("\"hELlo\""), e("(string-upcase \"hello\" :start 1 :end 3)"));
    /* :end omitted means to end of string */
    TEST_EQ(equal, e("\"hELLO\""), e("(string-upcase \"hello\" :start 1)"));
    /* Works on symbols too (via STRING coercion) */
    TEST_EQ(eql, T_Symbol,
            e("(stringp (string-upcase 'foo))"));}

static void test_string_downcase(void){
    TEST_EQ(equal, e("\"hello\""), e("(string-downcase \"HELLO\")"));
    /* :start 1 :end 3 downcases positions 1 and 2 only (E→e, L→l) */
    TEST_EQ(equal, e("\"HelLO\""),
            e("(string-downcase \"HELLO\" :start 1 :end 3)"));}

static void test_string_capitalize(void){
    TEST_EQ(equal, e("\"Hello World\""),
            e("(string-capitalize \"hello world\")"));
    TEST_EQ(equal, e("\"hEllo world\""),
            e("(string-capitalize \"hello world\" :start 1 :end 5)"));}

/* ---- Character predicates ----------------------------------------------- */

static void test_char_whitespace_p(void){
    TEST_EQ(eql, T_Symbol,   e("(char-whitespace-p #\\Space)"));
    TEST_EQ(eql, T_Symbol,   e("(char-whitespace-p #\\Tab)"));
    /* bocl #\Newline has char-code 133 (0x85/NEL); check whitespace-p accepts it */
    TEST_EQ(eql, T_Symbol,   e("(char-whitespace-p (code-char 133))"));
    TEST_EQ(eql, NIL_Symbol, e("(char-whitespace-p #\\A)"));
    TEST_EQ(eql, NIL_Symbol, e("(char-whitespace-p #\\0)"));}

static void test_char_case_predicates(void){
    TEST_EQ(eql, T_Symbol,   e("(char-upper-case-p #\\A)"));
    TEST_EQ(eql, NIL_Symbol, e("(char-upper-case-p #\\a)"));
    TEST_EQ(eql, T_Symbol,   e("(char-lower-case-p #\\a)"));
    TEST_EQ(eql, NIL_Symbol, e("(char-lower-case-p #\\A)"));
    TEST_EQ(eql, NIL_Symbol, e("(char-upper-case-p #\\1)"));
    TEST_EQ(eql, NIL_Symbol, e("(char-lower-case-p #\\1)"));}

static void test_char_alpha_numeric(void){
    TEST_EQ(eql, T_Symbol,   e("(char-alphabetic-p #\\A)"));
    TEST_EQ(eql, T_Symbol,   e("(char-alphabetic-p #\\z)"));
    TEST_EQ(eql, NIL_Symbol, e("(char-alphabetic-p #\\1)"));
    TEST_EQ(eql, T_Symbol,   e("(char-numeric-p #\\5)"));
    TEST_EQ(eql, NIL_Symbol, e("(char-numeric-p #\\A)"));
    TEST_EQ(eql, I(65),      e("(char-int #\\A)"));}

static void test_standard_char_p(void){
    TEST_EQ(eql, T_Symbol,   e("(standard-char-p #\\A)"));
    TEST_EQ(eql, T_Symbol,   e("(standard-char-p #\\Space)"));
    /* bocl #\Newline = code 133; standard-char-p accepts it */
    TEST_EQ(eql, T_Symbol,   e("(standard-char-p (code-char 133))"));
    /* CLHS 13.1.4.3: #\Space IS a graphic character. */
    TEST_EQ(eql, T_Symbol,   e("(graphic-char-p #\\Space)"));
    TEST_EQ(eql, T_Symbol,   e("(graphic-char-p #\\A)"));}

/* ---- PRINC keyword fix --------------------------------------------------- */

static void test_princ_keyword(void){
    /* PRINC is aesthetic (*print-escape* nil): a keyword prints as its name with
       no package marker, so :FOO -> "FOO" (PRIN1 would give ":FOO"). */
    TEST_EQ(equal, e("\"FOO\""),
            e("(with-output-to-string (s) (princ :foo s))"));
    /* prin1 on keyword also includes colon */
    TEST_EQ(equal, e("\":FOO\""),
            e("(with-output-to-string (s) (prin1 :foo s))"));}

/* ---- REMOVE-DUPLICATES --------------------------------------------------- */

static void test_remove_duplicates(void){
    /* lists */
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(remove-duplicates '(1 2 3))"));
    TEST_EQ(equal, e("'(1 3 2)"),
            e("(remove-duplicates '(1 2 3 2))"));
    /* keeps last occurrence — earlier duplicates removed */
    TEST_EQ(equal, e("'(3 2 1)"),
            e("(remove-duplicates '(1 2 3 2 1) :test #'eql)"));}

/* ---- FORMAT ~[~;~] conditional ------------------------------------------ */

static void test_format_conditional(void){
    /* ~[c0~;c1~;c2~] — select by integer */
    TEST_EQ(equal, e("\"zero\""),
            e("(format nil \"~[zero~;one~;two~]\" 0)"));
    TEST_EQ(equal, e("\"one\""),
            e("(format nil \"~[zero~;one~;two~]\" 1)"));
    TEST_EQ(equal, e("\"two\""),
            e("(format nil \"~[zero~;one~;two~]\" 2)"));
    /* ~:[false~;true~] — boolean */
    TEST_EQ(equal, e("\"yes\""),
            e("(format nil \"~:[no~;yes~]\" t)"));
    TEST_EQ(equal, e("\"no\""),
            e("(format nil \"~:[no~;yes~]\" nil)"));}

static void test_format_indirect(void){
    /* ~? — indirect formatting */
    TEST_EQ(equal, e("\"1+2=3\""),
            e("(format nil \"~?\" \"~D+~D=~D\" '(1 2 3))"));}

/* ---- Variadic char comparisons ------------------------------------------ */

static void test_char_comparisons(void){
    TEST_EQ(eql, T_Symbol,   e("(char= #\\a #\\a)"));
    TEST_EQ(eql, T_Symbol,   e("(char= #\\a #\\a #\\a)"));
    TEST_EQ(eql, NIL_Symbol, e("(char= #\\a #\\b)"));
    TEST_EQ(eql, T_Symbol,   e("(char< #\\a #\\b #\\c)"));
    TEST_EQ(eql, NIL_Symbol, e("(char< #\\a #\\a)"));}


TEST_INITIALIZE(test_slice26);
TEST_SUITE(test_slice26, s26_init, s26_fini)
    TEST_CASE("string-upcase :start/:end",   test_string_upcase)
    TEST_CASE("string-downcase :start/:end", test_string_downcase)
    TEST_CASE("string-capitalize :start/:end", test_string_capitalize)
    TEST_CASE("char-whitespace-p",           test_char_whitespace_p)
    TEST_CASE("char case predicates",        test_char_case_predicates)
    TEST_CASE("char-alphabetic-p/numeric-p", test_char_alpha_numeric)
    TEST_CASE("standard/graphic-char-p",     test_standard_char_p)
    TEST_CASE("princ keyword colon",         test_princ_keyword)
    TEST_CASE("remove-duplicates",           test_remove_duplicates)
    TEST_CASE("format ~[conditional]",       test_format_conditional)
    TEST_CASE("format ~? indirect",          test_format_indirect)
    TEST_CASE("char comparisons variadic",   test_char_comparisons)
    TEST_FINALIZE()
