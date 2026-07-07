# bocl — Remaining Work

Snapshot at **v1.4.30** (2026-07-07). Previous snapshot ("post-v0.0.56")
is superseded — the "Completed" sections there are all still true,
plus ~340 subsequent commits.

The live queue is [issues/open/](issues/open/); this file cross-refs
the highest-signal items. For the full CL-symbol coverage picture see
[CL-STATUS.md](CL-STATUS.md); for the phase-by-phase plan see
[PLAN.md](PLAN.md).

## Landed since v0.0.57

Consolidated highlights of what shipped between v0.0.57 and v1.4.30 —
individual entries live in the git log and PLAN.md.

### v0.0.57 – v0.0.60 (finishing Phase 5)

- **Logical pathnames** — full conforming implementation (Slice 33,
  v0.0.57).
- **Installable static + shared libs** + `make install PREFIX=…` +
  `make dist` (Slice 35, v0.0.60).
- Full ANSI **condition system** for user code (`DEFINE-CONDITION`
  on top of CLOS; full standard hierarchy).

### v0.1.x – v0.9.x (CLOS + reader completeness + big arrays)

- **Full CLOS**: `DEFCLASS` / `DEFGENERIC` / `DEFMETHOD` /
  `MAKE-INSTANCE` / `INITIALIZE-INSTANCE` / `SHARED-INITIALIZE` /
  `REINITIALIZE-INSTANCE` / `CHANGE-CLASS` /
  `UPDATE-INSTANCE-FOR-DIFFERENT-CLASS`, `:before` / `:after` /
  `:around` method combination (standard only).
- **True per-package obarrays** (present + external symbol tables) —
  Slice 46 v0.0.71. The v0.0.16 known limitation is gone.
- Fill-pointer arrays / adjustable / displaced (Slices 94–104).
- Multi-dim + bit arrays (Slices 100–102).
- Reader: `READTABLE-CASE`, `|…|` case-preserving symbol syntax,
  `#C` complex, `#\Newline` and named chars, `#n=` / `#n#` circular
  structures, `#.` read-time eval, `#nA` multi-dim array literal,
  `SET-MACRO-CHARACTER`, `SET-DISPATCH-MACRO-CHARACTER`, custom
  readtables.
- Restarts: `RESTART-BIND`, `COMPUTE-RESTARTS`, `HANDLER-CASE
  :NO-ERROR`, `FIND-RESTART` `:test-function`.
- **Bignum engine** ([bootstrap/bignum.lisp](bootstrap/bignum.lisp))
  + kernel type — Slices 72–75, 122.
- `*QUERY-IO*` / `DEFINE-METHOD-COMBINATION` (stub) / `FIND-METHOD`,
  `(SETF foo)` functions, `*LOAD-TRUENAME*`, `DEFGENERIC (:method
  …)`, O(1) bucketed hash tables.

### v1.0.x

- Release milestone.
- **Generational nursery GC** with write barrier + proactive default
  (Slices 168–174).

### v1.1.x – v1.2.x (SACLA must-* suite green)

- Iterated fixes across `must-character`, `must-string`, `must-do`,
  `must-eval`, `must-hash-table`, `must-data-and-control`,
  `must-package`, reader completeness, `SETF` eval-order,
  `MACRO-FUNCTION` env, condition/restart ordering.
- Slice 113 (v1.2.40): `must-package` conformance 31 → 0.
- Slice 114: finished the minimal debugger with function-call
  backtrace frames.
- **v1.3.0 = must-\* GREEN**.

### v1.3.x (ASDF + modules)

- `make install PREFIX=/opt/local` FHS layout (Slices 114–116).
- `--on-error` split: REPL defaults to `debug`, batch to `exit`
  (Slice 118).
- Package `PRINT-OBJECT` (Slice 119).
- `SORT` segfault fix: iterative `%merge` (Slice 120).
- Control-stack overflow → catchable `STORAGE-CONDITION`
  (Slice 121).
- **Module system**: `BOCL:*MODULE-LIST*` + manifest-driven `REQUIRE`
  + `modules/` build dir (Slices 122–124).
- **ASDF as git submodule** at [modules/asdf-src/](modules/asdf-src)
  (Slice 124).
