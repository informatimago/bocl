/* tests/test_designators.c — CLHS designator acceptance across CL functions
   (issues/closed/find-symbol-package.issue).

   CLHS designator types exercised here:
   - string designator   = (or string symbol character)
   - package designator  = (or package string-designator)
   - function designator = (or symbol function)
   - stream designator   = NIL (standard stream of the right direction),
                           T (*TERMINAL-IO*), or a stream
   - pathname designator = (or string pathname file-stream)
   - condition/restart designators for the condition system. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(desig_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);
    bocl_test_eval("(defpackage \"PD\" (:use) (:export \"EXT\"))");
    bocl_test_eval("(intern \"INT\" \"PD\")");
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(desig_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_test_eval(s); }

/* ---- string designators: the string functions --------------------------- */
static void test_string_designators(void){
    TEST_EQ(equal, e("\"FOO\""), e("(string 'foo)"));
    TEST_EQ(equal, e("\"A\""),   e("(string #\\A)"));
    TEST_EQ(eql, T_Symbol,  e("(string= 'foo \"FOO\")"));
    TEST_EQ(eql, T_Symbol,  e("(string= #\\A \"A\")"));
    TEST_EQ(eql, I(2),      e("(string< 'abc 'abd)"));
    TEST_EQ(equal, e("\"FOO\""), e("(string-upcase 'foo)"));
    TEST_EQ(equal, e("\"a\""),   e("(string-downcase #\\A)"));
    TEST_EQ(equal, e("\"Foo\""), e("(string-capitalize 'foo)"));
    TEST_EQ(equal, e("\"FOO\""), e("(string-trim \" \" 'foo)"));
    TEST_EQ(equal, e("\"FOO\""), e("(string-left-trim \" \" 'foo)"));
    TEST_EQ(equal, e("\"FOO\""), e("(string-right-trim \" \" 'foo)"));
    TEST_EQ(eql, NIL_Symbol, e("(string/= 'foo 'foo)"));
    TEST_EQ(eql, T_Symbol,  e("(string-equal 'foo \"foo\")"));
    TEST_EQ(eql, I(2),      e("(string-lessp 'abc 'abd)"));
    TEST_EQ(eql, I(2),      e("(string-greaterp 'abd 'abc)"));
    TEST_EQ(eql, I(0),      e("(string-not-equal 'foo \"bar\")"));
    /* CHARACTER accepts a character designator: 1-char string or symbol. */
    TEST_EQ(eql, e("#\\A"), e("(character \"A\")"));
    TEST_EQ(eql, e("#\\A"), e("(character '|A|)"));}

/* ---- package designators ------------------------------------------------- */
static void test_package_designators(void){
    TEST_EQ(eql, T_Symbol, e("(packagep (find-package \"PD\"))"));
    TEST_EQ(eql, T_Symbol, e("(packagep (find-package :pd))"));
    TEST_EQ(eql, T_Symbol, e("(packagep (find-package (find-package \"PD\")))"));
    TEST_EQ(equal, e("\"PD\""), e("(package-name :pd)"));
    TEST_EQ(equal, e("\"PD\""), e("(package-name (find-package \"PD\"))"));
    TEST_EQ(eql, e(":internal"), e("(nth-value 1 (intern \"INT\" (find-package \"PD\")))"));
    TEST_EQ(eql, e(":internal"), e("(nth-value 1 (intern \"INT\" :pd))"));
    TEST_EQ(eql, e(":external"), e("(nth-value 1 (find-symbol \"EXT\" :pd))"));
    TEST_EQ(eql, e(":external"), e("(nth-value 1 (find-symbol \"EXT\" (find-package \"PD\")))"));
    TEST_EQ(eql, T_Symbol, e("(let ((s (intern \"E2\" \"PD\"))) (export s :pd))"));
    TEST_EQ(eql, T_Symbol, e("(let ((s (intern \"E3\" \"PD\"))) (export s \"PD\") (unexport s :pd))"));
    TEST_EQ(eql, T_Symbol, e("(let ((s (make-symbol \"I1\"))) (import s :pd))"));
    TEST_EQ(eql, T_Symbol, e("(progn (shadow '#:sh1 :pd) t)"));
    TEST_EQ(eql, T_Symbol, e("(progn (shadow #\\S :pd) t)"));
    TEST_EQ(eql, T_Symbol, e("(let ((s (make-symbol \"SI1\"))) (shadowing-import s :pd))"));
    TEST_EQ(eql, T_Symbol, e("(let ((s (intern \"U1\" \"PD\"))) (unintern s :pd))"));
    TEST_EQ(eql, T_Symbol, e("(progn (defpackage \"PD2\" (:use)) (use-package :pd :pd2))"));
    TEST_EQ(eql, T_Symbol, e("(unuse-package :pd :pd2)"));
    TEST_EQ(eql, T_Symbol, e("(packagep (rename-package :pd2 :pd2b '(:pd2c)))"));
    TEST_EQ(eql, T_Symbol, e("(delete-package :pd2b)"));
    TEST_EQ(eql, T_Symbol, e("(let ((n 0)) (do-symbols (s :pd) s (incf n)) (if (>= n 1) t))"));
    TEST_EQ(eql, T_Symbol, e("(let ((n 0)) (do-external-symbols (s :pd) s (incf n)) (if (>= n 1) t))"));
    TEST_EQ(eql, T_Symbol, e("(symbolp (gentemp \"TMP\" :pd))"));}

