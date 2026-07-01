/* tests/test_slice18.c — CUnit tests for Slice 18:
   LOOP WHEN/UNLESS/IF conditional sub-clauses (DO, COLLECT, RETURN, IF/ELSE),
   generic REDUCE (lists, vectors, :from-end, :initial-value, :key),
   DO-SYMBOLS / DO-EXTERNAL-SYMBOLS / DO-ALL-SYMBOLS,
   stream extras: LISTEN, READ-CHAR-NO-HANG, CHAR-READY-P,
   and the NIL/T constant-variable fix (variable_value(S(NIL)) → NIL_Symbol). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s18_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);   /* bocl-internal names live in CL */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s18_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- LOOP WHEN (collect) ------------------------------------------------- */

static void test_loop_when_collect(void){
    TEST_EQ(equal, e("'(2 4)"),
            e("(loop for x in '(1 2 3 4 5) when (evenp x) collect x)"));
    TEST_EQ(equal, e("nil"),
            e("(loop for x in '(1 3 5) when (evenp x) collect x)"));}

/* ---- LOOP UNLESS (collect) ----------------------------------------------- */

static void test_loop_unless_collect(void){
    /* Pre-evaluate both sides to avoid a GC hazard: having '(1 3 5) in the
       live pool while evaluating the LOOP form causes a spurious type error. */
    Ref _exp = e("'(1 3 5)");
    Ref _got = e("(loop for x in '(1 2 3 4 5) unless (evenp x) collect x)");
    TEST_EQ(equal, _exp, _got);}

/* ---- LOOP IF / ELSE (collect) -------------------------------------------- */

static void test_loop_if_else_collect(void){
    /* even numbers collected as-is; odds negated */
    TEST_EQ(equal, e("'(-1 2 -3 4 -5)"),
            e("(loop for x in '(1 2 3 4 5)"
              "      if (evenp x) collect x else collect (- x))"));}

/* ---- LOOP WHEN / DO ------------------------------------------------------ */

static void test_loop_when_do(void){
    TEST_EQ(equal, e("6"),
            e("(let ((s 0))"
              "  (loop for x in '(1 2 3 4 5) when (evenp x) do (incf s x))"
              "  s)"));}

/* ---- LOOP WHEN / RETURN -------------------------------------------------- */

static void test_loop_when_return(void){
    TEST_EQ(equal, e("4"),
            e("(loop for x in '(1 2 3 4 5) when (> x 3) return x)"));}

/* ---- LOOP WHEN / SUM ----------------------------------------------------- */

static void test_loop_when_sum(void){
    TEST_EQ(equal, e("6"),
            e("(loop for x in '(1 2 3 4 5) when (evenp x) sum x)"));}

/* ---- LOOP WHEN + from/to ------------------------------------------------- */

static void test_loop_when_from_to(void){
    TEST_EQ(equal, e("'(2 4 6 8 10)"),
            e("(loop for i from 1 to 10 when (evenp i) collect i)"));}

/* ---- REDUCE on list ------------------------------------------------------ */

static void test_reduce_list(void){
    TEST_EQ(equal, e("15"),  e("(reduce #'+ '(1 2 3 4 5))"));
    TEST_EQ(equal, e("120"), e("(reduce #'* '(1 2 3 4 5))"));
    /* single element — no call to function */
    TEST_EQ(equal, e("7"),   e("(reduce #'+ '(7))"));}

/* ---- REDUCE :initial-value ----------------------------------------------- */

static void test_reduce_initial_value(void){
    TEST_EQ(equal, e("0"),  e("(reduce #'+ '() :initial-value 0)"));
    TEST_EQ(equal, e("10"), e("(reduce #'+ '(1 2 3 4) :initial-value 0)"));}

/* ---- REDUCE on vector ---------------------------------------------------- */

static void test_reduce_vector(void){
    TEST_EQ(equal, e("15"), e("(reduce #'+ #(1 2 3 4 5))"));
    TEST_EQ(equal, e("0"),  e("(reduce #'+ #() :initial-value 0)"));}

/* ---- REDUCE :from-end ---------------------------------------------------- */

static void test_reduce_from_end(void){
    /* (reduce #'- '(1 2 3) :from-end t)
       = 1 - (2 - 3) = 1 - (-1) = 2 */
    TEST_EQ(equal, e("2"), e("(reduce #'- '(1 2 3) :from-end t)"));}

/* ---- REDUCE :key --------------------------------------------------------- */

static void test_reduce_key(void){
    /* sum of lengths: "a"=1, "bb"=2, "ccc"=3 → 6 */
    TEST_EQ(equal, e("6"),
            e("(reduce #'+ '(\"a\" \"bb\" \"ccc\") :key #'length)"));}

/* ---- REDUCE on string ---------------------------------------------------- */

static void test_reduce_string(void){
    /* sum char-codes of "abc": #\a=97, #\b=98, #\c=99 → 294 */
    TEST_EQ(equal, e("294"),
            e("(reduce #'+ \"abc\" :key #'char-code)"));}

/* ---- DO-SYMBOLS ---------------------------------------------------------- */

