#include <stdio.h>
#include <sysexits.h>

#include "simple_test.h"
#include "variable.h"

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
