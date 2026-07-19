---
name: bytecode-vm-and-compiler
description: >
  Knowledge transfer for planning, designing, implementing, integrating and
  debugging a byte-code VM + compiler inside an existing Lisp interpreter.
  Distilled from the bocl plan-2 project (vm-1..vm-10 + hot-1..hot-6, branch
  `vm`, v1.8.1→v1.8.50), addressed to an agent adding a byte-code compiler to
  an AutoLISP implementation (lisp-1, pure dynamic binding, e.g. clautolisp).
  Read this BEFORE writing any plan of your own.
---

# Byte-code VM + compiler: how to actually pull it off

You are adding a byte-code compiler and VM to an existing interpreter.  This
was done once already, in bocl (a bootstrap Common Lisp, C17 kernel + Lisp
prelude, tree-walking interpreter).  Outcome there: 10–16× on op-bound code,
3.0× boot time via a compiled boot image, all ~44 test suites green, compiler
self-hosted.  Everything below is what made that work, what nearly sank it,
and what changes when the source language is a lisp-1 with pure dynamic
binding.

## 0. The three rules that dominate everything else

1. **The interpreter is the oracle.**  Never remove or bypass it.  Every
   compiled behavior is validated by running the same form through both paths
   and comparing (see §7 "parity harness").  Interpreter bugs found this way
   get fixed FIRST, then the compiler is held to the fixed behavior.
2. **Decline-and-fallback.**  From day one, the compiler must be allowed to
   say "I can't compile this" and hand the form back to the interpreter —
   per FORM, per FUNCTION, silently correct.  This is what makes an
   incremental bring-up shippable at every commit: the system is never
   broken, it is only "not yet faster" on some forms.  Track declines (a
   counter + a debug listing of declined operators) so coverage gaps are
   data, not surprises.  (bocl: `%vm-compile-file-form` declines; the gaps
   ticket vm-analyzer-decline-gaps.issue catalogs what remains.)
3. **One staged plan, one acceptance test per stage, full suite as gate.**
   The bocl staging (adapt names to your project) —
   - vm-1: ISA spec + compiled-function object layout (no execution).
   - vm-2: VM dispatch core in the host language, executing hand-assembled
     code (tests feed byte vectors directly).
   - vm-3: compiler front-end — macroexpand-all + analysis to a small IR
     with resolved variable addresses; unit-tested WITHOUT codegen.
   - vm-4: codegen core (atoms, calls, if, progn, let, setq, lambda) —
     `(compile nil '(lambda (x) (+ x 1)))` runs on the VM.
   - vm-5: control flow / non-local exit (block, go, catch, protect, mv).
   - vm-6: closures + calling-convention fast path (skip closures for
     AutoLISP — §6).  vm-6b: tail calls + fast apply.
   - vm-7: call-site caches for late-bound dispatch (CLOS there; for a
     lisp-1, symbol-function caching — §6).
   - vm-8: fasl format — serialize compiled functions; compile-file/load.
   - vm-9: debugger integration (VM frames in backtraces), self-host,
     benchmark honestly.
   - vm-10: compiled boot image (the prelude/runtime library itself
     compiled and loaded at startup).
   Then a measurement-driven optimization pass (hot-1..hot-6 in bocl:
   profile first, lexical addressing, frame representation, arg-list
   avoidance, dispatch caches, benchmark/validate).  Do NOT interleave
   optimization into the bring-up stages.

## 1. Planning: write the plan as a handoff document

The single most valuable artifact in bocl was a plan file
(issues/open/vm-COMPILER-PLAN.md) written as if for a DIFFERENT person:
status ledger, conventions (branch, version bump per commit, test gate
commands), ISA summary with a pointer to the authoritative header, LOCKED
design decisions ("do not relitigate"), the IR sketch, per-stage
deliverable + acceptance + file map, and a gotchas section that grew as
lessons were learned.  Keep it updated at every stage boundary; it is what
lets any session (or any other agent) resume cold.  Copy that structure.

## 2. ISA design (what worked, verbatim)

- **Stack machine.**  1-byte opcode + inline operands.  Operand encodings:
  `u8` for small counts; LEB128 unsigned varint (`uvar`) for pool/local
  indices; zig-zag LEB128 signed varint (`svar`) for jump displacements
  relative to the byte AFTER the instruction.
