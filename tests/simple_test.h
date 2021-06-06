#ifndef simple_test_h
#define simple_test_h

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <CUnit/Automated.h>
#pragma GCC diagnostic pop

#include "kernel.h"


typedef bool (*eq_pr)(Ref,Ref);

void simple_test_initialize(const char* name);
void simple_test_process_error(CU_ErrorCode error);
void* simple_test_check(void* object);
void simple_test_assert(const char* file, int line, const char* expression, eq_pr eq,Ref expected, Ref value, bool result);
void simple_test_finalize(unsigned int* failure_count);



#define TEST_INITIALIZE(name)                                           \
    simple_test_initialize(#name)

#define TESTSUITE(suitename,setup,cleanup)                                              \
    for(CU_pSuite pSuite = simple_test_check(CU_add_suite(#suitename,setup,cleanup));   \
        pSuite;                                                                         \
        pSuite=NULL)

#define TESTCASE(name,function)                                         \
    simple_test_check(CU_add_test(pSuite, name, function))

#define TEST_FINALIZE(failure_count)                                    \
    simple_test_finalize(&failure_count)

#define ST(eq,expected,expression) #eq "(" #expected "," #expression ")"

#define TESTEQ(eq,expected,expression)                                      \
    do{ bool result=eq(expected,expression);                                \
        simple_test_assert(__FILE__,__LINE__,ST(eq,expected,expression),    \
                           eq,expected,expression,result);                  \
        CU_assertImplementation((result), __LINE__,                         \
                                ST(eq,expected,expression),                 \
                                __FILE__, "", CU_FALSE);                    \
    }while(0)


#endif
