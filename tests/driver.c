/* driver.c — minimal independent C program that links the installed bocl
 * libraries (libbocl.a / libbocl.so|dylib) and drives the runtime through the
 * public API in <bocl/bocl.h>.  CI compiles and runs this against the *installed*
 * tree, once against the static archive and once against the shared library, as
 * a portability/linkage proof for every build environment.
 *
 *   cc -I$PREFIX/include driver.c $PREFIX/lib/libbocl.a -lm -o driver   # static
 *   cc -I$PREFIX/include driver.c -L$PREFIX/lib -lbocl -lm  -o driver   # shared
 *
 * Exits 0 on success, non-zero on failure.  No prelude is required: PRINT and +
 * are kernel primitives available right after kernel_initialize().
 */
#include <stdio.h>
#include <bocl/bocl.h>   /* pulls in <bocl/kernel.h>: Ref, kernel_initialize() */

int main(void){
    kernel_initialize();
    /* Evaluate a trivial form; PRINT writes "42" to standard output. */
    bocl_read_eval_cstring("(print (+ 40 2))");
    fputs("\n[driver] linked bocl OK\n", stdout);
    return 0;
}
