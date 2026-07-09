/* tests/test_slice25.c — CUnit tests for Slice 25:
   MACHINE-TYPE, SOFTWARE-TYPE, SOFTWARE-VERSION, MACHINE-INSTANCE,
   PARSE-FLOAT, RATIONAL/RATIONALIZE, COMPILE stub, PROVIDE/REQUIRE/ *MODULES*,
   STRING-STREAM-P, FILE-STREAM-P, MACRO-FUNCTION, SPECIAL-OPERATOR-P,
   DECLARE in function bodies, WITH-HASH-TABLE-ITERATOR, SHORT-SITE-NAME,
   *LOAD-PATHNAME*, COMPILER-MACRO-FUNCTION. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s25_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);   /* bocl-internal names live in CL */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s25_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- Environment query functions ---------------------------------------- */

static void test_machine_type(void){
    /* machine-type returns a string (non-empty) */
    TEST_EQ(eql, T_Symbol,
            e("(and (stringp (machine-type)) (> (length (machine-type)) 0))"));}

static void test_software_type(void){
    TEST_EQ(eql, T_Symbol,
            e("(and (stringp (software-type)) (> (length (software-type)) 0))"));}

static void test_software_version(void){
    TEST_EQ(eql, T_Symbol,
            e("(and (stringp (software-version)) (> (length (software-version)) 0))"));}

static void test_machine_instance(void){
    TEST_EQ(eql, T_Symbol,
            e("(and (stringp (machine-instance)) (>= (length (machine-instance)) 0))"));}

static void test_machine_version(void){
    TEST_EQ(eql, T_Symbol,
            e("(stringp (machine-version))"));}

/* ---- PARSE-FLOAT --------------------------------------------------------- */

static void test_parse_float(void){
    TEST_EQ(eql, T_Symbol,
            e("(floatp (parse-float \"3.14\"))"));
    TEST_EQ(eql, T_Symbol,
            e("(< (abs (- (parse-float \"2.718\") 2.718)) 0.001)"));
    TEST_EQ(eql, T_Symbol,
            e("(floatp (parse-float \"0.0\"))"));
    TEST_EQ(eql, T_Symbol,
            e("(floatp (parse-float \"-1.5\"))"));}

/* ---- RATIONAL / RATIONALIZE ---------------------------------------------- */

static void test_rational(void){
    /* For integers, rational is identity */
    TEST_EQ(eql, I(42),  e("(rational 42)"));
    TEST_EQ(eql, I(-7),  e("(rational -7)"));
    /* For floats, RATIONAL returns an exact rational (a ratio) equal in value
       to the float — bocl grew a real ratio type in slice 71 — so the result
       is RATIONALP, not FLOATP. */
    TEST_EQ(eql, T_Symbol,
            e("(rationalp (rational 3.14))"));
    TEST_EQ(eql, NIL_Symbol,
            e("(floatp (rational 3.14))"));}

static void test_rationalize(void){
    TEST_EQ(eql, I(5),    e("(rationalize 5)"));
    TEST_EQ(eql, T_Symbol, e("(realp (rationalize 1.5))"));}

/* ---- COMPILE (minimal compilation) ------------------------------------- */

static void test_compile(void){
    /* (compile nil lambda-form) returns a callable function */
    TEST_EQ(eql, T_Symbol,
            e("(functionp (compile nil '(lambda (x) (* x 2))))"));
    /* the compiled function can be called */
    TEST_EQ(eql, I(42),
            e("(funcall (compile nil '(lambda (x) (* x 2))) 21)"));
    /* failure-p (third value) is NIL */
    TEST_EQ(eql, NIL_Symbol,
            e("(nth-value 2 (compile nil '(lambda (x) x)))"));
    /* (compile 'name) returns name */
    TEST_EQ(eql, e("'car"),
            e("(compile 'car)"));
    /* (compile nil fn) where fn is already a function returns that function */
    TEST_EQ(eql, T_Symbol,
            e("(functionp (compile nil #'car))")); }

/* ---- EVAL ---------------------------------------------------------------- */

static void test_eval(void){
    TEST_EQ(eql, I(42), e("(eval 42)"));
    TEST_EQ(eql, I(3),  e("(eval '(+ 1 2))"));
    TEST_EQ(eql, T_Symbol,
            e("(eval '(and t t))"));
    /* eval sees top-level definitions */
    TEST_EQ(eql, I(99),
            e("(progn (eval '(defvar *eval-test-var* 99)) *eval-test-var*)"));}

/* ---- MACROEXPAND-1 / MACROEXPAND ---------------------------------------- */

