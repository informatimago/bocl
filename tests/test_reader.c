#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <sysexits.h>

#include "simple_test.h"

#include "string_list.h"
#include "kernel.h"
#include "bocl.h"
#include "macros.h"
#include "memory.h"
#include "boerror.h"
#include "strdup.h"
#include "stream.h"
#include "printer.h"
#include "reader.h"
#include "variable.h"

/* ============================================================ */
/* We want to unit test the internals: */
#include "reader.c"
/* ============================================================ */

TEST_SUITE_SETUP(reader_init){
    kernel_initialize();
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(reader_clean){
    return CUE_SUCCESS;}


static void test_read_integer(void){
    Ref expected;

    expected=I(1234);
    TEST_EQ(integer_eq,expected,bocl_read_cstring("1234"));
    TEST_EQ(integer_eq,expected,bocl_read_cstring("+1234"));
    TEST_EQ(integer_eq,expected,bocl_read_cstring("  1234   "));
    TEST_EQ(integer_eq,expected,bocl_read_cstring("  +1234  "));
    TEST_EQ(integer_eq,expected,bocl_read_cstring("1234."));
    TEST_EQ(integer_eq,expected,bocl_read_cstring("+1234."));
    TEST_EQ(integer_eq,expected,bocl_read_cstring("  1234.   "));
    TEST_EQ(integer_eq,expected,bocl_read_cstring("  +1234.  "));

    expected=I(-1234);
    TEST_EQ(integer_eq,expected,bocl_read_cstring("-1234"));
    TEST_EQ(integer_eq,expected,bocl_read_cstring("  -1234  "));
    TEST_EQ(integer_eq,expected,bocl_read_cstring("-1234."));
    TEST_EQ(integer_eq,expected,bocl_read_cstring("  -1234.  "));}


static void test_read_float(void){
    Ref expected;

    expected=F(1234.5);
    TEST_EQ(float_eq,expected,bocl_read_cstring("1234.5"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("+1234.5"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  1234.5   "));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  +1234.5  "));

    expected=F(-1234.5);
    TEST_EQ(float_eq,expected,bocl_read_cstring("-1234.5"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  -1234.5  "));

    expected=F(0.5);
    TEST_EQ(float_eq,expected,bocl_read_cstring(".5"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("+.5"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  .5   "));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  +.5  "));

    expected=F(-0.5);
    TEST_EQ(float_eq,expected,bocl_read_cstring("-.5"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  -.5  "));

    expected=F(1234.5e67);
    TEST_EQ(float_eq,expected,bocl_read_cstring("1234.5e67"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("+1234.5e67"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  1234.5e67   "));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  +1234.5e67  "));

    expected=F(-1234.5e67);
    TEST_EQ(float_eq,expected,bocl_read_cstring("-1234.5e67"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  -1234.5e67  "));

    expected=F(0.5e67);
    TEST_EQ(float_eq,expected,bocl_read_cstring(".5e67"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("+.5e67"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  .5e67   "));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  +.5e67  "));

    expected=F(-0.5e67);
    TEST_EQ(float_eq,expected,bocl_read_cstring("-.5e67"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  -.5e67  "));

    expected=F(1234.5e-67);
    TEST_EQ(float_eq,expected,bocl_read_cstring("1234.5e-67"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("+1234.5e-67"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  1234.5e-67   "));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  +1234.5e-67  "));

    expected=F(-1234.5e-67);
    TEST_EQ(float_eq,expected,bocl_read_cstring("-1234.5e-67"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  -1234.5e-67  "));

    expected=F(0.5e-67);
    TEST_EQ(float_eq,expected,bocl_read_cstring(".5e-67"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("+.5e-67"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  .5e-67   "));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  +.5e-67  "));

    expected=F(-0.5e-67);
    TEST_EQ(float_eq,expected,bocl_read_cstring("-.5e-67"));
    TEST_EQ(float_eq,expected,bocl_read_cstring("  -.5e-67  "));}


static void test_read_symbol(void){
    TEST_EQ(eql,S(+),                    bocl_read_cstring("+"));
    TEST_EQ(eql,S(+),                    bocl_read_cstring(" + "));
    TEST_EQ(eql,S(-),                    bocl_read_cstring("-"));
    TEST_EQ(eql,S(-),                    bocl_read_cstring(" - "));

    TEST_EQ(eql,S(SYMBOL),               bocl_read_cstring("symbol"));
    TEST_EQ(eql,S(T),                    bocl_read_cstring("t"));
    TEST_EQ(eql,S(NIL),                  bocl_read_cstring("nil"));
    TEST_EQ(eql,S(A-VERY-NICE-SYMBOL!),  bocl_read_cstring("a-very-nice-symbol!"));
    TEST_EQ(eql,S(AMIXEDCASESYMBOL),     bocl_read_cstring("AMixedCaseSymbol"));

    TEST_EQ(eql,K(KEYWORD),              bocl_read_cstring(":keyword"));
    TEST_EQ(eql,K(T),                    bocl_read_cstring(":t"));
    TEST_EQ(eql,K(NIL),                  bocl_read_cstring(":nil"));
    TEST_EQ(eql,K(A-VERY-NICE-KEYWORD!), bocl_read_cstring(":a-very-nice-keyword!"));
    TEST_EQ(eql,K(AMIXEDCASEKEYWORD),    bocl_read_cstring(":AMixedCaseKeyword"));}


static void test_read_comment(void){
    TEST_EQ(eql,S(IT-WAS-A-COMMENT),
           bocl_read_cstring(";a comment\nit-was-a-comment"));
    TEST_EQ(equal,list(S(BEFORE),S(MID),S(AFTER),NULL),
           bocl_read_cstring("(before ;a comment\n;another comment\nmid;mid comment\nafter) ;it-was-a-comment"));}

static void test_read_character(void){
    TEST_EQ(eql,C(a),                                 bocl_read_cstring("#\\a"));
    TEST_EQ(eql,C(A),                                 bocl_read_cstring("#\\A"));
    TEST_EQ(eql,C(NEWLINE),                           bocl_read_cstring("#\\Newline"));
    TEST_EQ(eql,C(x),                                 bocl_read_cstring("#\\LATIN_SMALL_LETTER_X"));
    TEST_EQ(eql,C(LATIN_CAPITAL_LETTER_A_WITH_GRAVE), bocl_read_cstring("#\\LATIN_CAPITAL_LETTER_A_WITH_GRAVE"));}

static void test_read_list(void){
    TEST_EQ(equal,S(NIL),
           bocl_read_cstring("()"));
    TEST_EQ(equal,cons(I(1234),S(NIL)),
           bocl_read_cstring("(1234)"));
    TEST_EQ(equal,list(I(1),I(22),I(333),I(4444),NULL),
           bocl_read_cstring("(1 22 333 4444)"));
    TEST_EQ(equal,list(S(A),S(LIST),NULL),
           bocl_read_cstring("(a list)"));
    TEST_EQ(equal,list(C(a),S(SLIGHTLY),C(LEFT_PARENTHESIS),S(LONGER),C(QUOTATION_MARK),S(LIST),C(QUOTATION_MARK),C(RIGHT_PARENTHESIS),NULL),
           bocl_read_cstring("(#\\a slightly #\\(longer #\\\" list #\\\" #\\))"));
    TEST_EQ(equal,list(S(A),list(S(RECURSIVE),list(S(AND),list(S(DEEP),
                                                              S(NIL),
                                                              list(S(NIL),NULL),
                                                              list(list(S(NIL),NULL),NULL),
                                                              list(list(list(S(LIST),NULL),NULL),NULL),NULL),
                                                  S(ET),NULL),
                                S(ENCORE),NULL),NULL),
           bocl_read_cstring("(a (recursive (and (deep () (()) ((())) ((((list))))) et) encore))"));}

static void test_read_vector(void){
    /*
    TODO: implement vector indexing to test
    TEST_EQ(eql,expected,bocl_read_cstring("#()"));
    TEST_EQ(eql,expected,bocl_read_cstring("#(1234)"));
    TEST_EQ(eql,expected,bocl_read_cstring("#(1 22 333 4444)"));
    TEST_EQ(eql,expected,bocl_read_cstring("#(a list)"));
    TEST_EQ(eql,expected,bocl_read_cstring("#(#\\a slightly #\\(longer #\\\" list #\\\" #\\))"));
    TEST_EQ(eql,expected,bocl_read_cstring("#(a #(recursive #(and #(deep #() (()) ((())) (((#(vector))))) et) encore))"));
    */}

static void test_read_string(void){
#if 0
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    print(T("a normal string"),out);
    print(bocl_read_cstring("\"a normal string\""),out);
    print(T("\"Hello World\""),out);
    print(bocl_read_cstring("\"\\\"Hello World\\\"\""),out);
    print(T(""),out);
    print(bocl_read_cstring("\"\""),out);
    print(T("X"),out);
    print(bocl_read_cstring("\"X\""),out);
    print(T("A string with \"another string\" in it and some \\escape\\!"),out);
    print(bocl_read_cstring("\"A string with \\\"another string\\\" in it and some \\\\escape\\\\!\""),out);
#endif
    TEST_EQ(string_eq,T("a normal string"), bocl_read_cstring("\"a normal string\""));
    TEST_EQ(string_eq,T("\"Hello World\""), bocl_read_cstring("\"\\\"Hello World\\\"\""));
    TEST_EQ(string_eq,T(""),                bocl_read_cstring("\"\""));
    TEST_EQ(string_eq,T("X"),               bocl_read_cstring("\"X\""));
    TEST_EQ(string_eq,T("A string with \"another string\" in it and some \\escape\\!"),
           bocl_read_cstring("\"A string with \\\"another string\\\" in it and some \\\\escape\\\\!\""));}

static void test_read_quote(void){
    TEST_EQ(equal,list(S(QUOTE),I(42),NULL),            bocl_read_cstring("'42 33"));
    TEST_EQ(equal,list(S(QUOTE),S(QUOTED-SYMBOL),NULL), bocl_read_cstring("'quoted-symbol 33"));
    TEST_EQ(equal,list(S(QUOTE),list(S(QUOTED),S(LIST),NULL),NULL),
           bocl_read_cstring("'(quoted list) 33"));}

static void test_read_bit_vector(void){
    octet bz[]={0};
    octet bo[]={1};
    octet bzz[]={0,0};
    octet boz[]={1,0};
    octet bzo[]={0,1};
    octet boo[]={1,1};
    octet bbig[]={1,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,0,1};
    octet bgib[]={0,1,1,0,1,1,1,0,1,1,1,1,0,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,1,0};
    Ref vb = bit_vector_new_octets(0,bz);
    Ref vbz = bit_vector_new_octets(1,bz);
    Ref vbo = bit_vector_new_octets(1,bo);
    Ref vbzz = bit_vector_new_octets(2,bzz);
    Ref vboz = bit_vector_new_octets(2,boz);
    Ref vbzo = bit_vector_new_octets(2,bzo);
    Ref vboo = bit_vector_new_octets(2,boo);
    Ref vbbig = bit_vector_new_octets(31,bbig);
    Ref vbgib = bit_vector_new_octets(31,bgib);

    TEST_EQ(equal,list(vb,I(1),NULL),
           bocl_read_cstring("(#* 1)"));
    TEST_EQ(equal,list(S(SOME),S(BIT-VECTORS),vbz,vbo,vbzz,vboz,vbzo,vboo,NULL),
           bocl_read_cstring("(some bit-vectors #*0 #*1  #*00 #*10  #*01 #*11)"));
    TEST_EQ(equal,list(S(AND),S(LONGER),S(BIT-VECTORS),vbbig,vbgib,NULL),
         bocl_read_cstring("(and longer  bit-vectors #*1001000100001000010000010000001\n"
                           "#*0110111011110111101111101111110)"));}


TEST_INITIALIZE(test_reader);
TEST_SUITE(test_reader, reader_init, reader_clean)
    TEST_CASE( "reading integers",    test_read_integer)
    TEST_CASE( "reading floats",      test_read_float)
    TEST_CASE( "reading symbols",     test_read_symbol)
    TEST_CASE( "reading comments",    test_read_comment)
    TEST_CASE( "reading characters",  test_read_character)
    TEST_CASE( "reading strings",     test_read_string)
    TEST_CASE( "reading lists",       test_read_list)
    TEST_CASE( "reading vectors",     test_read_vector)
    TEST_CASE( "reading bit-vectors", test_read_bit_vector)
    TEST_CASE( "reading quote",       test_read_quote)
    TEST_FINALIZE()