- **UIOP Tier-1 portability** hooks (Slice 125).
- `:allocation :class` (v1.3.17), computed `:initform` per-instance
  (v1.3.16), CLOS-aware `SUBTYPEP` / `TYPEP` (v1.3.15), structured
  pathnames + `DEFGENERIC` / `DEFMETHOD (setf name)` + `(setf
  DOCUMENTATION)` (v1.3.14).
- **v1.4.0 = ASDF 3.3.7 loads and runs**.

### v1.4.x (sockets + ASDF using + printer specials + CI fixes)

- Clang / gcc warning cleanup (v1.4.1–3).
- Callback stream + DRIBBLE (v1.4.4–6).
- `FORMAT ~D` mincol/padchar (v1.4.8), `--on-error` ordered (v1.4.9).
- `PARSE-NAMESTRING`, `TYPEP` compound types with `:*` bounds /
  `FUNCTION` / `BOOLEAN` / `KEYWORD` / `ATOM` / `BIT`.
- `REINITIALIZE-INSTANCE` + `SHARED-INITIALIZE` fix.
- `LOOP ALWAYS` / `NEVER` / `THEREIS` timing fix.
- `MAKE-PATHNAME :DIRECTORY NIL` supplied-p vs defaults.
- `WITH-COMPILATION-UNIT` (CLHS 3.2.2.3).
- CLOS multi-value preservation through generic-function dispatch.
- **BSD sockets kernel** + `BOCL-SOCKET` package (v1.4.20–21).
- Install ships [bootstrap/socket.lisp](bootstrap/socket.lisp)
  (v1.4.23).
- UIOP patch idempotent (v1.4.22).
- macOS thalassa CI runner.
- `(asdf:load-system)` runs a real system (v1.4.19).
- `*PRINT-CIRCLE*` SIGSEGV fix (v1.4.24), printer specials bound in
  kernel (v1.4.26), test-suite triage (v1.4.25–29), CI fails on test
  failure (v1.4.30).

## Still Remaining

The queue is now split across live issues plus a few unfiled items.

### Blocker for the last quality gate

- [ ] **ansi-test integration** — see [PLAN.md §3](PLAN.md) +
  [ansi-test.issue](issues/open/ansi-test.issue). No submodule, no
  `tests/ansi-test-harness/`, no `make ansi-test`. Should come with a
  known-fail map and a "pass count only goes up" CI gate.

### Live issues (see [issues/open/](issues/open/) for full text)

- [ ] [missing-functions](issues/open/missing-functions.issue) —
  systematic audit of the ~120 remaining CL symbols
  (CL-STATUS.md coverage table is the entry point).
- [ ] [precise-condition-subclasses](issues/open/precise-condition-subclasses.issue)
  — kernel signals `SIMPLE-ERROR` where a specific ANSI subclass is
  required.
- [ ] [clos-condition-hierarchy-parents](issues/open/clos-condition-hierarchy-parents.issue)
  — style-warning parent chain breaks `(TYPEP c 'WARNING)`.
- [ ] [ratio-floor-round-truncate](issues/open/ratio-floor-round-truncate.issue)
  — 1-arg `FLOOR` / `ROUND` / `CEILING` / `TRUNCATE` reject a
  `RATIO`.
- [ ] [subtypep-certainty](issues/open/subtypep-certainty.issue) —
  returns `(NIL NIL)` for disjoint known types.
- [ ] [open-element-type-default](issues/open/open-element-type-default.issue)
  — `OPEN` rejects `:ELEMENT-TYPE :DEFAULT`.
- [ ] [minimal-compilation](issues/open/minimal-compilation.issue) —
  real `COMPILE` + eager `MACROEXPAND-ALL` at `LOAD`.
- [ ] [deferred-bignum-ops](issues/open/deferred-bignum-ops.issue) —
  remaining bitwise + `ISQRT` paths that aren't bignum-aware.
- [ ] [defstruct-conc-name-package](issues/open/defstruct-conc-name-package.issue)
  — accessors interned in `*PACKAGE*` instead of the struct's home
  package.
- [ ] [package-print-object-method](issues/open/package-print-object-method.issue)
  — packages print as `#<PACKAGE 0x…>`.
