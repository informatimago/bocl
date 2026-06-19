/* driver.c — minimal independent C program that links the installed bocl
 * libraries (libbocl.a / libbocl.so|dylib) and drives the runtime through the
 * public API in <bocl/bocl.h>.  CI compiles and runs this against the *installed*
 * tree, once against the static archive and once against the shared library, as
 * a portability/linkage proof for every build environment.
 *
 *   cc -I$PREFIX/include driver.c $PREFIX/lib/libbocl.a -lm -o driver   # static
 *   cc -I$PREFIX/include driver.c -L$PREFIX/lib -lbocl -lm  -o driver   # shared
 *
 * Exits 0 on success, non-zero on failure.  This is a LINKAGE proof, so it uses
 * only the bare-kernel API and does NOT evaluate: on the current kernel the
 * evaluator and primitives like + / PRINT come from the Lisp prelude, which we
 * do not load here.  Reading a literal integer and reading it back through the
 * C accessor is enough to prove the library links and its public API is callable.
 */
#include <stdio.h>
#include <bocl/bocl.h>   /* pulls in <bocl/kernel.h>: Ref, kernel_initialize(), integer_value() */

int main(void){
    kernel_initialize();
    Ref x = bocl_read_cstring("42");        /* the reader works on the bare kernel */
    long v = (long)integer_value(x);
    fprintf(stdout, "[driver] read 42 -> %ld; linked bocl OK\n", v);
    return (v == 42) ? 0 : 1;
}
