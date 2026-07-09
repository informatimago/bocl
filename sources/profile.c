/* profile.c — phase-exclusive wall-clock profiler (issue hot-1-profile).
   See profile.h.  Active only under -DBOCL_PROFILE; otherwise the enter/exit/bump
   entry points are empty and PROFILE_REPORT says so.  CLOCK_MONOTONIC is POSIX
   (_POSIX_C_SOURCE=200809L is set by the build). */

#include "profile.h"
#include <time.h>
#include <string.h>

static const char* phase_names[PROF_PHASE_N] = {
    "other", "read", "macroexpand", "eval", "gc", "clos" };
static const char* counter_names[PROF_COUNTER_N] = {
    "allocations", "interns", "funcalls", "gc-runs", "forms" };

#ifdef BOCL_PROFILE

static uint64_t phase_ns[PROF_PHASE_N];
static uint64_t counters[PROF_COUNTER_N];

#define PSTACK_MAX 1024
static profile_phase_t pstack[PSTACK_MAX];
static int             psp = 0;               /* pstack[0] == PROF_OTHER (base) */
static struct timespec last_switch;
static int             started = 0;

static uint64_t ns_of(struct timespec* ts){
    return (uint64_t)ts->tv_sec * 1000000000ull + (uint64_t)ts->tv_nsec; }

static void ensure_started(void){
    if(!started){
        started = 1; psp = 0; pstack[0] = PROF_OTHER;
        clock_gettime(CLOCK_MONOTONIC, &last_switch); } }

/* Charge the time since the last switch to the phase currently on top. */
static void account(void){
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t n = ns_of(&now), p = ns_of(&last_switch);
    phase_ns[pstack[psp]] += (n >= p) ? (n - p) : 0;
    last_switch = now; }

void profile_enter(profile_phase_t ph){
    ensure_started(); account();
    if(psp + 1 < PSTACK_MAX){ pstack[++psp] = ph; } }

void profile_exit(profile_phase_t ph){ (void)ph;
    ensure_started(); account();
    if(psp > 0){ psp--; } }

void profile_bump(profile_counter_t c){
    if((int)c < PROF_COUNTER_N){ counters[c]++; } }

void profile_reset(void){
    memset(phase_ns, 0, sizeof phase_ns);
    memset(counters, 0, sizeof counters);
    started = 0; ensure_started(); }

void profile_report(FILE* out){
    account();  /* flush the phase currently running */
    uint64_t total = 0;
    for(int i = 0; i < PROF_PHASE_N; i++){ total += phase_ns[i]; }
    fprintf(out, "; ---- bocl profile (phase-exclusive wall time) ----\n");
    for(int i = 0; i < PROF_PHASE_N; i++){
        double s   = (double)phase_ns[i] / 1e9;
        double pct = total ? 100.0 * (double)phase_ns[i] / (double)total : 0.0;
        fprintf(out, ";   %-12s %10.3f s  %5.1f%%\n", phase_names[i], s, pct); }
    fprintf(out, ";   %-12s %10.3f s\n", "TOTAL", (double)total / 1e9);
    fprintf(out, "; ---- counters ----\n");
    for(int i = 0; i < PROF_COUNTER_N; i++){
        fprintf(out, ";   %-12s %llu\n",
                counter_names[i], (unsigned long long)counters[i]); } }

int profile_enabled(void){ return 1; }

#else /* !BOCL_PROFILE */

void profile_reset(void){}
void profile_report(FILE* out){
    fprintf(out, "; bocl profile: not compiled in "
                 "(rebuild with CPPFLAGS=-DBOCL_PROFILE)\n");
    (void)phase_names; (void)counter_names; }
int  profile_enabled(void){ return 0; }

#endif
