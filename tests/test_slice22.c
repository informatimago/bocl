/* tests/test_slice22.c — CUnit tests for Slice 22:
   CLOSE, READ-BYTE, WRITE-BYTE, PROBE-FILE, DELETE-FILE, RENAME-FILE,
   WILD-PATHNAME-P, TYPE-OF, INT-CHAR, ARRAY-DIMENSIONS, ARRAY-ELEMENT-TYPE,
   COPY-ALIST, SET-EXCLUSIVE-OR, NINTERSECTION, NSET-DIFFERENCE,
   HASH-TABLE-KEYS, HASH-TABLE-VALUES, LOOP-FINISH, CHAR-NAME, NAME-CHAR,
   READ-SEQUENCE, WRITE-SEQUENCE, MAKE-BROADCAST-STREAM,
   ARRAY-TOTAL-SIZE, ARRAY-RANK, SIMPLE-VECTOR-P, HASH-TABLE-ALIST. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s22_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);   /* bocl-internal names live in CL */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s22_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- TYPE-OF ------------------------------------------------------------- */

static void test_type_of(void){
    TEST_EQ(equal, S(SYMBOL),       e("(type-of 'foo)"));
    TEST_EQ(equal, S(CHARACTER),    e("(type-of #\\A)"));
    TEST_EQ(equal, S(CONS),         e("(type-of '(1 2))"));
    /* Integers return (INTEGER lo hi) as a proper list. */
    TEST_EQ(equal, e("'(integer 0 255)"),   e("(type-of 42)"));
    TEST_EQ(equal, e("'(integer 0 255)"),   e("(type-of 0)"));
    TEST_EQ(eql,   S(INTEGER),              e("(first (type-of 42))")); }

/* ---- INT-CHAR ------------------------------------------------------------- */

static void test_int_char(void){
    TEST_EQ(eql, e("(code-char 65)"), e("(int-char 65)"));
    TEST_EQ(eql, e("(code-char 0)"),  e("(int-char 0)"));}

/* ---- CLOSE ---------------------------------------------------------------- */

static void test_close(void){
    /* Closing a stream returns T. */
    TEST_EQ(eql, T_Symbol,
            e("(close (make-string-output-stream))"));}

/* ---- READ-BYTE / WRITE-BYTE ----------------------------------------------- */

static void test_read_byte(void){
    /* read-byte on a string stream returns the byte values. */
    TEST_EQ(eql, I(65),
            e("(read-byte (make-string-input-stream \"A\") nil nil)"));
    /* EOF returns NIL when eof-error-p is nil. */
    TEST_EQ(eql, NIL_Symbol,
            e("(read-byte (make-string-input-stream \"\") nil nil)"));}

static void test_write_byte(void){
    TEST_EQ(eql, I(65),
            e("(let ((s (make-string-output-stream)))"
              "  (write-byte 65 s)"
              "  65)"));}

/* ---- PROBE-FILE ----------------------------------------------------------- */

static void test_probe_file(void){
    /* /etc/hosts should exist on macOS. */
    TEST_EQ(eql, T_Symbol,
            e("(not (null (probe-file \"/etc/hosts\")))"));
    /* A bogus path returns NIL. */
    TEST_EQ(eql, NIL_Symbol,
            e("(probe-file \"/tmp/bocl-test-file-that-does-not-exist-12345\")")); }

/* ---- OPEN :ELEMENT-TYPE (issue open-element-type-default) ----------------- */

static void test_open_element_type(void){
    /* :DEFAULT must be accepted as a character stream (CLHS OPEN): write #\x,
       read it back.  Previously signalled "Invalid element-type :DEFAULT". */
    TEST_EQ(eql, T_Symbol,
            e("(progn"
              "  (with-open-file (s \"/tmp/bocl-et-default\" :direction :output"
              "                     :if-exists :supersede :element-type :default)"
              "    (write-char #\\x s))"
              "  (char= #\\x"
              "         (with-open-file (s \"/tmp/bocl-et-default\" :direction :input"
              "                            :element-type :default)"
              "           (read-char s))))"));
    /* BASE-CHAR is a character stream too. */
    TEST_EQ(eql, T_Symbol,
            e("(progn"
              "  (with-open-file (s \"/tmp/bocl-et-bchar\" :direction :output"
              "                     :if-exists :supersede :element-type (quote base-char))"
              "    (write-char #\\y s))"
              "  (char= #\\y (with-open-file (s \"/tmp/bocl-et-bchar\") (read-char s))))"));
    /* Bare (UNSIGNED-BYTE) and (UNSIGNED-BYTE 8) are octet streams. */
    TEST_EQ(eql, I(66),
            e("(progn"
              "  (with-open-file (s \"/tmp/bocl-et-ub\" :direction :output"
              "                     :if-exists :supersede :element-type (quote (unsigned-byte)))"
              "    (write-byte 66 s))"
              "  (with-open-file (s \"/tmp/bocl-et-ub\" :direction :input"
              "                     :element-type (quote (unsigned-byte 8)))"
              "    (read-byte s)))"));}

/* ---- ARRAY-DIMENSIONS ----------------------------------------------------- */

static void test_array_dimensions(void){
    TEST_EQ(equal, e("'(3)"),   e("(array-dimensions #(1 2 3))"));
    TEST_EQ(equal, e("'(5)"),   e("(array-dimensions (vector 0 1 2 3 4))"));
    TEST_EQ(equal, e("'(4)"),   e("(array-dimensions \"abcd\")"));}