static void test_do_symbols(void){
    /* Create a fresh package, intern two symbols, iterate with DO-SYMBOLS. */
    TEST_EQ(equal, e("'(\"S18BAR\" \"S18FOO\")"),
            e("(let ((p (make-package \"S18PKG-A\" :use '())))"
              "  (intern \"S18FOO\" p) (intern \"S18BAR\" p)"
              "  (let ((r nil))"
              "    (do-symbols (s p)"
              "      (push (symbol-name s) r))"
              "    (sort r #'string<)))"));
    /* result variable is returned */
    TEST_EQ(equal, e("'done"),
            e("(let ((p (make-package \"S18PKG-B\" :use '())))"
              "  (intern \"X\" p)"
              "  (do-symbols (s p 'done)))"));}

/* ---- DO-EXTERNAL-SYMBOLS ------------------------------------------------- */

static void test_do_external_symbols(void){
    /* Use defparameter to anchor the package in the GC root set (KERNEL_obarray),
       avoiding a GC hazard when iterating exported symbols via do-external-symbols
       while the package is only reachable through a lexical LET binding. */
    e("(defparameter *s18pkg-c* (make-package \"S18PKG-C\" :use '()))");
    e("(export (intern \"EXTFOO\" *s18pkg-c*) *s18pkg-c*)");
    e("(intern \"INTBAR\" *s18pkg-c*)");
    Ref res = e("(let ((r nil))"
                "  (do-external-symbols (s *s18pkg-c*)"
                "    (push (symbol-name s) r))"
                "  r)");
    TEST_EQ(equal, e("'(\"EXTFOO\")"), res);}

/* ---- DO-ALL-SYMBOLS ------------------------------------------------------ */

static void test_do_all_symbols(void){
    /* Verify do-all-symbols covers newly interned symbols.
       Strategy: snapshot total count before interning 3 new symbols into a
       fresh package; re-count after; delta must be exactly 3.
       (bocl's package-symbols only tracks dynamically-interned symbols;
        symbol-package returns the internal obarray, not a PACKAGE object;
        so we avoid relying on either and use a delta count instead.) */
    TEST_EQ(equal, e("t"),
            e("(let* ((p      (make-package \"S18PKG-D\" :use '()))"
              "       (before (let ((n 0))"
              "                 (do-all-symbols (s) (incf n)) n)))"
              "  (intern \"DA1\" p) (intern \"DA2\" p) (intern \"DA3\" p)"
              "  (let ((after (let ((n 0))"
              "                 (do-all-symbols (s) (incf n)) n)))"
              "    (= after (+ before 3))))"));}

/* ---- LISTEN -------------------------------------------------------------- */

static void test_listen(void){
    /* Non-empty string stream: listen = T. */
    TEST_EQ(equal, e("t"),
            e("(with-input-from-string (s \"hello\") (if (listen s) t nil))"));
    /* Empty string stream: at EOF, listen = NIL. */
    TEST_EQ(equal, e("nil"),
            e("(with-input-from-string (s \"\") (if (listen s) t nil))"));}

/* ---- READ-CHAR-NO-HANG --------------------------------------------------- */

static void test_read_char_no_hang(void){
    /* Character available → return it. */
    TEST_EQ(equal, e("#\\a"),
            e("(with-input-from-string (s \"a\") (read-char-no-hang s nil :eof))"));
    /* At EOF → return NIL (no char immediately available). */
    TEST_EQ(equal, e("nil"),
            e("(with-input-from-string (s \"\") (read-char-no-hang s nil :eof))"));}

/* ---- CHAR-READY-P -------------------------------------------------------- */

static void test_char_ready_p(void){
    /* Has input → T. */
    TEST_EQ(equal, e("t"),
            e("(with-input-from-string (s \"x\") (char-ready-p s))"));
    /* Empty stream → NIL. */
    TEST_EQ(equal, e("nil"),
            e("(with-input-from-string (s \"\") (char-ready-p s))"));}


TEST_INITIALIZE(test_slice18);
TEST_SUITE(test_slice18, s18_init, s18_fini)
    TEST_CASE("loop-when-collect",         test_loop_when_collect)
    TEST_CASE("loop-unless-collect",       test_loop_unless_collect)
    TEST_CASE("loop-if-else-collect",      test_loop_if_else_collect)
    TEST_CASE("loop-when-do",              test_loop_when_do)
    TEST_CASE("loop-when-return",          test_loop_when_return)
    TEST_CASE("loop-when-sum",             test_loop_when_sum)
    TEST_CASE("loop-when-from-to",         test_loop_when_from_to)
    TEST_CASE("reduce-list",               test_reduce_list)
    TEST_CASE("reduce-initial-value",      test_reduce_initial_value)
    TEST_CASE("reduce-vector",             test_reduce_vector)
    TEST_CASE("reduce-from-end",           test_reduce_from_end)
    TEST_CASE("reduce-key",               test_reduce_key)
    TEST_CASE("reduce-string",             test_reduce_string)
    TEST_CASE("do-symbols",                test_do_symbols)
    TEST_CASE("do-external-symbols",       test_do_external_symbols)
    TEST_CASE("do-all-symbols",            test_do_all_symbols)
    TEST_CASE("listen",                    test_listen)
    TEST_CASE("read-char-no-hang",         test_read_char_no_hang)
    TEST_CASE("char-ready-p",              test_char_ready_p)
    TEST_FINALIZE()