- **Opcode set is APPEND-ONLY** and versioned (`BYTECODE_VERSION` checked at
  fasl load).  bocl's 31 ops: PUSH_CONST/NIL/T, DUP, POP, DROP n,
  LOAD/STORE_LOCAL depth idx, LOAD/STORE_GLOBAL sym, LOAD_FUNCTION sym,
  JUMP, JUMP_IF_NIL, JUMP_IF_NOT_NIL, CALL n, CALL_NAMED sym n, TAIL_CALL n,
  RETURN, VALUES n, MV_CALL, MV_BIND n, BLOCK_OPEN, RETURN_FROM, CATCH_OPEN,
  THROW, UNWIND_PROTECT, UNWIND_LEAVE, MAKE_CLOSURE, LOAD_UPVAL, NOP, HALT.
  A lisp-1/dynamic target needs BIND/UNBIND instead of most of the local
  ops (§6) and no closure ops.
- **Compiled-function object**: name, lambda-list (for the debugger and
  arg checking), code (byte vector), constant pool (simple vector), packed
  arity word (req/opt/rest/key flags), max-stack, n-locals, and THE SOURCE
  LAMBDA (invaluable: disassembly, re-compilation, debugging).
- **Value representation: the interpreter's, unchanged.**  VM stack, locals
  and constants hold the same tagged values the interpreter uses.  Zero
  conversion between worlds.  Do not invent a second representation.
- **Dispatch loop**: portable `switch` as the default; computed-goto only
  behind a compiler-feature #ifdef.  The switch was never the bottleneck.

## 3. Compiler pipeline (front-end / IR / codegen)

- **Front-end**: run macroexpand-all first so analysis sees only special
  operators and calls.  (In a dynamically-scoped lisp-1 there are fewer
  special operators — the analysis is simpler, do not skip it.)
- **Small list-based IR**, one node kind per special operator, with variable
  references RESOLVED by the analyzer (constant / local slot / global).
  The IR earns its keep because the analyzer is unit-testable without any
  codegen, and codegen becomes a straight fold.  bocl node forms:
  `(:const v) (:lref depth idx) (:gref sym) (:lset ...) (:gset ...)
  (:if t e1 e2) (:progn ...) (:call fn args...) (:call-named sym args...)
  (:lambda ...) (:let ...) (:block ...) (:tagbody ...)` etc.
