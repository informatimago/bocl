#ifndef simple_test_h
#define simple_test_h

#if 0 // def GITLAB_CI

/* https://cunity.gitlab.io/cunit/ */

#include "CUnit/CUnitCI.h"

#else

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <CUnit/Automated.h>
#pragma GCC diagnostic pop

#endif

#include "kernel.h"

typedef bool (*eq_pr)(Ref,Ref);
void simple_test_assert(const char* file, int line, const char* expression, eq_pr eq,Ref expected, Ref value, bool result);


#if 0 // def GITLAB_CI

#define TEST_SUITE_SETUP(name)    CU_SUITE_SETUP()
#define TEST_SUITE_TEARDOWN(name) CU_SUITE_TEARDOWN()

#define TEST_CASE_SETUP(name)     CU_TEST_SETUP()
#define TEST_CASE_TEARDOWN(name)  CU_TEST_TEARDOWN()

#define TEST_INITIALIZE(name)
#define TEST_SUITE(suitename,setup,cleanup) CUNIT_CI_RUN(#suitename,
#define TEST_CASE(name,function)            CUNIT_CI_TEST(name),
#define TEST_FINALIZE()                     );


#define ST(eq,expected,expression) #eq "(" #expected "," #expression ")"

#define TEST_EQ(eq,expected,expression)                                     \
    do{ bool result=eq(expected,expression);                                \
        simple_test_assert(__FILE__,__LINE__,ST(eq,expected,expression),    \
                           eq,expected,expression,result);                  \
        CU_ASSERT(eq(expected,expression));                                 \
    }while(0)

#else

void simple_test_initialize(const char* name);
void simple_test_process_error(unsigned int error);
void* simple_test_check(void* object);
void simple_test_finalize(unsigned int* failure_count);


#define TEST_SUITE_SETUP(name)    static int name(void)
#define TEST_SUITE_TEARDOWN(name) static int name(void)
#define TEST_CASE_SETUP(name)     static int name(void)
#define TEST_CASE_TEARDOWN(name)  static int name(void)

#define TEST_INITIALIZE(name)                                           \
    int main(void){                                                     \
    simple_test_initialize(#name); { do{}while(0)

#define TEST_SUITE(suitename,setup,cleanup)                                              \
    }                                                                                   \
    for(CU_pSuite pSuite = simple_test_check(CU_add_suite(#suitename,setup,cleanup));   \
        pSuite;                                                                         \
        pSuite=NULL){

#define TEST_CASE(name,function)                                         \
    simple_test_check(CU_add_test(pSuite, name, function));

#define TEST_FINALIZE()                                                 \
    }                                                                   \
    unsigned int failure_count;                                         \
    simple_test_finalize(&failure_count);                               \
    printf("failure count = %u\n",failure_count);                       \
    return 0==failure_count ?0 :1;}

#define ST(eq,expected,expression) #eq "(" #expected "," #expression ")"

#define TEST_EQ(eq,expected,expression)                                      \
    do{ Ref __te_exp=(expected); Ref __te_val=(expression);                 \
        bool result=eq(__te_exp,__te_val);                                  \
        simple_test_assert(__FILE__,__LINE__,ST(eq,expected,expression),    \
                           eq,__te_exp,__te_val,result);                    \
        CU_assertImplementation((result), __LINE__,                         \
                                ST(eq,expected,expression),                 \
                                __FILE__, "", CU_FALSE);                    \
    }while(0)

#endif

#endif