static void test_macroexpand(void){
    /* macroexpand-1 of a macro call returns (values expansion t) */
    TEST_EQ(eql, T_Symbol,
            e("(nth-value 1 (macroexpand-1 '(when t 42)))"));
    /* expansion of WHEN — use EQUAL since two fresh cons lists won't be EQL */
    TEST_EQ(equal, e("'(if t (progn 42))"),
            e("(macroexpand-1 '(when t 42))"));
    /* macroexpand-1 of a non-macro returns (values form nil) */
    TEST_EQ(eql, NIL_Symbol,
            e("(nth-value 1 (macroexpand-1 '(car x)))"));
    /* macroexpand fully expands */
    TEST_EQ(eql, T_Symbol,
            e("(let ((exp (macroexpand '(when t (when nil 1)))))"
              "  (and (consp exp) (eq 'if (car exp))))"));}

/* ---- COMPILE-FILE eval-when semantics ----------------------------------- */
/* The test file /tmp/bocl-s25-cf-test.lisp contains:
 *   (eval-when (:compile-toplevel) (defvar *cf-s25-ct* t))
 *   (eval-when (:execute)          (defvar *cf-s25-ex* t))
 * It is created by the test suite setup or pre-exists on disk. */

static void write_cf_test_file(void){
    /* Write directly from C to avoid bocl file I/O issues. */
    FILE* f = fopen("/tmp/bocl-s25-cf-test.lisp", "w");
    if(f){
        fputs("(eval-when (:compile-toplevel) (defvar *cf-s25-ct* t))\n", f);
        fputs("(eval-when (:execute) (defvar *cf-s25-ex* t))\n", f);
        fclose(f);}}

static void test_compile_file_eval_when(void){
    write_cf_test_file();
    /* compile-file: :COMPILE-TOPLEVEL fires, :EXECUTE does not */
    e("(compile-file \"/tmp/bocl-s25-cf-test.lisp\")");
    TEST_EQ(eql, T_Symbol,
            e("(boundp '*cf-s25-ct*)"));      /* :compile-toplevel fired */
    TEST_EQ(eql, NIL_Symbol,
            e("(boundp '*cf-s25-ex*)"));      /* :execute did NOT fire */
    /* load: :EXECUTE fires, :COMPILE-TOPLEVEL does not */
    e("(when (boundp '*cf-s25-ct*) (makunbound '*cf-s25-ct*))");
    e("(load \"/tmp/bocl-s25-cf-test.lisp\")");
    TEST_EQ(eql, NIL_Symbol,
            e("(boundp '*cf-s25-ct*)"));      /* :compile-toplevel did NOT fire */
    TEST_EQ(eql, T_Symbol,
            e("(boundp '*cf-s25-ex*)"));      /* :execute fired */ }

/* ---- COMPILE-FILE writes a real macro-free fast-load file ---------------- */

static void write_cf_fasl_src(void){
    FILE* f = fopen("/tmp/bocl-s25-fasl-src.lisp", "w");
    if(f){
        fputs("(defmacro s25-dbl (x) (list '* 2 x))\n", f);
        fputs("(defun s25-usedbl (n) (s25-dbl n))\n", f);
        fputs("(defun s25-sum (lst) (let ((s 0)) (dolist (x lst s) (setq s (+ s x)))))\n", f);
        fputs("(defvar *s25-fasl-loaded* t)\n", f);
        fclose(f);}}

static void test_compile_file_fasl(void){
    write_cf_fasl_src();
    /* compile-file writes a real fasl that PROBE-FILE finds ... */
    e("(compile-file \"/tmp/bocl-s25-fasl-src.lisp\")");
    TEST_EQ(eql, T_Symbol,
            e("(if (probe-file \"/tmp/bocl-s25-fasl-src.fasl\") t nil)"));
    /* ... carrying a bocl fasl version header ... */
    TEST_EQ(eql, T_Symbol,
            e("(if (%bocl-fasl-version \"/tmp/bocl-s25-fasl-src.fasl\") t nil)"));
    /* ... which loads and defines everything, the macro having expanded. */
    e("(load \"/tmp/bocl-s25-fasl-src.fasl\")");
    TEST_EQ(eql, T_Symbol, e("(boundp '*s25-fasl-loaded*)"));
    TEST_EQ(eql, I(6),  e("(s25-usedbl 3)"));
    TEST_EQ(eql, I(10), e("(s25-sum '(1 2 3 4))"));
    /* the compiled bodies are macro-free: no S25-DBL call, no DOLIST, remains */
    TEST_EQ(eql, NIL_Symbol,
            e("(labels ((has (f s) (cond ((eq f s) t) ((consp f)"
              "           (or (has (car f) s) (has (cdr f) s))) (t nil))))"
              "  (or (has (function-lambda-expression #'s25-usedbl) 's25-dbl)"
              "      (has (function-lambda-expression #'s25-sum) 'dolist)))"));}