- **Flat-locals model** (bocl's locked decision #1): every binding in one
  function body gets a distinct slot in ONE frame; nested `let` consumes
  more slots, never a new frame.  n-locals = max slot high-water.  For pure
  dynamic binding this becomes mostly moot (§6) — but keep the idea for
  compiler-internal temporaries.
- **Emitter**: growable byte buffer + emit-op/emit-uvar/emit-svar, a
  constant-pool hash (object → index), and a simulated stack-depth counter
  to compute max-stack while emitting.
- **Jumps**: two-pass label resolution (first pass records label positions
  with placeholder widths, second pass encodes) — chosen over fixed-width
  reservations in bocl; "correctness over density".
- **Uniform call path FIRST** (locked decision #3): initially compile CALL
  to "build an argument list, go through the interpreter's own apply".
  One code path calls compiled, built-in, and interpreted functions
  interchangeably — interop works on day one.  The stack-based fast
  calling convention, the no-C-recursion trampoline and the VM
  stack-overflow condition come later (vm-6/6b), as measured optimizations.
- **Compiler lives in Lisp** (the implemented language, not the host), in
  its own module loaded at the end of the runtime library, so the final
  stage can compile the compiler with itself (self-host is the best
  coverage test you will ever run).

## 4. Non-local exit — mirror the interpreter's machinery, do not invent

The VM's block/catch/unwind-protect handlers push the SAME dynamic frame
records (with setjmp targets, in bocl's case) that the interpreter pushes,
onto the SAME global frame stack.  Consequences that only surface if you
get this wrong:

- Cross-world unwinds work: interpreted code can THROW through compiled
  frames and vice versa, cleanups run in order.
- The condition system sees one stack.  bocl bug to not repeat: the
  "find the top live frame to longjmp through" helper had to learn to skip
  VM marker frames that carry no setjmp target (`frame_top_live`).
- Exits from expired extents must signal catchable, descriptive conditions
  (bocl: RETURN-FROM-EXITED-BLOCK-ERROR / GO-EXITED-TAGBODY-ERROR,
  PROGRAM-ERROR subclasses, signaled BEFORE unwinding so a handler can take
  another exit).  Compiled and interpreted paths must both do this — it is
  ticket-tested (escaping-return-from).
- In a dynamically-bound lisp, EVERY unwind must also restore the dynamic
  bindings made since the target frame (§6) — put the unbind count in the
  frame record when it is pushed.

## 5. Fasl + compiled boot image (vm-8 / vm-10)

- Serialize compiled functions through the SAME object serializer the rest
  of the system uses (bocl: cfasl streaming model), by appending new tags;
  bump the format version; keep a magic header and an explicit EOF tag as
  the final byte — a TRUNCATED fasl must be detected at load, not half-load
  (bocl shipped that bug once: LOOP broke from a cut-off image).
- Text-fasl fallback: when the binary writer meets a literal it cannot
  serialize, fall back to a readable text fasl for that file (print with
  circularity handling).  Never fail the compile for a literal.
- COMPILE-FILE **bootstrap rule** (vm-10, learned through 3 boot-order
  bugs): when compiling the runtime library itself, macroexpand ONLY the
  function-defining forms and keep everything else as raw source evaluated
  at load.  Expanding everything at compile time bakes in compile-session
  state ("two-timeline mismatch") and breaks the NEXT boot.  Equivalent
  trap in any system whose macros are defined by the very file being
  compiled.
- Compile-time-too semantics: eval-when/compile-toplevel effects must fire
  during compile-file (macros defined mid-file are used later in the file).
- COMPILE-FILE must bind the compile-pathname variables of your dialect —
  and per-form errors should DECLINE that form (collect verbatim), not
  abort the file.

## 6. THE ADAPTATION: lisp-1 + pure dynamic binding (AutoLISP)

This is where the bocl design must be re-derived, not copied.

- **Single namespace**: there is no separate function cell.  CALL_NAMED
  becomes "load the symbol's VALUE, apply it".  LOAD_FUNCTION disappears.
  A call-site cache (vm-7's analog) caches the symbol→value lookup, but —
  since any SETQ can redefine a function — the cache must be invalidated on
  symbol assignment: cheapest is a global "definition generation" counter
  bumped by setq-of-a-cached-symbol, checked by the cache.  Measure before
  building anything fancier; in bocl the per-call-site cache only paid off
  for CLOS dispatch, while plain calls were already one hash lookup.
- **Pure dynamic binding — use shallow binding.**  Each symbol has ONE
  value cell.  Binding = push the old cell value onto a binding stack
  (symbol, old-value), store the new; unbinding = pop and restore.  Two
  opcodes: `BIND sym` (pops the init value off the operand stack) and
  `UNBIND n`.  Variable reference is just the value cell read — as fast as
  a global.  This is the classical Lisp 1.5 / Interlisp model (Baker 1978
  analyses it); it beats deep binding for read-heavy code, which is the
  common case.  Consequences:
  - The compiler does NOT resolve variables to frame slots — there are no
    lexical addresses.  The analyzer still classifies (bound-here vs free)
    for diagnostics, but codegen emits symbol reads.
  - NO closures, upvalues, displays, or capture analysis: a lambda
    compiles to a plain template; free variables resolve at CALL time in
    the dynamic environment.  This deletes vm-6 (the hardest bocl stage)
    from your plan.  Say so in the plan explicitly.
  - EVERY exit path must run the unbinds: normal returns, tail calls
    (unbind before the jump!), and non-local exits (record the binding
    stack depth in each NLX frame; unwind restores to it).  Your
    while/foreach/error-handler equivalents are all binding boundaries.
  - Recursion works naturally (each activation pushes/pops), but detect
    the pathological case in tests: a function SETQing a variable it also
    binds — semantics differ from lexical Lisps; parity with your
    interpreter is the spec, not CL intuition.
  - VM local slots remain useful for compiler-generated temporaries that
    no user code can see (loop counters, mv spills) — those are safe to
    keep frame-local since nothing can capture them.
- **Interpreter parity includes the dialect switches**: clautolisp has
  dialect-controlled behavior (strict AutoLISP vs extensions WARN, dynamic
  `*AUTOLISP-DIALECT*`).  Compiled code must consult the SAME dialect
  machinery at the same points — compile-time constant-folding of
  dialect-dependent behavior is a bug.

## 7. Debugging toolkit (build these BEFORE you need them)

- **Parity harness**: a helper `(vm-cmp lam args)` = "compile lam, apply to
  args, compare with interpreting the same" used in dozens of tests, plus
  corpus runs (compile a real library file, run its test suite compiled).
  bocl's test names to steal: vm execute / analyze / codegen / control /
  values / nlx / closures / fasl / conditions / restarts / escaping blocks
  / exited extent / tail calls / fast-apply / inline primitives / clos
  cache / backtrace / disassemble / selfhost.
- **DISASSEMBLE from day vm-2** (opcode names, operands, constant-pool
  values, source).  You will read disassembly constantly; it is not
  optional polish.
- **IR pretty-printer** so the analyzer can be eyeballed before codegen
  exists.
- **Fasl load tracer** behind an env var (bocl: BOCL_CFASL_DEBUG=1 prints
  one line per loaded top-level op).  When a fasl half-loads or corrupts
  state, this locates the failing form in seconds; without it you bisect
  for an hour.
- **Keep-the-process-alive workflow**: when a bug needs many probes, drive
  one live REPL/VM over a pipe or ssh rather than restarting a fresh
  process per probe (boot time dominates otherwise).
- **XFAIL quarantine** in the test harness: a known-failing test is listed,
  visible, and does not block the gate — beats commenting tests out.
- **Watch out for heisenbugs = memory bugs.**  If a failure MOVES when you
  add prints or reorder files, stop chasing the symptom: it is (in a
  GC'd host) a rooting bug or (in any host) uninitialized state.  Confirm
  by disabling GC (bocl: BOCL_NOGC=1) or running a sanitizer, then hunt
  the real cause.  bocl's poison mode (quarantine freed blocks + record
  the freeing backtrace) found several missing-root bugs mechanically.
- **Runaway processes**: interpreters in a tight loop may ignore SIGTERM —
  wrap every scripted run in `timeout -s KILL` with bounded loops.

## 8. Performance truths (measure, then believe)

- In the tree-walker, PER-CALL OVERHEAD dominates everything (~150 µs/call
  in bocl before work): the wins are (a) not re-traversing source, (b) not
  re-consing argument lists, (c) cheap variable access.  Allocation was
  NOT the bottleneck — do not start with an allocator rewrite.
- Expected shape of results (bocl measured): 10–16× on op-bound loops,
  3–4× on call-heavy code, ~3× boot with a compiled image; GC becomes the
  new limiter on allocation-heavy code only AFTER these wins.
- **Benchmark in fresh processes** — a long-lived process's heap layout and
  caches skew A/B numbers; bocl re-learned this the hard way.
- Profilers that instrument every op (callgrind style) are useless when a
  huge boot dominates; use A/B wall-clock plus cheap built-in counters
  (allocations, calls, GC runs) toggled by a build flag.
- Tail calls matter more in a VM than you think (they also bound frame
  growth); implement TAIL_CALL in the bring-up, optimize later.

## 9. Integration discipline

- Small commits, each leaving the full suite green; version bumped every
  code commit with the version in the subject; per-stage issues moved
  open→closed with a RESOLUTION appendix documenting what was actually
  done (deviations included).  The closed issues become the project's
  memory — this very file was distilled from them.
- File a ticket for every bug found while doing something else (symptom,
  repro, cause if known) instead of drive-by fixing or forgetting.
- Never let the compiled path change observable semantics to "better than
  the interpreter": fix the interpreter first, then match it.

## 10. References

In the bocl repository (gitlab.com/bocl/bocl, branch `vm`):
- `issues/open/vm-COMPILER-PLAN.md` — the full staged plan + gotchas; the
  template for YOUR plan document.
- `sources/bytecode.h` / `sources/bytecode.c` — ISA spec, object model,
  the VM loop (`bytecode_apply`), varint readers.
- `bootstrap/vm-compiler.lisp` — analyzer, emitter, codegen, decline
  logic (~1750 lines of implemented compiler to crib from).
- `sources/cfasl.{h,c}` + `issues/closed/vm-8-bytecode-fasl.issue` — fasl.
- `issues/closed/vm-1-*.issue` … `vm-10-compiled-prelude-image.issue`,
  `hot-1-*.issue` … `hot-6-benchmark-validate.issue` — per-stage
  resolutions with measurements and the bugs each stage hit.
- `issues/closed/vm-escaping-return-from.issue`, `vm-conditions.issue` —
  NLX/condition integration details.
- `tests/test_misc.c` (the vm* test battery) — acceptance-test patterns.

Literature:
- P. Norvig, *Paradigms of AI Programming*, ch. 23 "Compiling Lisp" — a
  complete Lisp→byte-code compiler + stack VM in ~50 pages; the closest
  published relative of this design.
- C. Queinnec, *Lisp in Small Pieces* — ch. 6–7 (fast interpretation,
  compilation to byte-code), ch. 2 (lisp-1 vs lisp-2, dynamic binding
  semantics).
- Abelson & Sussman, *SICP* ch. 5 — register machines and the
  explicit-control evaluator (the conceptual bridge interpreter→VM).
- H. G. Baker, "Shallow Binding in Lisp 1.5" (CACM 21(7), 1978) — the
  binding model recommended in §6, with the rerooting analysis.
- R. P. Gabriel & K. M. Pitman, "Technical Issues of Separation in
  Function Cells and Value Cells" (LASC 1(1), 1988) — lisp-1/lisp-2
  consequences you must respect in the compiler.
- CLHS 3.1–3.2 (evaluation and compilation model), 5.2 (exit extents) —
  even for an AutoLISP, the exit-extent and compile-time-effect notions
  transfer.
- A. Ertl, "The Structure and Performance of Efficient Interpreters" —
  dispatch-loop techniques (switch vs threaded), when they matter.
