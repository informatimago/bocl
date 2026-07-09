#ifndef profile_h
#define profile_h
/* profile.h — phase-exclusive wall-clock profiler + event counters for the
   performance program (issue hot-1-profile).

   Zero cost by default: the PROFILE_* macros expand to nothing unless the sources
   are compiled with -DBOCL_PROFILE (e.g. `make build CPPFLAGS=-DBOCL_PROFILE`).
   Then the instrumented sites accumulate EXCLUSIVE time per phase (a phase stack:
   time is charged only to the phase on top, so nested READ/GC pause the enclosing
   EVAL) and bump event counters.  The Lisp interface (BOCL:PROFILE-REPORT /
   -RESET / WITH-PROFILE-PHASE) drives it. */

#include <stdio.h>
#include <stdint.h>

typedef enum {
    PROF_OTHER = 0,   /* startup / unattributed (base of the phase stack) */
    PROF_READ,        /* the reader                                        */
    PROF_MACROEXPAND, /* MACROEXPAND-ALL (bracketed from Lisp)             */
    PROF_EVAL,        /* top-level form evaluation                         */
    PROF_GC,          /* garbage collection                                */
    PROF_CLOS,        /* CLOS setup / dispatch (bracketed from Lisp)       */
    PROF_PHASE_N
} profile_phase_t;

typedef enum {
    PROF_C_ALLOC = 0, /* object allocations */
    PROF_C_INTERN,    /* symbol interns     */
    PROF_C_FUNCALL,   /* function applies   */
    PROF_C_GC,        /* GC runs            */
    PROF_C_FORM,      /* top-level forms    */
    PROF_COUNTER_N
} profile_counter_t;

/* Always available (the Lisp interface calls these; they no-op / report
   "not compiled in" when BOCL_PROFILE is off). */
void profile_reset(void);
void profile_report(FILE* out);
int  profile_enabled(void);

#ifdef BOCL_PROFILE
void profile_enter(profile_phase_t p);
void profile_exit(profile_phase_t p);
void profile_bump(profile_counter_t c);
#  define PROFILE_ENTER(p) profile_enter(p)
#  define PROFILE_EXIT(p)  profile_exit(p)
#  define PROFILE_BUMP(c)  profile_bump(c)
#else
#  define PROFILE_ENTER(p) ((void)0)
#  define PROFILE_EXIT(p)  ((void)0)
#  define PROFILE_BUMP(c)  ((void)0)
#endif

#endif
