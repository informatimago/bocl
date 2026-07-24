/* tests/test_conformance.c — CLHS conformance fixes from the
   asdf-loading-cesarum issue: reader conditionals in dotted tails,
   PROBE-FILE/TRUENAME pathnames, DEFINE-MODIFY-MACRO, DEFSTRUCT
   docstrings, *COMPILE-FILE-PATHNAME*, non-keyword method qualifiers,
   short-form method combinations, DEFPACKAGE option ordering. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(conf_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(conf_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_test_eval(s); }

/* ---- reader conditionals in a dotted tail (asdf.asd idiom) -------------- */
static void test_reader_dotted_conditionals(void){
    /* feature ABSENT: #-x keeps its form as the tail, #+x consumed+discarded */
    TEST_EQ(equal, e("'(1 2)"),
            e("(read-from-string \"(1 2 . #-nofeat () #+nofeat (3 4))\")"));
    /* feature PRESENT: #-x suppressed (NOT an object), #+x form is the tail */
    TEST_EQ(equal, e("'(1 2 3 4)"),
            e("(progn (pushnew :conf-feat *features*)"
              "  (read-from-string \"(1 2 . #-conf-feat () #+conf-feat (3 4))\"))"));
    /* element position unchanged */
    TEST_EQ(equal, e("'(1 2)"), e("(read-from-string \"(1 2 #+nofeat 3)\")"));
    TEST_EQ(equal, e("'(1 2 3)"), e("(read-from-string \"(1 2 #-nofeat 3)\")"));}

/* ---- PROBE-FILE / TRUENAME return pathnames ----------------------------- */
static void test_probe_file_pathname(void){
    TEST_EQ(eql, T_Symbol, e("(pathnamep (probe-file \"/dev/null\"))"));
    TEST_EQ(eql, T_Symbol, e("(pathnamep (truename \"/dev/null\"))"));
    TEST_EQ(eql, NIL_Symbol, e("(probe-file \"/bocl-no-such-file-zz\")"));}

/* ---- DEFINE-MODIFY-MACRO: &rest, &optional, single evaluation ----------- */
static void test_define_modify_macro(void){
    e("(define-modify-macro conf-appf (&rest lists) append)");
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(let ((x (list 1))) (conf-appf x (list 2) (list 3)) x)"));
    e("(define-modify-macro conf-inc (&optional (delta 1)) +)");
    TEST_EQ(eql, I(16), e("(let ((n 5)) (conf-inc n) (conf-inc n 10) n)"));
    /* CLHS: the place's subforms are evaluated exactly once */
    TEST_EQ(equal, e("'((5 6) 0)"),
            e("(let ((i -1) (v (vector nil)))"
              "  (conf-appf (aref v (incf i)) (list 5 6))"
              "  (list (aref v 0) i))"));}

/* ---- DEFSTRUCT accepts a documentation string --------------------------- */
static void test_defstruct_docstring(void){
    TEST_EQ(eql, I(3),
            e("(progn (defstruct conf-ds \"Doc string.\" x) "
              "(conf-ds-x (make-conf-ds :x 3)))"));
    TEST_EQ(eql, T_Symbol,
            e("(progn (defstruct conf-ds2 \"Only a doc.\") "
              "(conf-ds2-p (make-conf-ds2)))"));}

/* ---- COMPILE-FILE binds *COMPILE-FILE-PATHNAME* ------------------------- */
static void test_compile_file_pathname_bound(void){
    e("(with-open-file (o \"{TMP}/conf-cfp.lisp\" :direction :output"
      "                   :if-exists :supersede)"
      "  (print '(eval-when (:compile-toplevel)"
      "            (setf (symbol-value 'cl-user::*conf-cfp-seen*)"
      "                  (and *compile-file-pathname* t))) o))");
    e("(compile-file \"{TMP}/conf-cfp.lisp\")");
    TEST_EQ(eql, T_Symbol, e("cl-user::*conf-cfp-seen*"));
    e("(ignore-errors (delete-file \"{TMP}/conf-cfp.lisp\"))");
    e("(ignore-errors (delete-file (compile-file-pathname \"{TMP}/conf-cfp.lisp\")))");}

/* ---- non-keyword qualifiers + short-form method combinations ------------ */
static void test_method_combinations(void){
    e("(defclass conf-a () ())");
    e("(defclass conf-b (conf-a) ())");
    e("(defgeneric conf-slots (o)"
      "  (:method-combination append)"
      "  (:method append ((o conf-a)) (list :a 1))"
      "  (:method append ((o conf-b)) (list :b 2)))");
    /* most-specific first, results appended */
    TEST_EQ(equal, e("'(:b 2 :a 1)"), e("(conf-slots (make-instance 'conf-b))"));
    TEST_EQ(equal, e("'(:a 1)"),      e("(conf-slots (make-instance 'conf-a))"));
    e("(defgeneric conf-tot (o)"
      "  (:method + ((o conf-a)) 10)"
      "  (:method + ((o conf-b)) 32))");
    TEST_EQ(eql, I(42), e("(conf-tot (make-instance 'conf-b))"));
    /* standard combination unaffected */
    e("(defgeneric conf-std (o))");
    e("(defmethod conf-std ((o conf-a)) :base)");
    e("(defmethod conf-std :around ((o conf-b)) (list :around (call-next-method)))");
    TEST_EQ(equal, e("'(:around :base)"), e("(conf-std (make-instance 'conf-b))"));}