/* ---- function designators: symbols accepted wherever functions are ------- */
static void test_function_designators(void){
    TEST_EQ(eql, I(1),  e("(funcall 'car '(1 2))"));
    TEST_EQ(eql, I(3),  e("(apply '+ '(1 2))"));
    TEST_EQ(equal, e("'(2 3)"), e("(mapcar '1+ '(1 2))"));
    TEST_EQ(equal, e("'(1 2)"), e("(mapcan 'list '(1 2))"));
    TEST_EQ(equal, e("'(2 3)"), e("(map 'list '1+ '(1 2))"));
    TEST_EQ(equal, e("'(1 2 3)"), e("(sort (list 3 1 2) '<)"));
    TEST_EQ(equal, e("'(1 2 3)"), e("(stable-sort (list 3 1 2) '<)"));
    TEST_EQ(eql, I(6),  e("(reduce '+ '(1 2 3))"));
    TEST_EQ(eql, T_Symbol, e("(some 'evenp '(1 2))"));
    TEST_EQ(eql, T_Symbol, e("(every 'numberp '(1 2))"));
    TEST_EQ(eql, T_Symbol, e("(notany 'stringp '(1 2))"));
    TEST_EQ(eql, T_Symbol, e("(notevery 'evenp '(1 2))"));
    TEST_EQ(equal, e("'(1 3)"), e("(remove-if 'evenp '(1 2 3))"));
    TEST_EQ(eql, I(1),  e("(count-if 'evenp '(1 2 3))"));
    TEST_EQ(eql, I(2),  e("(find-if 'evenp '(1 2 3))"));
    TEST_EQ(eql, I(1),  e("(position-if 'evenp '(1 2 3))"));
    TEST_EQ(equal, e("'(2 3)"), e("(member 2 '(1 2 3) :test 'eql)"));
    TEST_EQ(equal, e("'((2))"), e("(member 2 '((1) (2)) :key 'car)"));
    TEST_EQ(equal, e("'(2 . b)"), e("(assoc 2 '((1 . a) (2 . b)) :test 'eql)"));
    TEST_EQ(equal, e("'(1 2)"), e("(adjoin 1 '(1 2) :test 'eql)"));
    TEST_EQ(equal, e("'(1 2)"), e("(remove-duplicates '(1 1 2) :test 'eql)"));
    TEST_EQ(equal, e("'(1 0 3)"), e("(substitute-if 0 'evenp '(1 2 3))"));
    TEST_EQ(equal, e("'(1 3)"), e("(delete-if 'evenp (list 1 2 3))"));
    TEST_EQ(equal, e("'(1 2 3 4)"), e("(merge 'list (list 1 3) (list 2 4) '<)"));
    TEST_EQ(equal, e("'(1 2)"), e("(multiple-value-call 'list (values 1 2))"));}

