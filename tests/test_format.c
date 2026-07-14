/* tests/test_format.c — CUnit tests for Slice 10: FORMAT, WRITE-STRING,
   READ-FROM-STRING, WITH-OUTPUT-TO-STRING, PRINC-TO-STRING, WRITE-TO-STRING. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(fmt_init){
    kernel_initialize();
    bocl_test_load_prelude();
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(fmt_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){ return bocl_test_eval(text); }

/* --- Literal and ~A ------------------------------------------------------ */

static void test_format_literal(void){
    TEST_EQ(equal, e("\"hello\""), e("(format nil \"hello\")"));}

static void test_format_tilde_A(void){
    TEST_EQ(equal, e("\"42\""),    e("(format nil \"~A\" 42)"));
    TEST_EQ(equal, e("\"hello\""), e("(format nil \"~A\" \"hello\")"));
    TEST_EQ(equal, e("\"NIL\""),   e("(format nil \"~A\" nil)"));}

/* --- ~S produces PRIN1 output ------------------------------------------- */

static void test_format_tilde_S(void){
    /* Compare against prin1-to-string — no need to build the expected string
       by hand (which requires awkward C/bocl escape double-nesting). */
    TEST_EQ(eql, T_Symbol,
            e("(equal (format nil \"~S\" \"hello\") (prin1-to-string \"hello\"))"));
    TEST_EQ(eql, T_Symbol,
            e("(equal (format nil \"~S\" 42) \"42\")"));
    TEST_EQ(eql, T_Symbol,
            e("(equal (format nil \"~S\" nil) \"NIL\")"));}

/* --- ~% newline ---------------------------------------------------------- */

static void test_format_tilde_percent(void){
    /* (format nil "~%") should be a 1-char string whose char is #\Newline.
       bocl's #\Newline is NEL (code 133), so ~% to a string yields code 133
       (issue "newline and I/O"; to a *device* it translates to LF/CR/CRLF). */
    TEST_EQ(eql, I(1),   e("(length (format nil \"~%\"))"));
    TEST_EQ(eql, I(133), e("(char-code (char (format nil \"~%\") 0))"));
    /* ~2% gives two newlines. */
    TEST_EQ(eql, I(2),   e("(length (format nil \"~2%\"))"));}

/* --- #\Newline representation (issue "newline and I/O") ------------------ */

static void test_format_newline(void){
    /* The newline #\Newline (NEL, 133) is the internal line separator; ~%,
       TERPRI and a literal newline in a string all denote it. */
    TEST_EQ(eql, T_Symbol,
            e("(= (char-code #\\newline) (char-code (character (format nil \"~%\"))))"));
    TEST_EQ(eql, T_Symbol,
            e("(char= #\\newline (character (format nil \"~%\")))"));
    /* TERPRI to a string denotes #\Newline (133) as well. */
    TEST_EQ(eql, I(133),
            e("(char-code (char (with-output-to-string (s) (terpri s)) 0))"));
    /* ~& does not add a newline right after a ~% newline (fresh-line). */
    TEST_EQ(eql, T_Symbol,
            e("(string= (format nil \"a:~%~&b~%\") (format nil \"a:~%b~%\"))"));}

/* --- ~~ literal tilde --------------------------------------------------- */

static void test_format_tilde_tilde(void){
    TEST_EQ(equal, e("\"~\""),   e("(format nil \"~~\")"));
    TEST_EQ(equal, e("\"~~~\""), e("(format nil \"~3~\")"));}

/* --- ~D decimal ---------------------------------------------------------- */

static void test_format_tilde_D(void){
    TEST_EQ(equal, e("\"0\""),   e("(format nil \"~D\" 0)"));
    TEST_EQ(equal, e("\"42\""),  e("(format nil \"~D\" 42)"));
    TEST_EQ(equal, e("\"-7\""),  e("(format nil \"~D\" -7)"));
    /* ~@D prefixes + for non-negative values. */
    TEST_EQ(equal, e("\"+42\""), e("(format nil \"~@D\" 42)"));}

/* --- ~X hex ------------------------------------------------------------- */

