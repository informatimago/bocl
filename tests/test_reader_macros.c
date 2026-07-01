#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"

/* Phase 4.3 reader-macro tests: #|...|#, #+/#-, #', pkg:sym / pkg::sym. */

TEST_SUITE_SETUP(rm_init){
    kernel_initialize();
    bocl_test_load_cl_exports();   /* kernel-only: reader symbol match, no prelude */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(rm_clean){
    return CUE_SUCCESS;}

static void test_block_comment_simple(void){
    TEST_EQ(eql, I(42), bocl_read_cstring("#|hello|# 42"));}

static void test_block_comment_nested(void){
    TEST_EQ(eql, I(99), bocl_read_cstring("#|outer #|inner|# more|# 99"));}

static void test_feature_plus_present(void){
    /* :BOCL is in *FEATURES*. */
    TEST_EQ(eql, K(YES), bocl_read_cstring("#+bocl :yes"));}

static void test_feature_plus_absent_in_list(void){
    /* #+ skips when absent, list elements close up. */
    TEST_EQ(equal,
            list(I(1), I(2), I(3), NULL),
            bocl_read_cstring("(1 #+no-such-feature 99 2 3)"));}

static void test_feature_minus_present_in_list(void){
    /* #- skips when feature IS present. */
    TEST_EQ(equal,
            list(I(1), I(2), I(3), NULL),
            bocl_read_cstring("(1 #-bocl 99 2 3)"));}

static void test_feature_or_and_not(void){
    TEST_EQ(eql, K(EITHER), bocl_read_cstring("#+(or bocl nope) :either"));
    TEST_EQ(eql, K(BOTH),   bocl_read_cstring("#+(and bocl bocl-1.0) :both"));
    TEST_EQ(equal,
            list(I(1), I(3), NULL),
            bocl_read_cstring("(1 #+(not bocl) 2 3)"));}

static void test_function_sharp_quote(void){
    /* #'car reads as (FUNCTION CAR). */
    TEST_EQ(equal,
            list(FUNCTION_Symbol, S(CAR), NULL),
            bocl_read_cstring("#'car"));}

static void test_package_qualified_symbols(void){
    /* In Phase 3 minimum packages, pkg:sym and pkg::sym both intern in the
       shared obarray and return the same symbol as the bare name. */
    TEST_EQ(eql, S(CAR),   bocl_read_cstring("common-lisp:car"));
    TEST_EQ(eql, S(CONS),  bocl_read_cstring("cl::cons"));
    /* KEYWORD:foo and :foo are equivalent. */
    TEST_EQ(eql, K(FOO),   bocl_read_cstring("keyword:foo"));}

TEST_INITIALIZE(test_reader_macros);
TEST_SUITE(test_reader_macros, rm_init, rm_clean)
    TEST_CASE( "block comment simple",   test_block_comment_simple)
    TEST_CASE( "block comment nested",   test_block_comment_nested)
    TEST_CASE( "#+ present",             test_feature_plus_present)
    TEST_CASE( "#+ absent in list",      test_feature_plus_absent_in_list)
    TEST_CASE( "#- present in list",     test_feature_minus_present_in_list)
    TEST_CASE( "#+(or/and/not …)",       test_feature_or_and_not)
    TEST_CASE( "#'name",                 test_function_sharp_quote)
    TEST_CASE( "pkg:sym pkg::sym",       test_package_qualified_symbols)
    TEST_FINALIZE()
