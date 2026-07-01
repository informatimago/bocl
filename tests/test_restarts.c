#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"

#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

/* Slice 9b — RESTART-CASE, FIND-RESTART, INVOKE-RESTART, CERROR.
   Does NOT load prelude.lisp — all tests run at global-env level. */

TEST_SUITE_SETUP(rc_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);   /* expected symbols are CL (symbol_intern_cstring) */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(rc_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){ return bocl_read_eval_cstring(text); }

/* restart-case: normal form value returned when no restart invoked. */
static void test_restart_case_no_invoke(void){
    TEST_EQ(eql, I(42),
            e("(restart-case 42 (my-restart () :never-reached))"));}

/* restart-case + invoke-restart: invoke fires and returns restart body value. */
static void test_restart_case_invoke(void){
    Ref r = e("(restart-case"
              "  (invoke-restart 'my-restart)"
              "  (my-restart () :restart-result))");
    TEST_EQ(eql, K(RESTART-RESULT), r);}

/* find-restart: returns the restart name when active, NIL otherwise. */
static void test_find_restart(void){
    /* Active restart is visible. */
    TEST_EQ(eql, S(MY-RESTART),
            e("(restart-case"
              "  (find-restart 'my-restart)"
              "  (my-restart () nil))"));
    /* Outside the RESTART-CASE, restart is gone. */
    TEST_EQ(eql, NIL_Symbol,
            e("(find-restart 'my-restart)"));}

/* handler-bind + invoke-restart: the classic "correctable error" pattern.
   HANDLER-BIND's handler invokes the CONTINUE restart established by
   RESTART-CASE, returning NIL from the protected form. */
static void test_handler_bind_invoke_restart(void){
    Ref r = e("(restart-case"
              "  (handler-bind ((error (lambda (c)"
              "                    (invoke-restart 'continue))))"
              "    (error \"correctable\"))"
              "  (continue () :recovered))");
    TEST_EQ(eql, K(RECOVERED), r);}

/* cerror: CONTINUE restart fires through handler-bind; cerror returns NIL. */
static void test_cerror_continue(void){
    Ref r = e("(handler-bind"
              "    ((error (lambda (c) (invoke-restart 'continue))))"
              "  (cerror \"retry\" \"something went wrong\"))");
    TEST_EQ(eql, NIL_Symbol, r);}

/* Restart-case with argument: invoke-restart passes args to restart body. */
static void test_restart_with_arg(void){
    Ref r = e("(restart-case"
              "  (invoke-restart 'use-value 99)"
              "  (use-value (v) v))");
    TEST_EQ(eql, I(99), r);}

TEST_INITIALIZE(test_restarts);
TEST_SUITE(test_restarts, rc_init, rc_clean)
    TEST_CASE("restart-case no invoke",          test_restart_case_no_invoke)
    TEST_CASE("restart-case + invoke-restart",   test_restart_case_invoke)
    TEST_CASE("find-restart active / inactive",  test_find_restart)
    TEST_CASE("handler-bind + invoke-restart",   test_handler_bind_invoke_restart)
    TEST_CASE("cerror + continue",               test_cerror_continue)
    TEST_CASE("restart with argument",           test_restart_with_arg)
    TEST_FINALIZE()