static void test_format_tilde_X(void){
    TEST_EQ(equal, e("\"0\""),    e("(format nil \"~X\" 0)"));
    TEST_EQ(equal, e("\"FF\""),   e("(format nil \"~X\" 255)"));
    TEST_EQ(equal, e("\"1000\""), e("(format nil \"~X\" 4096)"));}

/* --- ~O octal ----------------------------------------------------------- */

static void test_format_tilde_O(void){
    TEST_EQ(equal, e("\"0\""),  e("(format nil \"~O\" 0)"));
    TEST_EQ(equal, e("\"10\""), e("(format nil \"~O\" 8)"));}

/* --- ~B binary ---------------------------------------------------------- */

static void test_format_tilde_B(void){
    TEST_EQ(equal, e("\"0\""),    e("(format nil \"~B\" 0)"));
    TEST_EQ(equal, e("\"1010\""), e("(format nil \"~B\" 10)"));}

/* --- ~nR radix ---------------------------------------------------------- */

static void test_format_tilde_R(void){
    TEST_EQ(equal, e("\"101\""), e("(format nil \"~2R\" 5)"));
    TEST_EQ(equal, e("\"Z\""),   e("(format nil \"~36R\" 35)"));}

/* --- ~C character ------------------------------------------------------- */

static void test_format_tilde_C(void){
    TEST_EQ(equal, e("\"A\""), e("(format nil \"~C\" #\\A)"));
    TEST_EQ(eql,   I(1),       e("(length (format nil \"~C\" #\\Space))"));}

/* --- ~P plural ---------------------------------------------------------- */

static void test_format_tilde_P(void){
    TEST_EQ(equal, e("\"s\""),   e("(format nil \"~P\" 0)"));
    TEST_EQ(equal, e("\"\""),    e("(format nil \"~P\" 1)"));
    TEST_EQ(equal, e("\"s\""),   e("(format nil \"~P\" 2)"));
    TEST_EQ(equal, e("\"ies\""), e("(format nil \"~@P\" 2)"));
    TEST_EQ(equal, e("\"y\""),   e("(format nil \"~@P\" 1)"));}

/* --- ~{~} iterate over list --------------------------------------------- */

static void test_format_tilde_brace(void){
    TEST_EQ(equal, e("\"123\""),
            e("(format nil \"~{~A~}\" '(1 2 3))"));
    /* Each element followed by a space. */
    TEST_EQ(equal, e("\"1 2 3 \""),
            e("(format nil \"~{~A ~}\" '(1 2 3))"));}

/* --- Composite ---------------------------------------------------------- */

static void test_format_composite(void){
    TEST_EQ(equal, e("\"x=42, y=-7\""),
            e("(format nil \"x=~D, y=~D\" 42 -7)"));
    TEST_EQ(equal, e("\"Hello, world!\""),
            e("(format nil \"Hello, ~A!\" \"world\")"));
    /* ~A and ~D together.  Bocl's reader upcases 'foo → FOO. */
    TEST_EQ(equal, e("\"item: FOO count: 3\""),
            e("(format nil \"item: ~A count: ~D\" 'foo 3)"));}

/* --- WITH-OUTPUT-TO-STRING ---------------------------------------------- */

static void test_with_output_to_string(void){
    TEST_EQ(equal, e("\"hello42\""),
            e("(with-output-to-string (s)"
              "  (write-string \"hello\" s)"
              "  (princ 42 s))"));}

/* --- PRINC-TO-STRING / WRITE-TO-STRING ---------------------------------- */

static void test_princ_to_string(void){
    TEST_EQ(equal, e("\"hello\""), e("(princ-to-string \"hello\")"));
    TEST_EQ(equal, e("\"42\""),    e("(princ-to-string 42)"));}

static void test_write_to_string(void){
    /* write-to-string uses prin1, so a string gets quotes. */
    TEST_EQ(eql, T_Symbol,
            e("(equal (write-to-string \"hello\") (prin1-to-string \"hello\"))"));
    TEST_EQ(equal, e("\"42\""), e("(write-to-string 42)"));}

