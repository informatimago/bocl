# bocl Common Lisp symbol-status inventory

Snapshot at **v1.9.0** (2026-07-19). Previous snapshot was v1.4.30
(2026-07-07) — since then ~144 commits landed (v1.5.x through v1.9.0),
dominated by the `vm` branch program: the byte-code VM + compiler, the
UIOP/ASDF real compile path, the v1.8.5x conformance run, and the GC
correctness campaign. This inventory was produced by *running* the
kind-aware audit script ([issues/open/audit-cl-symbols.lisp](issues/open/audit-cl-symbols.lisp))
against the v1.9.0 binary, plus targeted TYPEP/CONSTANTP probes — not
by estimating.

## Overall coverage

All **978** `COMMON-LISP` external symbols are accounted for except
the **11 residuals** listed below: **967 / 978 ≈ 99 %**. The
evidence-based breakdown (a symbol can appear in more than one row —
e.g. `*` is both a function and a REPL variable):

| Evidence                                  | Count |
|-------------------------------------------|-------|
| `SPECIAL-OPERATOR-P`                       | 25 / 25 — **100 %** |
| `MACRO-FUNCTION`                           | 86 (of the 91 CLHS macros; the other 5 are functional evaluator special forms, see residuals) |
| `FBOUNDP` (excl. macros/special operators) | 632 (of 638 fbound-expected; the 6 missing are the composite-stream accessors) |
| `BOUNDP` (variables + constants)           | 104, + the 6 REPL history variables `**` `***` `++` `+++` `//` `///` which are bound at REPL entry (verified) |
| `FIND-CLASS` only (no other role)          | 65 classes |
| Marker symbols — nothing to implement      | ~31: the 8 lambda-list keywords, 15 declaration identifiers (`SPEED`, `SAFETY`, `IGNORE`, …), `DECLARE`, `OTHERWISE`, the 3 `DOCUMENTATION` doc-types (`VARIABLE`, `STRUCTURE`, `COMPILER-MACRO`), `CALL-METHOD` / `MAKE-METHOD` (local macros of method combination), `STANDARD` |
| Type-specifier-only names                  | 17 (`FIXNUM`, `BOOLEAN`, `KEYWORD`, `SATISFIES`, `SIMPLE-ARRAY`, …) — most verified working in `TYPEP`; the broken ones are in [type-specifier-conformance](issues/open/type-specifier-conformance.issue) |

## The 11 residual gaps (unchanged since v1.8.57, re-verified at v1.9.0)

Tracked in [missing-functions.issue](issues/open/missing-functions.issue):

1. **Composite-stream accessors (6)** — `ECHO-STREAM-INPUT-STREAM`,
   `ECHO-STREAM-OUTPUT-STREAM`, `TWO-WAY-STREAM-INPUT-STREAM`,
   `TWO-WAY-STREAM-OUTPUT-STREAM`, `CONCATENATED-STREAM-STREAMS`,
   `SYNONYM-STREAM-SYMBOL`. Blocked: the `MAKE-*-STREAM` constructors
   are stubs returning an underlying stream, so there is no composite
   object to access. Fix = real composite stream objects on the
   callback-stream machinery.

2. **Macro visibility (5)** — `DEFMACRO`, `HANDLER-CASE`,
   `HANDLER-BIND`, `RESTART-BIND`, `RESTART-CASE` are fully functional
   evaluator special forms, but `MACRO-FUNCTION` returns `NIL` for
   them and they are (correctly) not in `SPECIAL-OPERATOR-P`'s ANSI
   list. Conformance-visibility only; becomes real macro definitions
   in the def-forms Phase-2 plan.

## Depth caveats — present is not the same as fully conforming

- **Pretty printer**: all entry points exist
  (`PPRINT-LOGICAL-BLOCK`, `PPRINT-POP`, `PPRINT-FILL/-LINEAR/-TABULAR`,
  `PPRINT-DISPATCH`, …) but there is no layout engine — output is
  linear.