/* ---- stream designators: NIL and T --------------------------------------- */
static void test_stream_designators(void){
    TEST_EQ(equal, e("\"x\""),
            e("(with-output-to-string (*standard-output*) (princ \"x\" nil))"));
    TEST_EQ(equal, e("\"y\""),
            e("(with-output-to-string (*standard-output*) (write-string \"y\" nil))"));
    TEST_EQ(equal, e("\"z\""),
            e("(with-output-to-string (*standard-output*) (write-char #\\z nil))"));
    TEST_EQ(eql, T_Symbol,
            e("(stringp (with-output-to-string (*standard-output*)"
              "  (prin1 :k nil) (print :k nil) (terpri nil) (fresh-line nil)"
              "  (write-line \"w\" nil)))"));
    TEST_EQ(equal, e("\"l1\""),
            e("(with-input-from-string (*standard-input* \"l1\") (read-line nil nil))"));
    TEST_EQ(eql, e("#\\a"),
            e("(with-input-from-string (*standard-input* \"a\") (read-char nil))"));
    TEST_EQ(eql, e("#\\b"),
            e("(with-input-from-string (*standard-input* \"b\") (peek-char nil nil))"));
    TEST_EQ(eql, I(42),
            e("(with-input-from-string (*standard-input* \"42\") (read nil))"));
    TEST_EQ(eql, T_Symbol,
            e("(with-input-from-string (*standard-input* \"x\") (listen nil))"));
    TEST_EQ(eql, NIL_Symbol, e("(finish-output nil)"));
    TEST_EQ(eql, NIL_Symbol, e("(force-output nil)"));
    TEST_EQ(eql, NIL_Symbol, e("(clear-output nil)"));
    /* UNREAD-CHAR: one-character pushback, and NIL stream designator. */
    TEST_EQ(eql, e("#\\c"),
            e("(with-input-from-string (*standard-input* \"cd\")"
              "  (unread-char (read-char nil) nil) (read-char nil))"));
    TEST_EQ(equal, e("'(#\\Newline #\\e)"),
            e("(with-input-from-string (i (format nil \"~%e\"))"
              "  (let ((c (read-char i))) (unread-char c i)"
              "    (list (read-char i) (read-char i))))"));}

/* ---- pathname designators ------------------------------------------------- */
static void test_pathname_designators(void){
    TEST_EQ(eql, T_Symbol, e("(pathnamep (pathname \"/tmp/x.lisp\"))"));
    TEST_EQ(equal, e("\"x\""), e("(pathname-name \"/tmp/x.lisp\")"));
    TEST_EQ(equal, e("\"lisp\""), e("(pathname-type \"/tmp/x.lisp\")"));
    TEST_EQ(equal, e("\"/tmp/x.lisp\""), e("(namestring \"/tmp/x.lisp\")"));
    TEST_EQ(eql, T_Symbol, e("(pathnamep (merge-pathnames \"x.lisp\" \"/tmp/\"))"));
    TEST_EQ(eql, T_Symbol,
            e("(let ((f (open (pathname \"{TMP}/desig.tmp\") :direction :output"
              "              :if-exists :supersede)))"
              "  (close f) (delete-file \"{TMP}/desig.tmp\") t)"));}

/* ---- condition and restart designators ------------------------------------ */
static void test_condition_designators(void){
    TEST_EQ(eql, e(":caught"),
            e("(handler-case (error \"boom ~A\" 1) (simple-error () :caught))"));
    TEST_EQ(eql, e(":caught"),
            e("(handler-case (error 'simple-error :format-control \"x\""
              "                     :format-arguments nil)"
              "  (simple-error () :caught))"));
    TEST_EQ(eql, e(":caught"),
            e("(handler-case (error (make-condition 'simple-error"
              "                      :format-control \"x\" :format-arguments nil))"
              "  (simple-error () :caught))"));
    TEST_EQ(eql, e(":caught"),
            e("(handler-case (signal \"sig ~A\" 2) (simple-condition () :caught))"));
    TEST_EQ(eql, e(":r1"),
            e("(restart-case (invoke-restart 'r1) (r1 () :r1))"));
    TEST_EQ(eql, e(":r2"),
            e("(restart-case (progn (invoke-restart (find-restart 'r2)) nil)"
              "  (r2 () :r2))"));}

TEST_INITIALIZE(test_designators);
TEST_SUITE(test_designators, desig_init, desig_fini)
    TEST_CASE("string designators",    test_string_designators)
    TEST_CASE("package designators",   test_package_designators)
    TEST_CASE("function designators",  test_function_designators)
    TEST_CASE("stream designators",    test_stream_designators)
    TEST_CASE("pathname designators",  test_pathname_designators)
    TEST_CASE("condition designators", test_condition_designators)
    TEST_FINALIZE()