- [ ] [uiop-bocl-porting](issues/open/uiop-bocl-porting.issue) — Tier
  1 done; further UIOP hooks remain.
- [ ] [asdf-modules](issues/open/asdf-modules.issue) — `REQUIRE`
  couldn't find asdf module at v1.4.19 (worked around; needs
  cleanup).
- [ ] [asdf-source-registry-inherit-scan-slow](issues/open/asdf-source-registry-inherit-scan-slow.issue)
  — ASDF `DIRECTORY` tree-walk hangs.
- [ ] [slow-apropos](issues/open/slow-apropos.issue) — 17 s for
  `(apropos "MODULE")`.
- [ ] [deferred-signal-hooks](issues/open/deferred-signal-hooks.issue)
  — extend `*ABORT-HOOK*` to `SIGTERM` / Core.
- [ ] [deferred-sacla-logic-bugs](issues/open/deferred-sacla-logic-bugs.issue)
  — residual GC + logic bugs in the SACLA harness.
- [ ] [deferred-cons-paged-heap](issues/open/deferred-cons-paged-heap.issue)
  — 16-byte cons arena (perf).
- [ ] [generational-gc](issues/open/generational-gc.issue) — further
  nursery work.
- [ ] [slice22-cls-vs-class-type-error](issues/open/slice22-cls-vs-class-type-error.issue)
  — state-dependent `%CLS` / `CLASS` abort.
- [ ] [structured-pathnames](issues/open/structured-pathnames.issue)
  — core done v1.3.14; refinements remain.
- [ ] [stale-test-suite-failures](issues/open/stale-test-suite-failures.issue)
  — kernel-only suites lack prelude load.
- [ ] [should-tests-type-errors](issues/open/should-tests-type-errors.issue)
  — ~45 SACLA `should-*` TYPE-ERROR gaps.

### Unfiled follow-ups (from the previous TODO.md "Low Priority")

- [ ] **`UPDATE-INSTANCE-FOR-REDEFINED-CLASS`** — wire into the
  class-redefinition path when `DEFCLASS` is re-evaluated on an
  existing class.
- [ ] **`DEFINE-METHOD-COMBINATION`** full support — stubs exist;
  implement custom combination types (currently only `standard`
  works).
- [ ] **MOP surface**: `FIND-METHOD` variants, `METHOD-QUALIFIERS`,
  `COMPUTE-APPLICABLE-METHODS-USING-CLASSES`, class-precedence-list
  finalisation hooks.
- [ ] **Pretty printer**: `PPRINT-LOGICAL-BLOCK`, `PPRINT-POP`,
  `PPRINT-EXIT-IF-LIST-EXHAUSTED`, `PPRINT-INDENT`, `PPRINT-NEWLINE`,
  `PPRINT-TAB`, `SET-PPRINT-DISPATCH`.
- [ ] **Phase 6 kernel test suite** — see [PLAN.md](PLAN.md); ~15
  `test_*.c` files still missing per subsystem (`test_memory`,
  `test_kernel_objects`, `test_cons`, `test_symbol`, `test_integer`,
  `test_float`, `test_character`, `test_string`, `test_vector`,
  `test_bit_vector`, `test_structure`, `test_printer`, `test_stream`,
  `test_filename`, `test_environment`, `test_built_in`).
- [ ] **Phase 7 quality**: Valgrind-clean Linux job (ASan / UBSan
  already in CI); build matrix currently gcc / clang / tcc on Debian
  + Alpine musl + FreeBSD + macOS — no OpenBSD job.

## Completed items (formerly on this list) — cross-check for closure

Items from the previous TODO.md "Still Remaining" section, verified
against master:

- [x] **Full PRINT-OBJECT auto-dispatch** — landed; prelude method
  dispatch is present.
- [x] **Ratio / complex numbers** — landed as first-class kernel
  types (`Ratio`, `Complex` in
  [sources/kernel_types.h](sources/kernel_types.h)); arithmetic
  auto-promotes. One edge case remains ([ratio-floor-round-truncate](issues/open/ratio-floor-round-truncate.issue)).
- [x] **Logical pathname stubs → real** — Slice 33 shipped conforming
  translations, `PATHNAME-MATCH-P`, `WILD-PATHNAME-P`.
