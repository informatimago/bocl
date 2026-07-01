/* tests/test_slice32.c — CUnit tests for Slice 33 logical pathnames:
   LOGICAL-PATHNAME, LOGICAL-PATHNAME-P, *-TRANSLATIONS,
   TRANSLATE-LOGICAL-PATHNAME, PATHNAME-HOST/DEVICE, MS-DOS device letters,
   distinguishing "C:/..." (device) from "HOST:..." (logical host). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s32_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);   /* bocl-internal names live in CL */
    /* Register a logical host used by several tests. */
    bocl_read_eval_cstring(
        "(set-logical-pathname-translations \"APP\""
        " '((\"APP:;**;*.LISP\" \"/usr/local/app/**/*.lisp\")"
        "   (\"APP:;**;*.*\"    \"/usr/local/app/**/*.*\")))");
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s32_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- LOGICAL-PATHNAME parsing & predicate ------------------------------- */

static void test_logical_pathname_p(void){
    TEST_EQ(eql, T_Symbol,
            e("(if (logical-pathname-p (logical-pathname \"APP:;src;init.lisp\")) t nil)"));
    TEST_EQ(eql, NIL_Symbol,
            e("(if (logical-pathname-p \"APP:;src;init.lisp\") t nil)"));}

static void test_namestring(void){
    TEST_EQ(equal, e("\"APP:;SRC;INIT.LISP\""),
            e("(namestring (logical-pathname \"APP:;src;init.lisp\"))"));}

/* ---- Component accessors ------------------------------------------------ */

static void test_components(void){
    TEST_EQ(equal, e("\"APP\""),
            e("(pathname-host (logical-pathname \"APP:;src;init.lisp\"))"));
    TEST_EQ(equal, e("\"INIT\""),
            e("(pathname-name (logical-pathname \"APP:;src;init.lisp\"))"));
    TEST_EQ(equal, e("\"LISP\""),
            e("(pathname-type (logical-pathname \"APP:;src;init.lisp\"))"));
    TEST_EQ(eql, e(":absolute"),
            e("(first (pathname-directory (logical-pathname \"APP:;src;init.lisp\")))"));
    TEST_EQ(equal, e("\"SRC\""),
            e("(second (pathname-directory (logical-pathname \"APP:;src;init.lisp\")))"));}

/* ---- TRANSLATE-LOGICAL-PATHNAME ----------------------------------------- */

static void test_translate(void){
    TEST_EQ(equal, e("\"/usr/local/app/src/init.lisp\""),
            e("(namestring (translate-logical-pathname"
              "             (logical-pathname \"APP:;src;init.lisp\")))"));}

/* ---- MS-DOS / Windows device letter ------------------------------------- */

static void test_device(void){
    TEST_EQ(equal, e("\"C\""),
            e("(pathname-device \"C:/users/foo/bar.txt\")"));
    TEST_EQ(eql, NIL_Symbol,
            e("(pathname-device \"/usr/local/bin\")"));}

/* ---- Disambiguation: C: is device, APP: is logical host ----------------- */

static void test_disambiguation(void){
    /* A single-letter prefix => physical pathname (device), NOT logical. */
    TEST_EQ(eql, NIL_Symbol,
            e("(if (logical-pathname-p (pathname \"C:/foo/bar\")) t nil)"));
    /* A registered multi-letter host => logical pathname. */
    TEST_EQ(eql, T_Symbol,
            e("(if (logical-pathname-p (pathname \"APP:;foo;bar.lisp\")) t nil)"));}

/* ---- TRANSLATIONS table accessor ---------------------------------------- */

static void test_translations(void){
    TEST_EQ(eql, I(2),
            e("(length (logical-pathname-translations \"APP\"))"));
    /* Case-insensitive host lookup. */
    TEST_EQ(eql, I(2),
            e("(length (logical-pathname-translations \"app\"))"));}


TEST_INITIALIZE(test_slice32);
TEST_SUITE(test_slice32, s32_init, s32_fini)
    TEST_CASE("logical-pathname-p",         test_logical_pathname_p)
    TEST_CASE("namestring",                 test_namestring)
    TEST_CASE("component accessors",        test_components)
    TEST_CASE("translate-logical-pathname", test_translate)
    TEST_CASE("pathname-device (C:)",       test_device)
    TEST_CASE("host vs device disambig",    test_disambiguation)
    TEST_CASE("translations table",         test_translations)
    TEST_FINALIZE()
