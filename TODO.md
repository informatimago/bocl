# bocl — Remaining Work

Snapshot at **v1.9.0** (2026-07-19). Previous snapshot (v1.4.30,
2026-07-07) is superseded — its "Landed" sections are all still true,
plus the ~144 commits of the `vm` branch program consolidated below.

The live, *ordered* queue is
[issues/open/index.txt](issues/open/index.txt); this file cross-refs
the highest-signal items. For the full CL-symbol coverage picture see
[CL-STATUS.md](CL-STATUS.md); for the phase-by-phase plan see
[PLAN.md](PLAN.md).

## Landed since v1.4.30

Consolidated highlights — the detail is in
[CL-STATUS.md](CL-STATUS.md) § "Landed between v1.4.30 and v1.9.0"
and the git log.

### v1.5.x (minimal compilation + text fasls)

- Minimal compilation: `FUNCTION-LAMBDA-EXPRESSION`, `DISASSEMBLE`,
  `MACROLET` / `SYMBOL-MACROLET` walk, `COMPILE-FILE` `EVAL-WHEN`.
- `COMPILE-FILE` writes a real fast-load file (macroexpanded forms,
  controlled I/O syntax for safe round-tripping).

### v1.6.x – v1.7.x (plan-3 interpreter hot path)

- Phase-exclusive load profiler; hot-1..hot-6: cached lexical-variable
  slot indices (2.6× variable-heavy eval), fast-path lexical lookup,
  cons-based lexical bindings, and the rest of the hot-path program.

### v1.8.x (plan-2 byte-code VM + compiler, conformance, GC)

- **Byte-code ISA, VM dispatch core, compiler front-end** (vm-1..vm-10,
  vm-6b): the whole language byte-compiles — ~10× tight loops, ~31×
  CLOS dispatch, ~3× boot from the cached `prelude.cfasl` image.
- **cfasl compact binary fasls** (cfasl-1..5); ASDF systems compile
  to cfasls (real compile path).
- **UIOP ported** (all but async subprocess);
  `com.informatimago.common-lisp` loads through ASDF — eight CLHS
  conformance fixes fell out (DEFSETF rewrite, typed DEFSTRUCT,
  DEFINE-MODIFY-MACRO, …).
- **v1.8.5x conformance run**: REPL echoes all values,
  `DIVISION-BY-ZERO`, CLOS condition reports, 33 missing symbols
  implemented (MOP defaults wired into dispatch, `TRACE`/`UNTRACE`,
  `FILE-POSITION`, degraded pprint entry points), designator audit,
  kernel errors name their function.
- **GC correctness campaign**: unrooted KERNEL-READ form (the
  wandering REPL/LOAD corruptions, v1.8.52), live-pool re-root audit
  (v1.8.58), FreeBSD interior-pointer hardening (v1.8.60);
  generational collector integrated and reworked on `vm`.
- **CI lanes green and blocking**: Debian gcc/clang, Alpine musl,
  tcc, FreeBSD (KVM), macOS, ASan/UBSan.
- **v1.9.0 = the vm program merged to master and released.**

## Still Remaining

Work through [issues/open/index.txt](issues/open/index.txt) in order;
this is the same queue grouped by theme.

### The last quality gate

- [ ] **ansi-test campaign** —
  [ansi-test.issue](issues/open/ansi-test.issue). The harness loads
  and runs end-to-end; still needed: a submodule / `make ansi-test`
  target, a known-fail map, and a "pass count only goes up" CI gate.
  Feeds every conformance item below. **Target: SACLA and ansi-test
  100 % green.**

### Conformance (ansi-test fodder)

- [ ] [missing-functions](issues/open/missing-functions.issue) — 11
  residual symbols: 6 composite-stream accessors (blocked on real
  composite stream objects) + 5 special forms without
  `MACRO-FUNCTION` visibility (def-forms Phase-2).
- [ ] [type-specifier-conformance](issues/open/type-specifier-conformance.issue)
  — float subtypes / `BASE-STRING` / `SIMPLE-BIT-VECTOR` not in
  `TYPEP`; `CONSTANTP` misses constant variables; `(BOUNDP T)` ⇒
  `NIL` (found by the v1.9.0 CL-STATUS audit).
- [ ] [precise-condition-subclasses](issues/open/precise-condition-subclasses.issue)
  — ~134 kernel + ~90 prelude error sites to reclassify onto precise
  condition classes; incremental, driven by handlers that
  discriminate.
- [ ] [should-tests-type-errors](issues/open/should-tests-type-errors.issue)
  — ~45 SACLA `should-*` failures, almost all "wrong-type argument
  should signal TYPE-ERROR"; pairs with the previous item.

### VM / compiler