- **`DEFINE-METHOD-COMBINATION`**: only the standard combination;
  user-defined combinations are stubs.
- **Precise condition subclasses**: many kernel `ERROR()` and prelude
  `(error "…")` sites still signal `SIMPLE-ERROR` where ANSI requires
  a specific subclass — the incremental campaign is
  [precise-condition-subclasses](issues/open/precise-condition-subclasses.issue),
  fed by the ~45 SACLA `should-*` failures
  ([should-tests-type-errors](issues/open/should-tests-type-errors.issue)).
- **Type-specifier nits** (new, found by this audit, filed as
  [type-specifier-conformance](issues/open/type-specifier-conformance.issue)):
  `SINGLE-FLOAT`/`DOUBLE-FLOAT`/`SHORT-FLOAT`/`LONG-FLOAT`,
  `BASE-STRING`/`SIMPLE-BASE-STRING`, and `SIMPLE-BIT-VECTOR` are not
  wired into `TYPEP`; `CONSTANTP` misses constant variables
  (`(constantp 'pi)` ⇒ `NIL`); `(boundp t)` / `(boundp nil)` ⇒ `NIL`;
  `TYPEP` returns `NIL` silently on unknown type specifiers.
- **`*BREAK-ON-SIGNALS*`**: defined; kernel signalling does not
  consult it yet.
- **VM decline gaps**: 3 of asdf.lisp's 586 functions decline
  byte-compilation and fall back to the interpreter
  ([vm-analyzer-decline-gaps](issues/open/vm-analyzer-decline-gaps.issue)).

## Architecture — kernel vs Lisp side (still holds, plus the VM)

The two-layer principle is unchanged: everything expressible in CL
lives in [bootstrap/prelude.lisp](bootstrap/prelude.lisp) (~12 300
lines); the C kernel keeps special operators, the GC, POSIX
primitives, the numeric layer, sockets, and the Lisp-visible
built-ins. New since v1.4.30: the **byte-code compiler + VM**
([sources/bytecode.c](sources/bytecode.c)) — the whole language
byte-compiles (~10× tight loops, ~31× CLOS dispatch) — and the
**binary fasl format** ([sources/cfasl.c](sources/cfasl.c)): the
prelude boots from a cached `prelude.cfasl` image (~3× faster boot),
and ASDF systems compile to cfasls.

## Landed between v1.4.30 and v1.9.0

Consolidated; the phase-by-phase account is in the git log and
[issues/closed/](issues/closed/).

- **plan-2 byte-code VM + compiler** (vm-1..vm-10, vm-6b) and
  **plan-3 interpreter hot path** (hot-1..hot-6). Both performance
  plans closed 2026-07-18.
- **cfasl compact binary fasls** (cfasl-1..5; cfasl-6 literal
  coverage still open) + prelude boot image.
- **UIOP ported** (all but the async-subprocess tier 3) and the
  **real ASDF compile path**; `com.informatimago.common-lisp`
  (cesarum) loads through ASDF — eight CLHS conformance fixes fell
  out of it (DEFSETF rewrite, typed DEFSTRUCT, DEFINE-MODIFY-MACRO,
  reader dotted conditionals, method-combination fixes, DEFPACKAGE
  clause order, …).
- **33 missing CL symbols implemented** at v1.8.57: the MOP defaults
  wired into dispatch (`COMPUTE-APPLICABLE-METHODS`, `ADD-METHOD` /
  `REMOVE-METHOD`, `NO-APPLICABLE-METHOD` / `NO-NEXT-METHOD`,
  `SLOT-MISSING` / `SLOT-UNBOUND`,
  `UPDATE-INSTANCE-FOR-REDEFINED-CLASS`, `MAKE-INSTANCES-OBSOLETE`, …),
  real `TRACE` / `UNTRACE`, `FILE-POSITION` (new C builtin),
  `FORMATTER`, `STEP`, `INSPECT`, `COPY-STRUCTURE`, and the degraded
  pretty-printer entry points.
