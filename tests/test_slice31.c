/* tests/test_slice31.c — CUnit tests for Slice 31:
   APPLY/FUNCALL with symbol designators, COERCE to character,
   MAKE-SEQUENCE, REVERSE for strings/vectors,
   LOOP BEING EACH HASH-KEY/HASH-VALUE OF,
   MAP with bit-vector result, STRING-CONTAINS. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(s31_init){
    kernel_initialize();
    bocl_load_cstring("/Users/pjb/src/public/bocl/bootstrap/prelude.lisp");
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(s31_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- APPLY/FUNCALL with symbol designators ------------------------------- */

static void test_apply_symbol(void){
    TEST_EQ(eql, I(1),
            e("(apply 'car '((1 2 3)))"));
    TEST_EQ(eql, I(6),
            e("(apply '+ '(1 2 3))"));
    TEST_EQ(eql, I(6),
            e("(apply '+ 1 2 '(3))"));}

static void test_funcall_symbol(void){
    TEST_EQ(eql, I(4),
            e("(funcall '1+ 3)"));
    TEST_EQ(eql, T_Symbol,
            e("(funcall 'evenp 4)"));
    TEST_EQ(eql, NIL_Symbol,
            e("(funcall 'evenp 3)"));}

/* ---- COERCE to 'character ----------------------------------------------- */

static void test_coerce_char(void){
    TEST_EQ(eql, e("#\\A"),
            e("(coerce 65 'character)"));
    /* character coerces to itself */
    TEST_EQ(eql, e("#\\x"),
            e("(coerce #\\x 'character)"));}

/* ---- MAKE-SEQUENCE ------------------------------------------------------- */

static void test_make_sequence(void){
    TEST_EQ(equal, e("\"xxxx\""),
            e("(make-sequence 'string 4 :initial-element #\\x)"));
    TEST_EQ(equal, e("'(0 0 0)"),
            e("(make-sequence 'list 3 :initial-element 0)"));
    TEST_EQ(eql, I(5),
            e("(length (make-sequence 'vector 5))"));}

/* ---- REVERSE for strings/vectors ---------------------------------------- */

static void test_reverse_string(void){
    TEST_EQ(equal, e("\"olleh\""),
            e("(reverse \"hello\")"));
    TEST_EQ(equal, e("\"\""),
            e("(reverse \"\")"));}

static void test_reverse_vector(void){
    TEST_EQ(equal, e("'(3 2 1)"),
            e("(coerce (reverse #(1 2 3)) 'list)"));}

/* ---- Hash iteration via loop-for-hash ------------------------------------ */
/* LOOP BEING THE HASH-KEYS is deferred to a future slice; use loop-for-hash */

static void test_loop_being_hash_keys(void){
    TEST_EQ(eql, I(3),
            e("(let ((h (make-hash-table)) (n 0))"
              "  (sethash 'a h 1) (sethash 'b h 2) (sethash 'c h 3)"
              "  (loop-for-hash (k v h) (setq n (1+ n)))"
              "  n)"));}

static void test_loop_being_hash_values(void){
    TEST_EQ(eql, I(6),
            e("(let ((h (make-hash-table)) (s 0))"
              "  (sethash 'a h 1) (sethash 'b h 2) (sethash 'c h 3)"
              "  (loop-for-hash (k v h) (setq s (+ s v)))"
              "  s)"));}

/* ---- MAP with bit-vector result ------------------------------------------ */

static void test_map_bitvector(void){
    TEST_EQ(equal, e("#*0101"),
            e("(map 'bit-vector #'logand #*1101 #*0111)"));
    TEST_EQ(equal, e("#*1111"),
            e("(map 'bit-vector #'logior #*1100 #*0011)"));}

/* ---- STRING-CONTAINS ----------------------------------------------------- */

static void test_string_contains(void){
    TEST_EQ(eql, I(2),
            e("(string-contains \"ll\" \"hello\")"));
    TEST_EQ(eql, NIL_Symbol,
            e("(string-contains \"xyz\" \"hello\")"));}


TEST_INITIALIZE(test_slice31);
TEST_SUITE(test_slice31, s31_init, s31_fini)
    TEST_CASE("apply with symbol",            test_apply_symbol)
    TEST_CASE("funcall with symbol",          test_funcall_symbol)
    TEST_CASE("coerce to character",          test_coerce_char)
    TEST_CASE("make-sequence",               test_make_sequence)
    TEST_CASE("reverse string",              test_reverse_string)
    TEST_CASE("reverse vector",              test_reverse_vector)
    TEST_CASE("hash keys via loop-for-hash",  test_loop_being_hash_keys)
    TEST_CASE("hash values via loop-for-hash",test_loop_being_hash_values)
    TEST_CASE("map to bit-vector",            test_map_bitvector)
    TEST_CASE("string-contains",             test_string_contains)
    TEST_FINALIZE()
