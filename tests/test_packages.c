#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"
#include "packages.h"

/* Phase 3 minimum package-system tests. */

TEST_SUITE_SETUP(packages_init){
    kernel_initialize();
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(packages_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){ return bocl_read_eval_cstring(text); }

static void test_standard_packages_exist(void){
    TEST_EQ(eql, T_Symbol, e("(packagep (find-package \"COMMON-LISP\"))"));
    TEST_EQ(eql, T_Symbol, e("(packagep (find-package \"COMMON-LISP-USER\"))"));
    TEST_EQ(eql, T_Symbol, e("(packagep (find-package \"KEYWORD\"))"));
    /* Nicknames resolve too. */
    TEST_EQ(eql, T_Symbol, e("(packagep (find-package \"CL\"))"));
    TEST_EQ(eql, T_Symbol, e("(packagep (find-package \"CL-USER\"))"));}

static void test_find_package_unknown(void){
    TEST_EQ(eql, NIL_Symbol, e("(find-package \"DOES-NOT-EXIST\")"));}

static void test_star_package_initial_value(void){
    /* *PACKAGE* initially binds to CL-USER. */
    TEST_EQ(equal, T("COMMON-LISP-USER"), e("(package-name *package*)"));}

static void test_in_package_switches(void){
    e("(in-package :common-lisp)");
    TEST_EQ(equal, T("COMMON-LISP"), e("(package-name *package*)"));
    /* Switch back so later tests run in CL-USER. */
    e("(in-package :common-lisp-user)");}

static void test_make_package(void){
    e("(make-package \"MY-PKG\")");
    TEST_EQ(eql, T_Symbol, e("(packagep (find-package \"MY-PKG\"))"));}

static void test_list_all_packages(void){
    /* Includes at least the three standard packages. */
    TEST_EQ(eql, T_Symbol, e("(<= 3 (list-length (list-all-packages)))"));}

static void test_packagep_negative(void){
    TEST_EQ(eql, NIL_Symbol, e("(packagep 42)"));
    TEST_EQ(eql, NIL_Symbol, e("(packagep \"COMMON-LISP\")"));
    TEST_EQ(eql, NIL_Symbol, e("(packagep nil)"));}

static void test_intern_creates_or_returns(void){
    /* intern returns the same symbol on repeated calls. */
    e("(setq tp-a (intern \"FOOBAR-FROM-INTERN\"))");
    e("(setq tp-b (intern \"FOOBAR-FROM-INTERN\"))");
    TEST_EQ(eql, T_Symbol, e("(eql tp-a tp-b)"));}

TEST_INITIALIZE(test_packages);
TEST_SUITE(test_packages, packages_init, packages_clean)
    TEST_CASE( "standard packages",       test_standard_packages_exist)
    TEST_CASE( "find-package unknown",    test_find_package_unknown)
    TEST_CASE( "*package* initial",       test_star_package_initial_value)
    TEST_CASE( "in-package switches",     test_in_package_switches)
    TEST_CASE( "make-package",            test_make_package)
    TEST_CASE( "list-all-packages",       test_list_all_packages)
    TEST_CASE( "packagep negatives",      test_packagep_negative)
    TEST_CASE( "intern identity",         test_intern_creates_or_returns)
    TEST_FINALIZE()