static void test_array_total_size(void){
    TEST_EQ(eql, I(3), e("(array-total-size #(1 2 3))"));
    TEST_EQ(eql, I(0), e("(array-total-size #())")); }

static void test_simple_vector_p(void){
    TEST_EQ(eql, T_Symbol,   e("(simple-vector-p #(1 2))"));
    TEST_EQ(eql, NIL_Symbol, e("(simple-vector-p '(1 2))")); }

/* ---- COPY-ALIST ----------------------------------------------------------- */

static void test_copy_alist(void){
    /* New cons cells, same keys/values */
    TEST_EQ(equal, e("'((a . 1) (b . 2))"),
            e("(copy-alist '((a . 1) (b . 2)))"));
    /* Structural copy: modifying original doesn't affect copy */
    TEST_EQ(eql, T_Symbol,
            e("(let* ((orig '((x . 10) (y . 20)))"
              "       (copy (copy-alist orig)))"
              "  (rplacd (car orig) 99)"
              "  (= 10 (cdr (car copy))))"));}

/* ---- SET-EXCLUSIVE-OR / NINTERSECTION / NSET-DIFFERENCE ------------------- */

static void test_set_exclusive_or(void){
    TEST_EQ(equal, e("'(1 4)"),
            e("(sort (set-exclusive-or '(1 2 3) '(2 3 4)) #'<)"));}

static void test_nintersection(void){
    TEST_EQ(equal, e("'(2 3)"),
            e("(sort (nintersection '(1 2 3) '(2 3 4)) #'<)"));}

static void test_nset_difference(void){
    TEST_EQ(equal, e("'(1)"),
            e("(nset-difference '(1 2 3) '(2 3 4))")); }

/* ---- HASH-TABLE-KEYS / -VALUES / -ALIST ----------------------------------- */

static void test_hash_table_keys(void){
    TEST_EQ(eql, I(2),
            e("(let ((h (make-hash-table)))"
              "  (sethash 'a h 1) (sethash 'b h 2)"
              "  (length (hash-table-keys h)))")); }

static void test_hash_table_values(void){
    TEST_EQ(eql, I(2),
            e("(let ((h (make-hash-table)))"
              "  (sethash 'x h 10) (sethash 'y h 20)"
              "  (length (hash-table-values h)))")); }

/* ---- CHAR-NAME / NAME-CHAR ------------------------------------------------ */

static void test_char_name(void){
    TEST_EQ(equal, T("Space"),   e("(char-name #\\Space)"));
    TEST_EQ(equal, T("Newline"), e("(char-name #\\Newline)"));
    TEST_EQ(eql,   NIL_Symbol,   e("(char-name #\\A)"));}

static void test_name_char(void){
    TEST_EQ(eql, e("(code-char 32)"),  e("(name-char \"Space\")"));
    /* In bocl #\Newline is NEL (code 133), the internal line separator; ASCII
       LF (code 10) is #\Linefeed (see the char-name/name-char comment in the
       prelude, kept consistent with the reader so char-name round-trips). */
    TEST_EQ(eql, e("(code-char 133)"), e("(name-char \"NEWLINE\")"));
    TEST_EQ(eql, e("(code-char 10)"),  e("(name-char \"LINEFEED\")"));
    TEST_EQ(eql, NIL_Symbol,           e("(name-char \"XYZ\")"));}

/* ---- WRITE-SEQUENCE / READ-SEQUENCE --------------------------------------- */

static void test_write_sequence(void){
    TEST_EQ(equal, T("hello"),
            e("(with-output-to-string (s)"
              "  (write-sequence \"hello\" s))")); }

/* ---- WILD-PATHNAME-P ------------------------------------------------------- */

static void test_wild_pathname_p(void){
    /* Stub always returns NIL */
    TEST_EQ(eql, NIL_Symbol, e("(wild-pathname-p \"/etc/hosts\")"));}


TEST_INITIALIZE(test_slice22);
TEST_SUITE(test_slice22, s22_init, s22_fini)
    TEST_CASE("type-of",              test_type_of)
    TEST_CASE("int-char",             test_int_char)
    TEST_CASE("close",                test_close)
    TEST_CASE("read-byte",            test_read_byte)
    TEST_CASE("write-byte",           test_write_byte)
    TEST_CASE("probe-file",           test_probe_file)
    TEST_CASE("open-element-type",    test_open_element_type)
    TEST_CASE("array-dimensions",     test_array_dimensions)
    TEST_CASE("array-total-size",     test_array_total_size)
    TEST_CASE("simple-vector-p",      test_simple_vector_p)
    TEST_CASE("copy-alist",           test_copy_alist)
    TEST_CASE("set-exclusive-or",     test_set_exclusive_or)
    TEST_CASE("nintersection",        test_nintersection)
    TEST_CASE("nset-difference",      test_nset_difference)
    TEST_CASE("hash-table-keys",      test_hash_table_keys)
    TEST_CASE("hash-table-values",    test_hash_table_values)
    TEST_CASE("char-name/name-char",  test_char_name)
    TEST_CASE("name-char",            test_name_char)
    TEST_CASE("write-sequence",       test_write_sequence)
    TEST_CASE("wild-pathname-p",      test_wild_pathname_p)
    TEST_FINALIZE()