/* --- READ-FROM-STRING --------------------------------------------------- */

static void test_read_from_string(void){
    TEST_EQ(eql, I(42),    e("(read-from-string \"42\")"));
    TEST_EQ(eql, K(HELLO), e("(read-from-string \":hello\")"));
    TEST_EQ(eql, I(99),    e("(read-from-string \" 99 \"  nil nil :start 0)"));}

/* --- WRITE-STRING start/end --------------------------------------------- */

static void test_write_string(void){
    /* :start 1 skips the first character. */
    TEST_EQ(equal, e("\"ello\""),
            e("(with-output-to-string (s)"
              "  (write-string \"hello\" s :start 1))"));
    /* :end 3 takes only the first 3 characters. */
    TEST_EQ(equal, e("\"hel\""),
            e("(with-output-to-string (s)"
              "  (write-string \"hello\" s :end 3))"));}

/* ~(...~) case conversion (CLHS 22.3.8.2) + ~@? indirect format. */
static void test_format_case_conversion(void){
    TEST_EQ(equal, e("\"hello cruel world!\""),
            e("(format nil \"~(~A~)\" \"Hello Cruel World!\")"));   /* downcase */
    TEST_EQ(equal, e("\"Hello Cruel World!\""),
            e("(format nil \"~:(~A~)\" \"hello cruel world!\")"));  /* capitalize each word */
    TEST_EQ(equal, e("\"Hello cruel world!\""),
            e("(format nil \"~@(~A~)\" \"hello cruel world!\")"));  /* capitalize first word */
    TEST_EQ(equal, e("\"HELLO CRUEL WORLD!\""),
            e("(format nil \"~:@(~A~)\" \"hello cruel world!\")")); /* upcase */
    /* the issue's assertion, driven through ~@? (indirect, consuming args) */
    TEST_EQ(eql, T_Symbol,
            e("(if (equalp"
              "     (loop for spec in '(\"~(~A~)\" \"~:(~A~)\" \"~@(~A~)\" \"~:@(~A~)\")"
              "           collect (format nil \"~@?\" spec \"hello cruel world!\"))"
              "     '(\"hello cruel world!\" \"Hello Cruel World!\""
              "       \"Hello cruel world!\" \"HELLO CRUEL WORLD!\")) t nil)"));
    /* ~? (arg-list) still works */
    TEST_EQ(equal, e("\"a=1 b=2\""),
            e("(format nil \"~?\" \"a=~A b=~A\" '(1 2))"));}

TEST_INITIALIZE(test_format);
TEST_SUITE(test_format, fmt_init, fmt_clean)
    TEST_CASE("format ~(...~)/~@?",              test_format_case_conversion)
    TEST_CASE("format literal",                  test_format_literal)
    TEST_CASE("format ~A",                       test_format_tilde_A)
    TEST_CASE("format ~S",                       test_format_tilde_S)
    TEST_CASE("format ~%",                       test_format_tilde_percent)
    TEST_CASE("format newline (NEL)",            test_format_newline)
    TEST_CASE("format ~~",                       test_format_tilde_tilde)
    TEST_CASE("format ~D",                       test_format_tilde_D)
    TEST_CASE("format ~X",                       test_format_tilde_X)
    TEST_CASE("format ~O",                       test_format_tilde_O)
    TEST_CASE("format ~B",                       test_format_tilde_B)
    TEST_CASE("format ~R",                       test_format_tilde_R)
    TEST_CASE("format ~C",                       test_format_tilde_C)
    TEST_CASE("format ~P",                       test_format_tilde_P)
    TEST_CASE("format ~{~}",                     test_format_tilde_brace)
    TEST_CASE("format composite",                test_format_composite)
    TEST_CASE("with-output-to-string",           test_with_output_to_string)
    TEST_CASE("princ-to-string",                 test_princ_to_string)
    TEST_CASE("write-to-string",                 test_write_to_string)
    TEST_CASE("read-from-string",                test_read_from_string)
    TEST_CASE("write-string start/end",          test_write_string)
    TEST_FINALIZE()
