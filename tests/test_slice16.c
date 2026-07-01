/* tests/test_slice16.c — CUnit tests for Slice 16:
   char=, char/=, char<, char>, char<=, char>=,
   char-lessp, char-greaterp, char-not-greaterp, char-not-lessp, char-not-equal,
   digit-char,
   subseq on strings and vectors,
   coerce (list→vector, vector→list, list→string, string→list),
   concatenate with STRING result-type. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s16_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);   /* bocl-internal names live in CL */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s16_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- CHAR= / CHAR/= ------------------------------------------------------- */

static void test_char_equal(void){
    TEST_EQ(eql, T_Symbol, e("(char= #\\a #\\a)"));
    TEST_EQ(equal, e("nil"),  e("(char= #\\a #\\b)"));
    TEST_EQ(eql, T_Symbol, e("(char= #\\Z #\\Z)"));
    /* case-sensitive: upper ≠ lower */
    TEST_EQ(equal, e("nil"), e("(char= #\\A #\\a)"));}

static void test_char_noteq(void){
    TEST_EQ(eql, T_Symbol, e("(char/= #\\a #\\b)"));
    TEST_EQ(equal, e("nil"),  e("(char/= #\\a #\\a)"));}

/* ---- CHAR< / CHAR> / CHAR<= / CHAR>= ------------------------------------- */

static void test_char_lt(void){
    TEST_EQ(eql, T_Symbol, e("(char< #\\a #\\b)"));
    TEST_EQ(equal, e("nil"),  e("(char< #\\b #\\a)"));
    TEST_EQ(equal, e("nil"),  e("(char< #\\a #\\a)"));}

static void test_char_gt(void){
    TEST_EQ(eql, T_Symbol, e("(char> #\\b #\\a)"));
    TEST_EQ(equal, e("nil"),  e("(char> #\\a #\\b)"));
    TEST_EQ(equal, e("nil"),  e("(char> #\\a #\\a)"));}

static void test_char_lte(void){
    TEST_EQ(eql, T_Symbol, e("(char<= #\\a #\\b)"));
    TEST_EQ(eql, T_Symbol, e("(char<= #\\a #\\a)"));
    TEST_EQ(equal, e("nil"),  e("(char<= #\\b #\\a)"));}

static void test_char_gte(void){
    TEST_EQ(eql, T_Symbol, e("(char>= #\\b #\\a)"));
    TEST_EQ(eql, T_Symbol, e("(char>= #\\a #\\a)"));
    TEST_EQ(equal, e("nil"),  e("(char>= #\\a #\\b)"));}

/* ---- Case-insensitive comparisons --------------------------------------- */

static void test_char_lessp(void){
    TEST_EQ(eql, T_Symbol, e("(char-lessp #\\a #\\B)"));
    TEST_EQ(eql, T_Symbol, e("(char-lessp #\\A #\\b)"));
    TEST_EQ(equal, e("nil"),  e("(char-lessp #\\b #\\A)"));
    TEST_EQ(equal, e("nil"),  e("(char-lessp #\\a #\\A)"));}

static void test_char_greaterp(void){
    TEST_EQ(eql, T_Symbol, e("(char-greaterp #\\b #\\A)"));
    TEST_EQ(eql, T_Symbol, e("(char-greaterp #\\B #\\a)"));
    TEST_EQ(equal, e("nil"),  e("(char-greaterp #\\A #\\b)"));
    TEST_EQ(equal, e("nil"),  e("(char-greaterp #\\a #\\A)"));}

static void test_char_not_greaterp(void){
    TEST_EQ(eql, T_Symbol, e("(char-not-greaterp #\\a #\\B)"));
    TEST_EQ(eql, T_Symbol, e("(char-not-greaterp #\\a #\\A)"));
    TEST_EQ(equal, e("nil"),  e("(char-not-greaterp #\\b #\\A)"));}

static void test_char_not_lessp(void){
    TEST_EQ(eql, T_Symbol, e("(char-not-lessp #\\b #\\A)"));
    TEST_EQ(eql, T_Symbol, e("(char-not-lessp #\\a #\\A)"));
    TEST_EQ(equal, e("nil"),  e("(char-not-lessp #\\A #\\b)"));}

static void test_char_not_equal(void){
    /* char-not-equal is case-insensitive /= */
    TEST_EQ(eql, T_Symbol, e("(char-not-equal #\\a #\\b)"));
    TEST_EQ(equal, e("nil"),  e("(char-not-equal #\\a #\\A)"));
    TEST_EQ(equal, e("nil"),  e("(char-not-equal #\\A #\\a)"));}

/* ---- DIGIT-CHAR ---------------------------------------------------------- */

static void test_digit_char(void){
    /* decimal digits */
    TEST_EQ(eql, C(0), e("(digit-char 0)"));
    TEST_EQ(eql, C(9), e("(digit-char 9)"));
    /* hex */
    TEST_EQ(eql, C(A), e("(digit-char 10 16)"));
    TEST_EQ(eql, C(F), e("(digit-char 15 16)"));
    /* out of range → NIL */
    TEST_EQ(equal, e("nil"), e("(digit-char 10 10)"));
    TEST_EQ(equal, e("nil"), e("(digit-char 36 36)"));}