/* ---- DIVISION-BY-ZERO condition class (division-by-zero-not-caught) ------ */
static void test_division_by_zero(void){
    TEST_EQ(eql, e(":dbz"),
            e("(handler-case (/ 10 0) (division-by-zero () :dbz))"));
    TEST_EQ(eql, e(":ae"),
            e("(handler-case (/ 10.5 0.0) (arithmetic-error () :ae))"));
    TEST_EQ(eql, e(":mod"),
            e("(handler-case (mod 5 0) (division-by-zero () :mod))"));
    TEST_EQ(eql, e(":ok"),
            e("(progn (/ 10 2) :ok)"));}

/* ---- CLOS condition report printing (printing-conditions +
        clos-condition-report-not-printed) ---------------------------------- */
static void test_clos_condition_printing(void){
    e("(define-condition conf-err (simple-error) ())");
    /* :FORMAT-CONTROL reaches the inherited CLOS simple-condition slot. */
    TEST_EQ(equal, e("\"boom 5\""),
            e("(let ((c (make-condition 'conf-err :format-control \"boom ~S\""
              "                         :format-arguments '(5))))"
              "  (with-output-to-string (s) (princ c s)))"));
    /* PRIN1 keeps the escaped #<...> form. */
    TEST_EQ(eql, I(0),
            e("(let ((c (make-condition 'conf-err :format-control \"x\""
              "                         :format-arguments nil)))"
              "  (search \"#<CONF-ERR\" (with-output-to-string (s) (prin1 c s))))"));
    /* DEFINE-CONDITION :REPORT is used under PRINC. */
    TEST_EQ(equal, e("\"custom report\""),
            e("(progn (define-condition conf-rep () ()"
              "         (:report (lambda (c s) (declare (ignore c))"
              "                    (write-string \"custom report\" s))))"
              "  (with-output-to-string (s) (princ (make-condition 'conf-rep) s)))"));
    /* SIMPLE-CONDITION-FORMAT-CONTROL works on CLOS condition instances. */
    TEST_EQ(equal, e("\"fc\""),
            e("(simple-condition-format-control"
              "  (make-condition 'conf-err :format-control \"fc\" :format-arguments nil))"));}

/* ---- DEFPACKAGE: shadowing imports before :use -------------------------- */
static void test_defpackage_shadowing_order(void){
    /* Using a package that exports a CL-colliding name works when the
       collision is resolved by :shadowing-import-from — which DEFPACKAGE
       must process BEFORE establishing :use inheritance. */
    e("(defpackage \"CONF-SET\" (:use) (:export \"UNION\" \"MERGE\"))");
    TEST_EQ(equal, e("\"CONF-SET\""),
            e("(progn (defpackage \"CONF-IX\""
              "         (:use \"COMMON-LISP\" \"CONF-SET\")"
              "         (:shadowing-import-from \"CONF-SET\" \"UNION\" \"MERGE\"))"
              "  (package-name (symbol-package (find-symbol \"UNION\" \"CONF-IX\"))))"));}

/* ---- Composite streams: two-way / echo / synonym / concatenated -------- */
static void test_composite_streams(void){
    /* TWO-WAY: predicate T; input/output accessors return the components. */
    TEST_EQ(eql, e("t"),
            e("(let* ((i (make-string-input-stream \"\"))"
              "       (o (make-string-output-stream))"
              "       (tw (make-two-way-stream i o)))"
              "  (and (two-way-stream-p tw)"
              "       (eq (two-way-stream-input-stream tw) i)"
              "       (eq (two-way-stream-output-stream tw) o)"
              "       t))"));
    /* TWO-WAY: reads come from the input side, writes go to the output side. */
    TEST_EQ(equal, e("\"hiWORLD\""),
            e("(let* ((i (make-string-input-stream \"hi\"))"
              "       (o (make-string-output-stream))"
              "       (tw (make-two-way-stream i o)))"
              "  (with-output-to-string (r)"
              "    (write-char (read-char tw) r) (write-char (read-char tw) r)"
              "    (write-string \"WORLD\" tw)"
              "    (write-string (get-output-stream-string o) r))))"));
    /* CONCATENATED: reads through each component in turn, advancing at EOF. */
    TEST_EQ(equal, e("\"abcde\""),
            e("(let ((c (make-concatenated-stream (make-string-input-stream \"abc\")"
              "                                   (make-string-input-stream \"de\"))))"
              "  (and (concatenated-stream-p c)"
              "       (with-output-to-string (o)"
              "         (loop for ch = (read-char c nil nil) while ch do (write-char ch o)))))"));
    /* ECHO: a character read from the input is echoed to the output. */
    TEST_EQ(equal, e("\"xy\""),
            e("(let* ((i (make-string-input-stream \"xyz\"))"
              "       (o (make-string-output-stream))"
              "       (e (make-echo-stream i o)))"
              "  (and (echo-stream-p e)"
              "       (eq (echo-stream-input-stream e) i)"
              "       (progn (read-char e) (read-char e)"
              "              (get-output-stream-string o)))))"));
    /* SYNONYM: forwards to the current value of its symbol. */
    TEST_EQ(equal, e("\"PQ\""),
            e("(let ((s (make-synonym-stream '*conf-syn*)))"
              "  (declare (special *conf-syn*))"
              "  (setf (symbol-value '*conf-syn*) (make-string-input-stream \"PQR\"))"
              "  (and (synonym-stream-p s)"
              "       (eq (synonym-stream-symbol s) '*conf-syn*)"
              "       (with-output-to-string (o)"
              "         (write-char (read-char s) o) (write-char (read-char s) o)))))"));
    /* READ parses a whole s-expression through a two-way stream's cookie FILE*. */
    TEST_EQ(equal, e("'(1 2 3 4)"),
            e("(read (make-two-way-stream (make-string-input-stream \"(1 2 . (3 4))\")"
              "                           (make-string-output-stream)))"));}