/* ---- PROVIDE / REQUIRE / *MODULES* -------------------------------------- */

static void test_provide_require(void){
    /* provide registers the module name */
    TEST_EQ(eql, T_Symbol,
            e("(progn (provide \"test-mod-25\") (member \"test-mod-25\" *modules* :test #'string=) t)"));
    /* require for an already-provided module is a no-op */
    TEST_EQ(eql, T_Symbol,
            e("(progn (provide \"mod-a\") (require \"mod-a\") t)"));}

/* ---- Stream predicates --------------------------------------------------- */

static void test_string_stream_p(void){
    TEST_EQ(eql, T_Symbol,
            e("(string-stream-p (make-string-output-stream))"));
    TEST_EQ(eql, NIL_Symbol,
            e("(string-stream-p *standard-output*)"));}

static void test_file_stream_p(void){
    TEST_EQ(eql, T_Symbol,
            e("(file-stream-p *standard-output*)"));
    TEST_EQ(eql, NIL_Symbol,
            e("(file-stream-p (make-string-output-stream))"));}

/* ---- MACRO-FUNCTION ------------------------------------------------------ */

static void test_macro_function(void){
    /* WHEN is defined as a macro */
    TEST_EQ(eql, T_Symbol,
            e("(if (macro-function 'when) t nil)"));
    /* non-macro symbol returns NIL */
    TEST_EQ(eql, NIL_Symbol,
            e("(macro-function 'car)"));}

/* ---- SPECIAL-OPERATOR-P -------------------------------------------------- */

static void test_special_operator_p(void){
    TEST_EQ(eql, T_Symbol,   e("(special-operator-p 'if)"));
    TEST_EQ(eql, T_Symbol,   e("(special-operator-p 'let)"));
    TEST_EQ(eql, T_Symbol,   e("(special-operator-p 'quote)"));
    TEST_EQ(eql, NIL_Symbol, e("(special-operator-p 'car)"));
    TEST_EQ(eql, NIL_Symbol, e("(special-operator-p 'format)"));}

/* ---- DECLARE in function bodies ----------------------------------------- */

static void test_declare_in_defun(void){
    /* defun returns the symbol name; declare should be silently skipped */
    TEST_EQ(eql, e("'t25-decl-test"),
            e("(defun t25-decl-test (x) (declare (ignore x)) 42)"));
    /* calling the function returns the body value */
    TEST_EQ(eql, I(42),
            e("(t25-decl-test 99)"));}

static void test_declare_in_lambda(void){
    TEST_EQ(eql, I(7),
            e("(funcall (lambda (x y) (declare (ignore y)) x) 7 999)"));}

/* ---- WITH-HASH-TABLE-ITERATOR -------------------------------------------- */

static void test_with_hash_table_iterator(void){
    /* Should be able to iterate over a single-entry hash table */
    TEST_EQ(eql, T_Symbol,
            e("(let ((h (make-hash-table)))"
              "  (sethash 'k h 42)"
              "  (with-hash-table-iterator (nxt h)"
              "    (multiple-value-bind (more k v) (nxt)"
              "      (and more (eq k 'k) (eql v 42)))))"));
    /* Second call returns (nil nil nil) */
    TEST_EQ(eql, NIL_Symbol,
            e("(let ((h (make-hash-table)))"
              "  (sethash 'k h 1)"
              "  (with-hash-table-iterator (nxt h)"
              "    (nxt)"
              "    (nth-value 0 (nxt))  ; second call => more-p = NIL\n"
              "    ))"));}

/* ---- SHORT-SITE-NAME / LONG-SITE-NAME ------------------------------------ */

static void test_site_names(void){
    /* bocl maps the site name to the host identity (see issues/closed/
       platform-identification.issue): both now return the host name string,
       not the earlier NIL stub.  ANSI permits a string or NIL. */
    TEST_EQ(eql, T_Symbol, e("(stringp (short-site-name))"));
    TEST_EQ(eql, T_Symbol, e("(stringp (long-site-name))"));}

/* ---- *LOAD-PATHNAME* etc. ------------------------------------------------ */

static void test_load_vars(void){
    TEST_EQ(eql, NIL_Symbol, e("*load-pathname*"));
    TEST_EQ(eql, NIL_Symbol, e("*load-truename*"));}

/* ---- COMPILER-MACRO-FUNCTION ---------------------------------------------- */

static void test_compiler_macro_function(void){
    /* bocl has no compiler macros, so always NIL */
    TEST_EQ(eql, NIL_Symbol,
            e("(compiler-macro-function '+)"));}