/* ---- SUBSEQ on strings -------------------------------------------------- */

static void test_subseq_string(void){
    /* (subseq "hello" 1 3) → "el" — verify char by char */
    TEST_EQ(eql, C(e),
            e("(char (subseq \"hello\" 1 3) 0)"));
    TEST_EQ(eql, C(l),
            e("(char (subseq \"hello\" 1 3) 1)"));
    TEST_EQ(eql, I(2),
            e("(string-length (subseq \"hello\" 1 3))"));
    /* (subseq "hello" 2) → "llo" */
    TEST_EQ(eql, I(3),
            e("(string-length (subseq \"hello\" 2))"));
    TEST_EQ(eql, C(l),
            e("(char (subseq \"hello\" 2) 0)"));
    TEST_EQ(eql, C(l),
            e("(char (subseq \"hello\" 2) 1)"));
    TEST_EQ(eql, C(o),
            e("(char (subseq \"hello\" 2) 2)"));}

/* ---- SUBSEQ on vectors -------------------------------------------------- */

static void test_subseq_vector(void){
    /* (subseq #(1 2 3 4) 1 3) → #(2 3) */
    TEST_EQ(eql, I(2),
            e("(vector-length (subseq #(1 2 3 4) 1 3))"));
    TEST_EQ(eql, I(2),
            e("(svref (subseq #(1 2 3 4) 1 3) 0)"));
    TEST_EQ(eql, I(3),
            e("(svref (subseq #(1 2 3 4) 1 3) 1)"));}

/* ---- COERCE ------------------------------------------------------------- */

static void test_coerce_list_to_vector(void){
    TEST_EQ(eql, I(3),
            e("(vector-length (coerce '(1 2 3) 'vector))"));
    TEST_EQ(eql, I(2),
            e("(svref (coerce '(1 2 3) 'vector) 1)"));}

static void test_coerce_vector_to_list(void){
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(coerce (vector 1 2 3) 'list)"));}

static void test_coerce_list_to_string(void){
    /* '(#\h #\i) → "hi" */
    TEST_EQ(eql, I(2),
            e("(string-length (coerce (list #\\h #\\i) 'string))"));
    TEST_EQ(eql, C(h),
            e("(char (coerce (list #\\h #\\i) 'string) 0)"));
    TEST_EQ(eql, C(i),
            e("(char (coerce (list #\\h #\\i) 'string) 1)"));}

static void test_coerce_string_to_list(void){
    TEST_EQ(eql, I(3),
            e("(length (coerce \"abc\" 'list))"));
    TEST_EQ(eql, C(a),
            e("(car (coerce \"abc\" 'list))"));
    TEST_EQ(eql, C(b),
            e("(cadr (coerce \"abc\" 'list))"));}

/* ---- CONCATENATE with STRING result-type -------------------------------- */

static void test_concatenate_string(void){
    /* two strings */
    TEST_EQ(eql, I(5),
            e("(string-length (concatenate 'string \"hel\" \"lo\"))"));
    TEST_EQ(eql, C(h),
            e("(char (concatenate 'string \"hel\" \"lo\") 0)"));
    TEST_EQ(eql, C(o),
            e("(char (concatenate 'string \"hel\" \"lo\") 4)"));
    /* list of chars + string */
    TEST_EQ(eql, I(3),
            e("(string-length (concatenate 'string (list #\\a #\\b) \"c\"))"));
    TEST_EQ(eql, C(a),
            e("(char (concatenate 'string (list #\\a #\\b) \"c\") 0)"));
    TEST_EQ(eql, C(c),
            e("(char (concatenate 'string (list #\\a #\\b) \"c\") 2)"));}

TEST_INITIALIZE(test_slice16);
TEST_SUITE(test_slice16, s16_init, s16_fini)
    TEST_CASE("char=",              test_char_equal)
    TEST_CASE("char/=",             test_char_noteq)
    TEST_CASE("char<",              test_char_lt)
    TEST_CASE("char>",              test_char_gt)
    TEST_CASE("char<=",             test_char_lte)
    TEST_CASE("char>=",             test_char_gte)
    TEST_CASE("char-lessp",         test_char_lessp)
    TEST_CASE("char-greaterp",      test_char_greaterp)
    TEST_CASE("char-not-greaterp",  test_char_not_greaterp)
    TEST_CASE("char-not-lessp",     test_char_not_lessp)
    TEST_CASE("char-not-equal",     test_char_not_equal)
    TEST_CASE("digit-char",         test_digit_char)
    TEST_CASE("subseq-string",      test_subseq_string)
    TEST_CASE("subseq-vector",      test_subseq_vector)
    TEST_CASE("coerce-list-vector", test_coerce_list_to_vector)
    TEST_CASE("coerce-vector-list", test_coerce_vector_to_list)
    TEST_CASE("coerce-list-string", test_coerce_list_to_string)
    TEST_CASE("coerce-string-list", test_coerce_string_to_list)
    TEST_CASE("concatenate-string", test_concatenate_string)
    TEST_FINALIZE()