/* ---- Adjustable / fill-pointer character vectors are STRINGs ------------ */
static void test_adjustable_string(void){
    /* Type recognition: a complex 1-D character array is a STRING / VECTOR /
       ARRAY / SEQUENCE (but never SIMPLE-STRING); STRINGP and TYPEP agree. */
    TEST_EQ(eql, e("t"),
            e("(let ((a (make-array 3 :element-type 'character :adjustable t :fill-pointer 0)))"
              "  (vector-push-extend #\\a a) (vector-push-extend #\\b a)"
              "  (and (stringp a) (typep a 'string) (typep a 'vector) (typep a 'array)"
              "       (typep a 'sequence) (not (typep a 'simple-string))"
              "       (not (stringp (make-array 2 :adjustable t :fill-pointer 0))) t))"));
    /* Element access + string ops run on it (CHAR / SCHAR / SUBSEQ / case). */
    TEST_EQ(equal, e("\"ab AB b bc\""),
            e("(let ((a (make-array 3 :element-type 'character :adjustable t :fill-pointer 0)))"
              "  (vector-push-extend #\\a a) (vector-push-extend #\\b a) (vector-push-extend #\\c a)"
              "  (format nil \"~A ~A ~A ~A\" (subseq a 0 2) (string-upcase (subseq a 0 2))"
              "                              (char a 1) (subseq a 1)))"));
    /* CLOS method specialised on STRING is applicable to it. */
    TEST_EQ(equal, e("\"str\""),
            e("(progn (defgeneric adjs-k (x) (:method ((x string)) \"str\") (:method (x) \"other\"))"
              "  (let ((a (make-array 1 :element-type 'character :adjustable t :fill-pointer 0)))"
              "    (vector-push-extend #\\z a) (adjs-k a)))"));}

/* ---- SETF of THE and of a macro place (CLHS 5.1.2.3 / 5.1.2.6) ---------- */
static void test_setf_place_forms(void){
    /* (setf (the type place) v) == (setf place (the type v)). */
    TEST_EQ(equal, e("'(9 2 3)"),
            e("(let ((v (vector 1 2 3))) (setf (the fixnum (aref v 0)) 9) (coerce v 'list)))"));
    /* A macro place with no SETF expander is macroexpanded, then SETF'd. */
    TEST_EQ(equal, e("'(1 2 99)"),
            e("(progn (defmacro conf-third-place (l) `(third ,l))"
              "  (let ((x (list 1 2 3))) (setf (conf-third-place x) 99) x))"));}

TEST_INITIALIZE(test_conformance);
TEST_SUITE(test_conformance, conf_init, conf_fini)
    TEST_CASE("reader dotted conditionals", test_reader_dotted_conditionals)
    TEST_CASE("probe-file pathname",        test_probe_file_pathname)
    TEST_CASE("define-modify-macro",        test_define_modify_macro)
    TEST_CASE("defstruct docstring",        test_defstruct_docstring)
    TEST_CASE("compile-file-pathname",      test_compile_file_pathname_bound)
    TEST_CASE("method combinations",        test_method_combinations)
    TEST_CASE("defpackage shadowing order", test_defpackage_shadowing_order)
    TEST_CASE("division-by-zero class",     test_division_by_zero)
    TEST_CASE("clos condition printing",    test_clos_condition_printing)
    TEST_CASE("composite streams",          test_composite_streams)
    TEST_CASE("adjustable string is string", test_adjustable_string)
    TEST_CASE("setf THE / macro place",      test_setf_place_forms)
    TEST_FINALIZE()
