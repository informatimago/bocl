#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "simple_test.h"

#include "kernel.h"
#include "bocl.h"
#include "macros.h"

/* Phase 3 LOAD tests. We create a fixture file in /tmp/ and verify that
   bocl_load_cstring evaluates each form's side effects against the global
   environment. */

static char fixture_path[512];   /* per-process (bocl_test_tmp), set in setup */

TEST_SUITE_SETUP(load_init){
    kernel_initialize();
    bocl_test_load_prelude();   /* uses prelude-level operators (DEFPARAMETER/DEFUN/…) */
    snprintf(fixture_path,sizeof(fixture_path),"%s",bocl_test_tmp("test-load.lisp"));
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(load_clean){
    unlink(fixture_path);
    return CUE_SUCCESS;}

static void write_fixture(const char* content){
    FILE* f = fopen(fixture_path,"w");
    if(!f){ perror("fopen fixture"); exit(2); }
    fputs(content,f);
    fclose(f);}

static Ref e(const char* text){ return bocl_test_eval(text); }

static void test_load_empty_file(void){
    write_fixture("");
    TEST_EQ(eql, T_Symbol, bocl_load_cstring(fixture_path));}

static void test_load_defines_variables(void){
    write_fixture("(defparameter *tl-x* 42)\n");
    bocl_load_cstring(fixture_path);
    TEST_EQ(eql, I(42), e("*tl-x*"));}

static void test_load_defines_functions(void){
    write_fixture(
        "(defun tl-double (n) (* n 2))\n"
        "(defun tl-square (n) (* n n))\n");
    bocl_load_cstring(fixture_path);
    TEST_EQ(eql, I(14), e("(tl-double 7)"));
    TEST_EQ(eql, I(49), e("(tl-square 7)"));}

static void test_load_multiple_forms_in_order(void){
    /* The later defparameter wins (defparameter is always re-bound). */
    write_fixture(
        "(defparameter *tl-y* 1)\n"
        "(defparameter *tl-y* 2)\n"
        "(defparameter *tl-y* 3)\n");
    bocl_load_cstring(fixture_path);
    TEST_EQ(eql, I(3), e("*tl-y*"));}

static void test_load_via_lisp(void){
    /* Same as test_load_defines_variables but called through (LOAD …) at
       Lisp level. Confirms that the kernel_LOAD wrapper is wired up. */
    write_fixture("(defparameter *tl-z* 99)\n");
    char form[256];
    snprintf(form,sizeof(form),"(load \"%s\")",fixture_path);
    TEST_EQ(eql, T_Symbol, e(form));
    TEST_EQ(eql, I(99), e("*tl-z*"));}

static void test_load_binds_package(void){
    /* CLHS: LOAD binds *PACKAGE* (and *READTABLE*) to their pre-load values, so
       an IN-PACKAGE inside the loaded file must NOT leak into the caller. */
    write_fixture("(in-package \"KEYWORD\")\n");
    e("(defparameter *tl-pkg-before* (package-name *package*))");
    char form[256];
    snprintf(form,sizeof(form),"(load \"%s\")",fixture_path);
    e(form);
    /* *PACKAGE* is the same after the load as before it. */
    TEST_EQ(equal, e("*tl-pkg-before*"), e("(package-name *package*)"));}

static void test_load_missing_file_errors(void){
    /* bocl_load_cstring exits via error() on a missing file (if-does-not-exist
       is :error). Calling it in-process would terminate the test, so we just
       confirm here that the API exists at the C level by not invoking it. */
    /* Marker test — nothing to assert beyond the fact that this compiles. */
    (void)bocl_load_cstring;}

TEST_INITIALIZE(test_load);
TEST_SUITE(test_load, load_init, load_clean)
    TEST_CASE( "empty file",          test_load_empty_file)
    TEST_CASE( "defines variables",   test_load_defines_variables)
    TEST_CASE( "defines functions",   test_load_defines_functions)
    TEST_CASE( "multiple forms",      test_load_multiple_forms_in_order)
    TEST_CASE( "(load ...) via Lisp", test_load_via_lisp)
    TEST_CASE( "LOAD binds *PACKAGE*", test_load_binds_package)
    TEST_CASE( "missing-file marker", test_load_missing_file_errors)
    TEST_FINALIZE()
