#ifndef conditions_h
#define conditions_h
#include "kernel.h"

/* Phase 4.2 minimum condition system.

   What's in: SIGNAL, ERROR, WARN, HANDLER-CASE, MAKE-CONDITION. The
   class hierarchy (CONDITION → SERIOUS-CONDITION → ERROR / TYPE-ERROR /
   ARITHMETIC-ERROR / …, CONDITION → WARNING / SIMPLE-WARNING) is wired
   up in kernel_objects.c; here we just give those classes Condition-
   shaped instances and a SIGNAL/handler-walk mechanism.

   What's NOT in (deferred to a follow-on commit):
   - HANDLER-BIND (runs the handler in the dynamic context of SIGNAL;
     needs more frame plumbing than the simple unwind that HANDLER-CASE
     does).
   - Restarts: RESTART-CASE, RESTART-BIND, FIND-RESTART, INVOKE-RESTART.
   - CERROR (would be trivial once restarts work). */

void kernel_initialize_conditions(void);

Ref make_simple_condition(Ref class, Ref format_control, Ref format_arguments);

/* Signal `cond` by walking env's frames for the first matching HANDLER-CASE
   clause. If found, longjmps; if no handler matches, returns NIL (per CL
   SIGNAL semantics). ERROR wraps this with an unhandled-error fallback. */
Ref signal_condition(Ref cond, Ref environment);

#endif