- [ ] [vm-analyzer-decline-gaps](issues/open/vm-analyzer-decline-gaps.issue)
  — 3 of asdf.lisp's 586 functions decline byte-compilation
  (&REST-as-variable analyzer bug ×2, RETURN-FROM/BLOCK visibility).
- [ ] [cfasl-6-literal-coverage](issues/open/cfasl-6-literal-coverage.issue)
  — bignum / structure / bit-vector literals in the binary fasl
  writer ([plan-1-compact-fasl](issues/open/plan-1-compact-fasl.issue)
  stays open until this lands).

### GC

- [ ] [minor-gc-doomed-under-gcminorstress](issues/open/minor-gc-doomed-under-gcminorstress.issue)
  — filed against the old new-gc branch; re-verify under the current
  collector, close or chase as a remembered-set gap.
- [ ] [generational-gc](issues/open/generational-gc.issue) — status
  reconciliation: mark what landed on `vm`, keep only the remaining
  ideas (or close in favour of deferred-cons-paged-heap).

### Epics (schedule deliberately)

- [ ] [green-threads](issues/open/green-threads.issue) — cooperative
  threads in the VM, single C thread, bordeaux-threads API; prereq
  for the Strandh-style debugger (debugger as coroutine).
- [ ] [posix-shared-heap](issues/open/posix-shared-heap.issue) —
  `BOCL-POSIX` (fork/exec/pipes/mmap/shm/sem), then shared-memory
  Lisp arenas + channels; Part 1 also unblocks uiop-tier3.
- [ ] [uiop-tier3-subprocess](issues/open/uiop-tier3-subprocess.issue)
  — async `LAUNCH-PROGRAM`, `DUMP-IMAGE` / `COMBINE-FASLS` (optional;
  UIOP tolerates NOT-IMPLEMENTED-ERROR).

### Deferred by design (revisit when their trigger appears)

- [ ] [deferred-bignum-ops](issues/open/deferred-bignum-ops.issue),
  [deferred-cons-paged-heap](issues/open/deferred-cons-paged-heap.issue),
  [deferred-sacla-logic-bugs](issues/open/deferred-sacla-logic-bugs.issue),
  [deferred-signal-hooks](issues/open/deferred-signal-hooks.issue).

### Unfiled follow-ups

- [ ] **`DEFINE-METHOD-COMBINATION`** full support — only the
  `standard` combination works; user-defined combinations are stubs.
- [ ] **Pretty-printer layout engine** — all entry points exist since
  v1.8.57 but output is linear; `PPRINT-INDENT` / `PPRINT-NEWLINE` /
  `SET-PPRINT-DISPATCH` need a real engine.
- [ ] **Remaining MOP surface** —
  `COMPUTE-APPLICABLE-METHODS-USING-CLASSES`, class-precedence-list
  finalisation hooks (most MOP defaults landed v1.8.57).
- [ ] **`UPDATE-INSTANCE-FOR-REDEFINED-CLASS` trigger** — the generic
  function exists (v1.8.57); verify the `DEFCLASS`-redefinition path
  actually calls it (ansi-test will check).
- [ ] **Phase 6 kernel test suites** — the 44 existing CUnit suites
  are functional-area/slice suites; the per-subsystem files from
  [PLAN.md](PLAN.md) Phase 6 (`test_memory`, `test_cons`,
  `test_symbol`, `test_printer`, `test_stream`, …) are still absent.
- [ ] **Phase 7 quality** — Valgrind-clean Linux job (ASan/UBSan
  already in CI); no OpenBSD lane in the matrix yet.

### Perf ledger (not ready to process; keep measuring)

- [compilation-fasl-speed](issues/open/compilation-fasl-speed.issue) —
  update the measurement table per release. Root cause of the
  remaining gap is GC superlinearity (minor GC rescans whole
  remembered container blocks); fix = card-marking remset.
- [minimal-compilation-optimization](issues/open/minimal-compilation-optimization.issue).

## Completed items (formerly on this list) — verified closures

Everything the v1.4.30 TODO listed as a live issue is now closed and
in [issues/closed/](issues/closed/): minimal-compilation (subsumed by
the VM compiler), uiop-bocl-porting, asdf-modules,
asdf-source-registry-inherit-scan-slow, slow-apropos,
ratio-floor-round-truncate, subtypep-certainty,
open-element-type-default, defstruct-conc-name-package,
package-print-object-method, clos-condition-hierarchy-parents,
slice22-cls-vs-class-type-error, structured-pathnames,
stale-test-suite-failures. From the "unfiled" list, the MOP defaults
and `UPDATE-INSTANCE-FOR-REDEFINED-CLASS` landed at v1.8.57, and the
pretty-printer entry points exist (degraded).
