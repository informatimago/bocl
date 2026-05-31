/* tests/test_slice30.c — CUnit tests for Slice 30:
   TYPEP compound types (OR/AND/NOT/MEMBER/SATISFIES/EQL/range),
   FORMAT ~w,dF decimal precision, FORMAT ~mA min-col padding,
   FORMAT multi-param parsing, number type predicates, bit-vector ops,
   integer-type-p, make-bit-vector. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s30_init){
    kernel_initialize();
    bocl_load_cstring("/Users/pjb/src/public/bocl/bootstrap/prelude.lisp");
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s30_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- TYPEP compound types ----------------------------------------------- */

static void test_typep_or(void){
    TEST_EQ(eql, T_Symbol,   e("(typep 42 '(or integer string))"));
    TEST_EQ(eql, T_Symbol,   e("(typep \"hi\" '(or integer string))"));
    TEST_EQ(eql, NIL_Symbol, e("(typep 3.14 '(or integer string))"));}

static void test_typep_and(void){
    TEST_EQ(eql, T_Symbol,   e("(typep 42 '(and number integer))"));
    TEST_EQ(eql, NIL_Symbol, e("(typep 42 '(and number string))"));}

static void test_typep_not(void){
    TEST_EQ(eql, T_Symbol,   e("(typep \"hi\" '(not integer))"));
    TEST_EQ(eql, NIL_Symbol, e("(typep 42 '(not integer))"));}

static void test_typep_member(void){
    TEST_EQ(eql, T_Symbol,   e("(typep :foo '(member :foo :bar))"));
    TEST_EQ(eql, NIL_Symbol, e("(typep :baz '(member :foo :bar))"));}

static void test_typep_satisfies(void){
    TEST_EQ(eql, T_Symbol,   e("(typep 4 '(satisfies evenp))"));
    TEST_EQ(eql, NIL_Symbol, e("(typep 3 '(satisfies evenp))"));}

static void test_typep_ranges(void){
    /* (integer 0 255) — byte range */
    TEST_EQ(eql, T_Symbol,   e("(typep 100 '(integer 0 255))"));
    TEST_EQ(eql, NIL_Symbol, e("(typep 256 '(integer 0 255))"));
    TEST_EQ(eql, NIL_Symbol, e("(typep -1  '(integer 0 255))"));}

/* ---- FORMAT ~w,dF -------------------------------------------------------- */

static void test_format_float(void){
    /* ~2F — 2 decimal places */
    TEST_EQ(equal, e("\"3.14\""),
            e("(format nil \"~2F\" 3.14159)"));
    /* ~8,2F — 8 wide, 2 decimal */
    TEST_EQ(equal, e("\"    3.14\""),
            e("(format nil \"~8,2F\" 3.14159)"));
    /* ~0,4F — no width, 4 decimals */
    TEST_EQ(equal, e("\"3.1416\""),
            e("(format nil \"~0,4F\" 3.14159)"));}

/* ---- FORMAT ~mA (min-col padding) --------------------------------------- */

static void test_format_mincol(void){
    /* ~10A pads to at least 10 chars */
    TEST_EQ(eql, I(10),
            e("(string-length (format nil \"~10A\" \"hi\"))"));
    /* content is preserved at the left */
    TEST_EQ(equal, e("\"hi\""),
            e("(subseq (format nil \"~10A\" \"hi\") 0 2)"));
    /* no padding needed when string >= mincol */
    TEST_EQ(equal, e("\"hello world\""),
            e("(format nil \"~5A\" \"hello world\")"));}

/* ---- Number type predicates --------------------------------------------- */

static void test_number_predicates(void){
    TEST_EQ(eql, T_Symbol,   e("(fixnump 42)"));
    TEST_EQ(eql, T_Symbol,   e("(integer-type-p 'integer)"));
    TEST_EQ(eql, T_Symbol,   e("(integer-type-p '(integer 0 255))"));
    TEST_EQ(eql, NIL_Symbol, e("(integer-type-p 'float)"));}

/* ---- BIT-VECTOR operations ----------------------------------------------- */

static void test_bit_vector_ops(void){
    TEST_EQ(equal, e("#*0101"),
            e("(bit-and #*1101 #*0111)"));
    TEST_EQ(equal, e("#*1111"),
            e("(bit-or #*1100 #*0011)"));
    TEST_EQ(equal, e("#*1010"),
            e("(bit-xor #*1100 #*0110)"));
    TEST_EQ(equal, e("#*0011"),
            e("(bit-not #*1100)"));}


TEST_INITIALIZE(test_slice30);
TEST_SUITE(test_slice30, s30_init, s30_fini)
    TEST_CASE("typep (or ...)",              test_typep_or)
    TEST_CASE("typep (and ...)",             test_typep_and)
    TEST_CASE("typep (not ...)",             test_typep_not)
    TEST_CASE("typep (member ...)",          test_typep_member)
    TEST_CASE("typep (satisfies ...)",       test_typep_satisfies)
    TEST_CASE("typep integer/float ranges",  test_typep_ranges)
    TEST_CASE("format ~w,dF decimal",        test_format_float)
    TEST_CASE("format ~mA min-col",          test_format_mincol)
    TEST_CASE("number type predicates",      test_number_predicates)
    TEST_CASE("bit-vector operations",       test_bit_vector_ops)
    TEST_FINALIZE()
