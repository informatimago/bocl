#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>

#include "simple_test.h"
#include "variable.h"
#include "bocl.h"
#include "packages.h"

/* Locate a readable prelude.lisp (its cl-exports.lisp sibling is derived from
   it): $BOCL_PRELUDE, then the compile-time BOCL_TEST_PRELUDE_PATH, then the
   relative fallbacks.  Exits with a clear message if none is found. */
static const char* bocl_test_find_prelude(void){
    const char* candidates[4];
    int n=0;
    const char* env=getenv("BOCL_PRELUDE");
    if(env&&*env){ candidates[n++]=env; }
#ifdef BOCL_TEST_PRELUDE_PATH
    candidates[n++]=BOCL_TEST_PRELUDE_PATH;
#endif
    candidates[n++]="../bootstrap/prelude.lisp";
    candidates[n++]="bootstrap/prelude.lisp";
    for(int i=0;i<n;i++){
        if(access(candidates[i],R_OK)==0){ return candidates[i]; }}
    fprintf(stderr,
            "bocl test: cannot locate prelude.lisp; set BOCL_PRELUDE to its path\n");
    exit(EX_NOINPUT);}

void bocl_test_load_prelude(void){
    /* bocl_load_prelude (not bocl_load_cstring): binds *PACKAGE* to COMMON-LISP
       and loads cl-exports.lisp first, so the reader's bare symbols match
       eval.c's S(DEFMACRO) caches. */
    bocl_load_prelude(bocl_test_find_prelude());}

/* Minimal reader setup for KERNEL-only suites: load cl-exports.lisp only, NOT
   the prelude, so bare symbols resolve to the kernel's registered CL symbols
   (else even (+ 2 3) / (quote a) read as "undefined") while keeping the suite
   focused on the bare kernel.  Implemented here with only the long-standing
   exported entry points (bocl_load_cstring / bocl_in_package) so it never needs
   a freshly-rebuilt libbocl to link — cl-exports.lisp is the sibling of
   prelude.lisp and must be READ with *PACKAGE* = COMMON-LISP (it uses
   %EXPORT-STRINGS), after which we restore *PACKAGE* to CL-USER as main.c does. */
void bocl_test_load_cl_exports(void){
    const char* prelude = bocl_test_find_prelude();
    const char* p = strstr(prelude, "prelude.lisp");
    if(!p){ return; }
    size_t base = (size_t)(p - prelude);
    char* exports = (char*)malloc(base + sizeof("cl-exports.lisp"));
    memcpy(exports, prelude, base);
    strcpy(exports + base, "cl-exports.lisp");
    bocl_in_package(COMMON_LISP_Package);
    bocl_load_cstring(exports);
    bocl_in_package(COMMON_LISP_USER_Package);
    free(exports);}

#ifndef GITLAB_CI

void simple_test_initialize(const char* name){
    CU_set_error_action(CUEA_ABORT);
    if (CUE_SUCCESS != CU_initialize_registry()){
        simple_test_process_error(CU_get_error());}
    CU_set_output_filename(name);}

void simple_test_process_error(unsigned int error){
    if(CUE_SUCCESS!=error){
        fprintf(stderr,"%s\n",CU_get_error_msg());
        exit(EX_SOFTWARE);}}

void* simple_test_check(void* object){
    if(object==NULL){
        CU_cleanup_registry();
        simple_test_process_error(CU_get_error());}
    return object;}

void simple_test_finalize(unsigned int* failure_count){
    /* Run all tests using the CUnit Basic interface */
#ifdef VERBOSE
    CU_basic_set_mode(CU_BRM_VERBOSE);
#else
    CU_basic_set_mode(CU_BRM_NORMAL);
#endif

    /* CU_basic_run_tests(); */

    CU_automated_run_tests();
    CU_list_tests_to_file();

    CU_basic_show_failures(CU_get_failure_list());
    printf("\n");

    (*failure_count)=CU_get_number_of_failure_records();
    CU_cleanup_registry();
    simple_test_process_error(CU_get_error());}

#endif


void simple_test_assert(const char* file, int line, const char* expression,unused eq_pr eq,Ref expected, Ref value, bool result){
    if(!result){
        Ref out=variable_value(S(*STANDARD-OUTPUT*));
        printf("%s:%d: %s FAILED\n",file,line,expression);
        printf("    expected   = ");prin1(expected,out);terpri(out);
        printf("    expression = ");prin1(value,out);terpri(out);}}


/**** THE END ****/

/* See simple_test.h. */
static const char* bocl_test_prefix(void){
    static char prefix[256];
    if(!prefix[0]){
        const char* d = getenv("TMPDIR");
        if(!d || !*d){ d = "/tmp"; }
        snprintf(prefix, sizeof(prefix), "%s/bocl-%ld-", d, (long)getpid());}
    return prefix;}

const char* bocl_test_tmp(const char* name){
    static char bufs[16][512];
    static int k = 0;
    char* b = bufs[k]; k = (k+1)%16;
    snprintf(b, sizeof(bufs[0]), "%s%s", bocl_test_prefix(), name);
    return b;}

/* Expand every "{TMP}/" in TEMPL to the per-process prefix. */
static void bocl_test_expand(const char* templ, char* out, size_t cap){
    const char* pre = bocl_test_prefix();
    size_t o = 0;
    while(*templ){
        if(templ[0]=='{' && strncmp(templ,"{TMP}/",6)==0){
            size_t n = strlen(pre);
            if(o+n>=cap){ fprintf(stderr,"bocl_test_expand: buffer overflow\n"); exit(EX_SOFTWARE); }
            memcpy(out+o, pre, n); o += n;
            templ += 6;
        } else {
            if(o+1>=cap){ fprintf(stderr,"bocl_test_expand: buffer overflow\n"); exit(EX_SOFTWARE); }
            out[o++] = *templ++;}}
    out[o] = '\0';}

const char* bocl_test_path(const char* templ){
    static char bufs[16][512];
    static int k = 0;
    char* b = bufs[k]; k = (k+1)%16;
    bocl_test_expand(templ, b, sizeof(bufs[0]));
    return b;}

Ref bocl_test_eval(const char* form){
    static char bufs[4][16384];
    static int k = 0;
    char* b = bufs[k]; k = (k+1)%4;
    bocl_test_expand(form, b, sizeof(bufs[0]));
    return bocl_read_eval_cstring(b);}