- **Conformance quick wins** (v1.8.54–56): the REPL echoes every
  returned value; `(/ x 0)` signals `DIVISION-BY-ZERO`; CLOS
  conditions print their `:REPORT` under `PRINC`.
- **Designator audit** (v1.8.49) and kernel error messages that name
  their function (v1.8.53).
- **GC correctness campaign**: KERNEL-READ returned an unrooted form
  — the wandering REPL/LOAD heap corruptions (v1.8.52); live-pool
  re-root audit, 72 sites classified (v1.8.58); FreeBSD
  interior-pointer hardening in `valid_block_start_p` (v1.8.60). The
  generational collector (nursery, remembered set, rebuild passes)
  was integrated and reworked on the `vm` branch.
- **CI portability lanes green and blocking**: Debian gcc/clang,
  Alpine musl, tcc, FreeBSD (KVM), macOS, ASan/UBSan sanitiser lane.
- **The entire "still open" list of the v1.4.30 snapshot is closed**
  (verified in [issues/closed/](issues/closed/)):
  ratio-floor-round-truncate, subtypep-certainty,
  open-element-type-default, defstruct-conc-name-package,
  package-print-object-method, clos-condition-hierarchy-parents,
  slow-apropos, slice22-cls-vs-class-type-error,
  structured-pathnames, stale-test-suite-failures,
  asdf-source-registry-inherit-scan-slow, minimal-compilation
  (subsumed by the VM compiler), uiop-bocl-porting, asdf-modules.

## Tests

- 44 CUnit kernel suites under [tests/](tests/) — green (v1.9.0
  release run).
- SACLA `must-*` suites — **green**; `should-*` suites — ~45
  failures, almost all "wrong-type argument should signal
  TYPE-ERROR".
- ansi-test: the harness loads and runs end-to-end; a systematic run
  is the next campaign ([ansi-test.issue](issues/open/ansi-test.issue)).

## What's still open

The live, ordered queue is
[issues/open/index.txt](issues/open/index.txt) (re-triaged 2026-07-18
at v1.8.53, updated since). Highest-signal items:

- [type-specifier-conformance](issues/open/type-specifier-conformance.issue) — this audit's findings (S–M).
- [vm-analyzer-decline-gaps](issues/open/vm-analyzer-decline-gaps.issue) — 3 functions decline byte-compilation (M).
- [precise-condition-subclasses](issues/open/precise-condition-subclasses.issue) + [should-tests-type-errors](issues/open/should-tests-type-errors.issue) — the TYPE-ERROR campaign (M–L).
- [cfasl-6-literal-coverage](issues/open/cfasl-6-literal-coverage.issue) — bignum/structure/bit-vector fasl literals (M).
- [ansi-test](issues/open/ansi-test.issue) — the conformance campaign; feeds all of the above (L).
- [green-threads](issues/open/green-threads.issue) → Strandh-style debugger; then [posix-shared-heap](issues/open/posix-shared-heap.issue) (epics).
- [uiop-tier3-subprocess](issues/open/uiop-tier3-subprocess.issue) — async `LAUNCH-PROGRAM` (optional).

## Bottom line

The v1.4.30 snapshot measured ~88 % coverage with the gaps spread over
~120 symbols; at v1.9.0 the audit accounts for **967 / 978 ≈ 99 %**,
with the 11 residuals precisely characterised and blocked on two
well-understood pieces of work (composite stream objects; def-forms
Phase 2). Symbol *presence* is no longer the frontier — conformance
*depth* is: precise condition classes, type-specifier wiring, the
pretty-printer layout engine, and user-defined method combinations.
The gate for all of it is the
[ansi-test campaign](issues/open/ansi-test.issue), toward the stated
target of SACLA **and** ansi-test 100 % green.