/* ---- FUNCTION-LAMBDA-EXPRESSION / DISASSEMBLE (minimal-compilation) ------ */

static void test_function_lambda_expression(void){
    e("(defun s25-fle (x) (* x x))");
    /* primary value is the reconstructed (LAMBDA (x) ...) expression */
    TEST_EQ(eql, e("'lambda"),
            e("(car (function-lambda-expression #'s25-fle))"));
    TEST_EQ(equal, e("'(x)"),
            e("(cadr (function-lambda-expression #'s25-fle))"));
    /* third value NAME is the defun's name */
    TEST_EQ(eql, e("'s25-fle"),
            e("(nth-value 2 (function-lambda-expression #'s25-fle))"));
    /* a closure reports closure-p (second value) true */
    TEST_EQ(eql, T_Symbol,
            e("(let ((n 1)) (nth-value 1 (function-lambda-expression"
              "                            (compile nil (list 'lambda '() 'n)))))"));
    /* the body of a COMPILE'd function is macroexpanded (WHEN -> IF, no WHEN) */
    TEST_EQ(eql, NIL_Symbol,
            e("(let ((e (function-lambda-expression"
              "           (compile nil '(lambda (y) (when y 7))))))"
              "  (labels ((has (f s) (cond ((eq f s) t) ((consp f)"
              "             (or (has (car f) s) (has (cdr f) s))) (t nil))))"
              "    (has e 'when)))"));
    /* a built-in has no lambda expression */
    TEST_EQ(eql, NIL_Symbol,
            e("(function-lambda-expression #'car)"));}

static void test_disassemble(void){
    /* DISASSEMBLE returns NIL (CLHS) for a named function, a built-in, and a
       lambda expression */
    e("(defun s25-dis (x) (+ x 1))");
    TEST_EQ(eql, NIL_Symbol, e("(disassemble 's25-dis)"));
    TEST_EQ(eql, NIL_Symbol, e("(disassemble #'car)"));
    TEST_EQ(eql, NIL_Symbol, e("(disassemble '(lambda (x) x))"));}

static void test_macrolet_walk(void){
    /* MACROEXPAND-ALL (via COMPILE) understands MACROLET: the local macro is
       expanded and the compiled function behaves correctly. */
    TEST_EQ(eql, I(10),
            e("(funcall (compile nil"
              "  '(lambda () (macrolet ((m (x) (list '+ x x))) (m 5)))))"));
    /* SYMBOL-MACROLET likewise, including inner-LET shadowing */
    TEST_EQ(eql, I(10),
            e("(funcall (compile nil"
              "  '(lambda () (symbol-macrolet ((z (+ 2 3))) (* z 2)))))"));
    TEST_EQ(eql, I(7),
            e("(funcall (compile nil"
              "  '(lambda () (symbol-macrolet ((v 999)) (let ((v 7)) v)))))"));}


TEST_INITIALIZE(test_slice25);
TEST_SUITE(test_slice25, s25_init, s25_fini)
    TEST_CASE("machine-type",              test_machine_type)
    TEST_CASE("software-type",             test_software_type)
    TEST_CASE("software-version",          test_software_version)
    TEST_CASE("machine-instance",          test_machine_instance)
    TEST_CASE("machine-version",           test_machine_version)
    TEST_CASE("parse-float",               test_parse_float)
    TEST_CASE("rational/rationalize",      test_rational)
    TEST_CASE("rationalize",               test_rationalize)
    TEST_CASE("compile",                   test_compile)
    TEST_CASE("eval",                      test_eval)
    TEST_CASE("macroexpand",               test_macroexpand)
    TEST_CASE("compile-file eval-when",    test_compile_file_eval_when)
    TEST_CASE("compile-file fasl",         test_compile_file_fasl)
    TEST_CASE("provide/require/*modules*", test_provide_require)
    TEST_CASE("string-stream-p",           test_string_stream_p)
    TEST_CASE("file-stream-p",             test_file_stream_p)
    TEST_CASE("macro-function",            test_macro_function)
    TEST_CASE("special-operator-p",       test_special_operator_p)
    TEST_CASE("declare in defun",          test_declare_in_defun)
    TEST_CASE("declare in lambda",         test_declare_in_lambda)
    TEST_CASE("with-hash-table-iterator",  test_with_hash_table_iterator)
    TEST_CASE("site-names",                test_site_names)
    TEST_CASE("load-pathname vars",        test_load_vars)
    TEST_CASE("compiler-macro-function",   test_compiler_macro_function)
    TEST_CASE("function-lambda-expression", test_function_lambda_expression)
    TEST_CASE("disassemble",               test_disassemble)
    TEST_CASE("macrolet/symbol-macrolet",  test_macrolet_walk)
    TEST_FINALIZE()
