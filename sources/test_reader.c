#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <sysexits.h>
#include "string_list.h"
#include "kernel.h"
#include "bocl.h"
#include "macros.h"
#include "memory.h"
#include "error.h"
#include "strdup.h"
#include "stream.h"
#include "printer.h"
#include "reader.h"
#include "variable.h"

/* ============================================================ */
/* We want to unit test the internals: */
#include "reader.c"
/* ============================================================ */


static void test_reader(void){

    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    print(bocl_read_cstring("1234"),out);
    print(bocl_read_cstring("+1234"),out);
    print(bocl_read_cstring("-1234"),out);
    print(bocl_read_cstring("  1234   "),out);
    print(bocl_read_cstring("  +1234  "),out);
    print(bocl_read_cstring("  -1234  "),out);

    print(bocl_read_cstring("1234."),out);
    print(bocl_read_cstring("+1234."),out);
    print(bocl_read_cstring("-1234."),out);
    print(bocl_read_cstring("  1234.   "),out);
    print(bocl_read_cstring("  +1234.  "),out);
    print(bocl_read_cstring("  -1234.  "),out);

    print(bocl_read_cstring("1234.5"),out);
    print(bocl_read_cstring("+1234.5"),out);
    print(bocl_read_cstring("-1234.5"),out);
    print(bocl_read_cstring("  1234.5   "),out);
    print(bocl_read_cstring("  +1234.5  "),out);
    print(bocl_read_cstring("  -1234.5  "),out);

    print(bocl_read_cstring(".5"),out);
    print(bocl_read_cstring("+.5"),out);
    print(bocl_read_cstring("-.5"),out);
    print(bocl_read_cstring("  .5   "),out);
    print(bocl_read_cstring("  +.5  "),out);
    print(bocl_read_cstring("  -.5  "),out);

    print(bocl_read_cstring("1234.5e67"),out);
    print(bocl_read_cstring("+1234.5e67"),out);
    print(bocl_read_cstring("-1234.5e67"),out);
    print(bocl_read_cstring("  1234.5e67   "),out);
    print(bocl_read_cstring("  +1234.5e67  "),out);
    print(bocl_read_cstring("  -1234.5e67  "),out);

    print(bocl_read_cstring(".5e67"),out);
    print(bocl_read_cstring("+.5e67"),out);
    print(bocl_read_cstring("-.5e67"),out);
    print(bocl_read_cstring("  .5e67   "),out);
    print(bocl_read_cstring("  +.5e67  "),out);
    print(bocl_read_cstring("  -.5e67  "),out);

    print(bocl_read_cstring("1234.5e-67"),out);
    print(bocl_read_cstring("+1234.5e-67"),out);
    print(bocl_read_cstring("-1234.5e-67"),out);
    print(bocl_read_cstring("  1234.5e-67   "),out);
    print(bocl_read_cstring("  +1234.5e-67  "),out);
    print(bocl_read_cstring("  -1234.5e-67  "),out);

    print(bocl_read_cstring(".5e-67"),out);
    print(bocl_read_cstring("+.5e-67"),out);
    print(bocl_read_cstring("-.5e-67"),out);
    print(bocl_read_cstring("  .5e-67   "),out);
    print(bocl_read_cstring("  +.5e-67  "),out);
    print(bocl_read_cstring("  -.5e-67  "),out);

    print(bocl_read_cstring("symbol"),out);
    print(bocl_read_cstring("t"),out);
    print(bocl_read_cstring("nil"),out);
    print(bocl_read_cstring("a-very-nice-symbol!"),out);
    print(bocl_read_cstring("AMixedCaseSymbol"),out);

    print(bocl_read_cstring(":keyword"),out);
    print(bocl_read_cstring(":t"),out);
    print(bocl_read_cstring(":nil"),out);
    print(bocl_read_cstring(":a-very-nice-keyword!"),out);
    print(bocl_read_cstring(":AMixedCaseKeyword"),out);

    print(bocl_read_cstring(";a comment\nit-was-a-comment"),out);

    print(bocl_read_cstring("#\\a"),out);
    print(bocl_read_cstring("#\\A"),out);
    print(bocl_read_cstring("#\\Newline"),out);
    print(bocl_read_cstring("#\\LATIN_SMALL_LETTER_X"),out);
    print(bocl_read_cstring("#\\LATIN_CAPITAL_LETTER_A_WITH_GRAVE"),out);

    print(bocl_read_cstring("()"),out);
    print(bocl_read_cstring("(1234)"),out);
    print(bocl_read_cstring("(1 22 333 4444)"),out);
    print(bocl_read_cstring("(a list)"),out);
    print(bocl_read_cstring("(#\\a slightly #\\(longer #\\\" list #\\\" #\\))"),out);
    print(bocl_read_cstring("(a (recursive (and (deep () (()) ((())) ((((list))))) et) encore))"),out);

    print(bocl_read_cstring("#()"),out);
    print(bocl_read_cstring("#(1234)"),out);
    print(bocl_read_cstring("#(1 22 333 4444)"),out);
    print(bocl_read_cstring("#(a list)"),out);
    print(bocl_read_cstring("#(#\\a slightly #\\(longer #\\\" list #\\\" #\\))"),out);
    print(bocl_read_cstring("#(a #(recursive #(and #(deep #() (()) ((())) (((#(vector))))) et) encore))"),out);

    print(bocl_read_cstring("\"Hello World\""),out);
    print(bocl_read_cstring("\"\""),out);
    print(bocl_read_cstring("\"X\""),out);
    print(bocl_read_cstring("\"A string with \\\"another string\\\" in it, and some \\escape\\!\""),out);

    print(bocl_read_cstring("'42 33"),out);
    print(bocl_read_cstring("'quoted-symbol 33"),out);
    print(bocl_read_cstring("'(quoted list) 33"),out);
    print(bocl_read_cstring("'(quoted list) 33"),out);

    print(bocl_read_cstring("(some bit-vectors #*0 #*1  #*00 #*10  #*01 #*11)"),out);
    print(bocl_read_cstring("(and longer  bit-vectors #*1001000100001000010000010000001\n"
                            "#*0110111011110111101111101111110)"),out);

    terpri(out);
}

int main(){
     kernel_initialize();
     test_reader();
     return 0;}
