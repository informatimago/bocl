# bocl Common Lisp symbol-status inventory

Snapshot at **v1.4.30** (2026-07-07). Previous snapshot in this file was
v0.0.20 — since then, roughly 340 commits landed across slices 14–174
and versions v0.1.x through v1.4.x. This rewrite reflects the current
state of the `COMMON-LISP` external symbols against what bocl provides
in the C kernel + [bootstrap/prelude.lisp](bootstrap/prelude.lisp) (~10 700
lines) + [bootstrap/socket.lisp](bootstrap/socket.lisp) + the ASDF module
under [modules/asdf-src](modules/asdf-src).

## Overall coverage

Rough estimate — the "top-50 missing" list from the previous snapshot
is essentially cleared. The remaining gaps are concentrated in a few
narrow areas (compiler surface, some MOP hooks, a handful of edge-case
type errors), not in whole chapters.

| Kind             | Implemented    | Total | %       |
|------------------|----------------|-------|---------|
| Special operator | 25 / 25        | 25    | **100 %** |
| Function         | ~570 / 636     | 636   | ~90 %   |
| Macro            | ~85 / 91       | 91    | ~93 %   |
| Variable         | ~40 / 50       | 50    | ~80 %   |
| Constant         | ~50 / 62       | 62    | ~80 %   |
| Type / decl      | ~90 / 114      | 114   | ~80 %   |
| **Total**        | **~860 / 978** | **978** | **~88 %** |

The remaining ~120 symbols are almost entirely in three buckets:
CLOS/MOP hooks that bocl deliberately stubs (`FIND-METHOD` variants,
`METHOD-QUALIFIERS`, `COMPUTE-APPLICABLE-METHODS-USING-CLASSES`, the
class-precedence-list finalisation surface); the compiler surface
(`COMPILE-FILE-PATHNAME`, `WITH-COMPILATION-UNIT` shipped but the
underlying COMPILE is minimal); and a scattering of edge-case type
predicates and printer control (`PPRINT-LOGICAL-BLOCK` and most of the
pretty printer).

## The four "still-missing" special operators — all landed

Previous snapshot listed `LET*`, `MACROLET`, `SYMBOL-MACROLET`, and
`LOAD-TIME-VALUE` as pending. All four shipped in Slice 1 (v0.0.21)
and live in [sources/eval.c](sources/eval.c). Every one of the 25 CL
special operators is now implemented.

## Architecture — kernel vs Lisp side (still holds)

The guiding principle from the v0.0.20 snapshot is unchanged and has
proven correct: everything that can be expressed in CL lives in
[bootstrap/prelude.lisp](bootstrap/prelude.lisp). The C kernel keeps
only special operators, POSIX/GC primitives, the numeric layer,
sockets, and a small set of Lisp-visible built-ins (see
[sources/built_in_functions.c](sources/built_in_functions.c) +
[sources/arithmetic.c](sources/arithmetic.c) +
[sources/socket.c](sources/socket.c) +
[sources/packages.c](sources/packages.c) +
[sources/hash_table.c](sources/hash_table.c) +
[sources/conditions.c](sources/conditions.c) +
[sources/reader.c](sources/reader.c) +
[sources/printer.c](sources/printer.c)).

## Landed since v0.0.20

Consolidated view of the big-ticket items now shipping. See
[PLAN.md](PLAN.md) for the phase-by-phase account.

### Macros / control (all shipped)

- `SETF` + `DEFINE-SETF-EXPANDER` with a registry of place expanders
  (`CAR`, `CDR`, `NTH`, `AREF`, `GETHASH`, `SYMBOL-VALUE`,
  `SYMBOL-FUNCTION`, `GET`, `SUBSEQ`, the whole CxxxR family, `SECOND`
  through `FOURTH`, `CHAR`, `SCHAR`, `LDB`, `DOCUMENTATION`, and
  user-defined via `DEFINE-SETF-EXPANDER`) — Slice 2 v0.0.22.
- `CASE` / `ECASE` / `CCASE` / `TYPECASE` / `ETYPECASE` / `CTYPECASE`
  — Slice 3 v0.0.23.
- `WHEN` / `UNLESS` / `COND` / `AND` / `OR` / `NOT` / `PROG` / `PROG1`
  / `PROG2` / `PROGN` — Slice 3.
- `DO` / `DO*` / `DOTIMES` / `DOLIST` / `DO-SYMBOLS` /
  `DO-EXTERNAL-SYMBOLS` / `DO-ALL-SYMBOLS` — Slices 3, 18, 28.
- `LOOP` — Slice 11 (v0.0.33) + LOOP conditionals `WHEN` / `UNLESS`
  / `IF` for `COLLECT` / `DO` / `RETURN` / `SUM` in Slice 18 (v0.0.41)
  + hash-key iteration + `ALWAYS` / `NEVER` / `THEREIS` timing fix
  (v1.4.x).
- `MULTIPLE-VALUE-BIND` / `-LIST` / `-SETQ` / `NTH-VALUE` — Slice 15.
- `DESTRUCTURING-BIND` — Slice 7 v0.0.28.
- `DEFSTRUCT` (`:conc-name`, `:constructor`, `:predicate`, `:include`,
  `:type`, `:print-object`) — Slice 8 v0.0.29.
- `IGNORE-ERRORS` / `ASSERT` / `CHECK-TYPE` — Slice 9.
- `WITH-OPEN-FILE` / `WITH-OPEN-STREAM` /
  `WITH-INPUT-FROM-STRING` / `WITH-OUTPUT-TO-STRING` /
  `WITH-STANDARD-IO-SYNTAX` — Slices 10, 15, 17.
- `DECLAIM` / `DECLARE` / `PROCLAIM` — no-op declarations, stored
  where relevant.
- `WITH-COMPILATION-UNIT` — v1.4 (per CLHS 3.2.2.3).
- `WITH-HASH-TABLE-ITERATOR` — Slice 22.

### Sequences (all shipped)

`REDUCE` (generic across lists/vectors/strings, `:from-end`,
`:initial-value`, `:key` — Slice 18); `FIND` / `FIND-IF` /
`FIND-IF-NOT`; `POSITION` / `POSITION-IF` / `POSITION-IF-NOT`;
`COUNT` / `COUNT-IF` / `COUNT-IF-NOT`; `REMOVE` / `REMOVE-IF` /
`REMOVE-IF-NOT` / `DELETE` / `DELETE-IF` / `DELETE-IF-NOT`;
`SUBSTITUTE` / `NSUBSTITUTE` / `-IF` / `-IF-NOT`; `SUBSEQ` (generic);
`BUTLAST` / `NBUTLAST` / `LDIFF`; `MAPLIST` / `MAPCON` / `MAPCAN`
/ `MAPL` / `MAPC` / `MAPCAR`; `CONCATENATE` (all result types);
`MERGE`; `SORT` / `STABLE-SORT` (iterative `%merge`, no more stack
overflow — Slice 120); `UNION` / `INTERSECTION` / `SET-DIFFERENCE`
/ `SET-EXCLUSIVE-OR` / `ADJOIN` / `PAIRLIS`; `FILL` / `REPLACE` /
`MISMATCH` / `SEARCH` / `EVERY` / `SOME` / `NOTANY` / `NOTEVERY`;
`MAP` / `MAP-INTO`.

### Strings (all shipped)

`STRING=` / `STRING<` / `STRING>` / `STRING<=` / `STRING>=` /
`STRING/=` and their `-EQUAL` case-insensitive variants;
`STRING-UPCASE` / `-DOWNCASE` / `-CAPITALIZE`; `STRING-TRIM` /
`STRING-LEFT-TRIM` / `STRING-RIGHT-TRIM`.

### Characters (all shipped)

`CHAR=` / `CHAR<` / `CHAR>` / `CHAR<=` / `CHAR>=` / `CHAR/=` and
`-LESSP` / `-GREATERP` / `-NOT-EQUAL` case-insensitive variants;
`CHAR-UPCASE` / `-DOWNCASE`; `ALPHA-CHAR-P` / `ALPHANUMERICP`;
`DIGIT-CHAR` / `DIGIT-CHAR-P`; `CHAR-CODE` / `CODE-CHAR`;
`CHAR-INT` / `CHAR-NAME` / `NAME-CHAR`; `#\Newline` and named
character reader syntax (Slice 76).

### Numeric

- **Full numeric tower**: `Bignum`, `Ratio`, `Complex` are all kernel
  types in [sources/kernel_types.h](sources/kernel_types.h);
  arithmetic auto-promotes across the tower. Bignum engine
  in [bootstrap/bignum.lisp](bootstrap/bignum.lisp) + kernel type
  (Slices 72–75, 122).
- **Transcendentals in C**: `SIN` / `COS` / `TAN` / `ASIN` / `ACOS`
  / `ATAN` / `SINH` / `COSH` / `TANH` / `EXP` / `LOG` / `SQRT` /
  `EXPT` — Slice 6.
- **Integer helpers**: `GCD` / `LCM` / `ISQRT` / `SIGNUM` — Slice 6.
- **Complex helpers**: `CONJUGATE` / `PHASE` / `REALPART` /
  `IMAGPART` / `CIS` — Slice 6.
- **Float introspection**: `NUMERATOR` / `DENOMINATOR` /
  `FLOAT-DIGITS` / `FLOAT-PRECISION` / `SCALE-FLOAT` /
  `DECODE-FLOAT` / `INTEGER-DECODE-FLOAT` / `FLOAT-RADIX` — Slice 6.
- **Coercion**: `COERCE` handles numeric + sequence conversions —
  Slice 16.
- **Rationalize / rational**: shipped, but note the open
  [ratio-floor-round-truncate](issues/open/ratio-floor-round-truncate.issue)
  issue — 1-arg `FLOOR` / `ROUND` / `CEILING` / `TRUNCATE` of a
  `RATIO` still errors.

### Hash tables, conditions, restarts

- All four test functions (`EQ` / `EQL` / `EQUAL` / `EQUALP`),
  `MAKE-HASH-TABLE` full keyword surface, `WITH-HASH-TABLE-ITERATOR`,
  `HASH-TABLE-REHASH-SIZE`, etc. — Slices 4.1, 22, 166 (O(1) buckets).
- `HANDLER-CASE` (Slice 4.2), `HANDLER-BIND` + restart machinery
  (Slice 9a), `DEFINE-CONDITION` (Slice 21, on top of CLOS), full
  ANSI condition hierarchy shipped, `RESTART-BIND` /
  `COMPUTE-RESTARTS` / `FIND-RESTART` (`:test-function`) /
  `INVOKE-RESTART` / `HANDLER-CASE :NO-ERROR` / `CERROR` /
  `WITH-SIMPLE-RESTART` / `MUFFLE-WARNING`.

### Reader / printer

- Full reader macros: `#'` / `#|…|#` / `#+` / `#-` / `#\` (all
  named chars) / `#(` / `#B` / `#O` / `#X` / `#R` / `#C` /
  `#n=` / `#n#` (circular structures — Slices 62–114) / `#S` /
  `#nA` (multi-dim array literal) / `pkg:sym` / `pkg::sym` /
  `#.` (read-time eval) / `SET-MACRO-CHARACTER` /
  `SET-DISPATCH-MACRO-CHARACTER` / custom readtables /
  `READTABLE-CASE` / `|…|` case-preserving symbol syntax.
- All `*PRINT-*` control vars bound: `*PRINT-READABLY*` /
  `*PRINT-ESCAPE*` / `*PRINT-PRETTY*` / `*PRINT-CIRCLE*` (with
  `#n=` / `#n#` — Slice 13 + v1.4.24 SIGSEGV fix) / `*PRINT-BASE*` /
  `*PRINT-LEVEL*` / `*PRINT-LENGTH*` / `*PRINT-GENSYM*` /
  `*PRINT-RADIX*` / `*PRINT-CASE*` / `*PRINT-ARRAY*` /
  `*PRINT-LINES*` / `*PRINT-MISER-WIDTH*` /
  `*PRINT-RIGHT-MARGIN*`. Bound in the kernel now (v1.4.26).
- All `*READ-*`: `*READ-BASE*` / `*READ-EVAL*` /
  `*READ-DEFAULT-FLOAT-FORMAT*` / `*READ-SUPPRESS*` / `*READTABLE*`.
- `FORMAT` — full directive engine (`~A ~S ~% ~& ~~ ~D ~B ~O ~X ~R
  ~F ~E ~G ~I ~N ~<…~> ~(…~) ~[…~] ~* ~? ~^ ~@D`); mincol/padchar/
  comma params on `~D ~B ~O ~X ~R` (v1.4.8).

### CLOS

- `DEFCLASS` / `DEFGENERIC` / `DEFMETHOD` / `MAKE-INSTANCE` /
  `INITIALIZE-INSTANCE` / `SHARED-INITIALIZE` (initarg/slot-names
  rule fixed v1.4) / `REINITIALIZE-INSTANCE` / `CHANGE-CLASS` /
  `UPDATE-INSTANCE-FOR-DIFFERENT-CLASS`.
- Method combinations: `:before` / `:after` / `:around` and standard
  combination.
- `:allocation :class` (v1.3.17).
- Computed `:initform` evaluated per-instance (v1.3.16).
- CLOS-aware `SUBTYPEP` / `TYPEP` (v1.3.15).
- Multiple values preserved through generic-function dispatch (v1.4).
- (SETF `DEFGENERIC` / `DEFMETHOD` / `DOCUMENTATION`) — v1.3.14.

### Packages

- **True per-package obarrays** since Slice 46 v0.0.71 (present +
  external symbol tables); the "known limitation" from Phase 3 is
  gone. Slice 113 (v1.2.40) took `must-package` conformance to 31 →
  0.
- Full: `IN-PACKAGE`, `MAKE-PACKAGE`, `USE-PACKAGE`, `EXPORT`,
  `IMPORT`, `SHADOW`, `SHADOWING-IMPORT`, `UNEXPORT`, `UNUSE-PACKAGE`,
  `DELETE-PACKAGE`, `RENAME-PACKAGE`, `DEFPACKAGE`, `FIND-PACKAGE`,
  `PACKAGE-NAME`, `PACKAGE-NICKNAMES`, `PACKAGE-USE-LIST`,
  `PACKAGE-USED-BY-LIST`, `PACKAGE-SHADOWING-SYMBOLS`, `INTERN`,
  `FIND-SYMBOL`, `UNINTERN`, `PACKAGEP`, `LIST-ALL-PACKAGES`.

### Streams, pathnames, filesystem

- Streams: `OPEN` / `CLOSE` (idempotent) / `READ-CHAR` / `PEEK-CHAR` /
  `UNREAD-CHAR` / `READ-LINE` / `READ-BYTE` / `WRITE-BYTE` /
  `WRITE-CHAR` / `WRITE-STRING` / `WRITE-LINE` / `LISTEN` /
  `CLEAR-INPUT` / `READ-CHAR-NO-HANG` / `FRESH-LINE`;
  broadcast/concatenated/synonym/two-way stream primitives;
  string streams / file streams / callback streams (v1.4.4).
- Pathnames: `PATHNAME`, `MAKE-PATHNAME`, `MERGE-PATHNAMES`,
  `TRUENAME`, `NAMESTRING`,
  `PATHNAME-{NAME,TYPE,DIRECTORY,HOST,DEVICE,VERSION}`,
  `FILE-NAMESTRING`, `DIRECTORY-NAMESTRING`, `HOST-NAMESTRING`,
  `ENOUGH-NAMESTRING`, `PARSE-NAMESTRING` (v1.4.x), `PATHNAMEP`,
  `PATHNAME-MATCH-P`, `WILD-PATHNAME-P`, `PROBE-FILE`, `DELETE-FILE`,
  `RENAME-FILE`, `DIRECTORY`.
- **Logical pathnames**: `LOGICAL-PATHNAME`, `-P`,
  `LOGICAL-PATHNAME-TRANSLATIONS`,
  `SET-LOGICAL-PATHNAME-TRANSLATIONS`, `TRANSLATE-LOGICAL-PATHNAME`,
  `LOAD-LOGICAL-PATHNAME-TRANSLATIONS` — Slice 33 v0.0.57.

### Sockets

`bootstrap/socket.lisp` (~720 lines) on top of `sources/socket.c`
(~790 lines) — package `BOCL-SOCKET` (nickname `USOCKET`), a
usocket-compatible surface: `SOCKET-CONNECT`, `SOCKET-LISTEN`,
`SOCKET-ACCEPT`, `SOCKET-SEND`, `SOCKET-RECEIVE`, `SOCKET-CLOSE`,
`WAIT-FOR-INPUT`. v1.4.20–23.

### ASDF + modules

- Module system: `BOCL:*MODULE-LIST*`, manifest-driven `REQUIRE`,
  `PROVIDE`, `*MODULES*` — Slices 122–124.
- ASDF 3.3.7 shipped as submodule [modules/asdf-src/](modules/asdf-src);
  single-file `asdf.lisp` produced by [modules/Makefile](modules/Makefile);
  UIOP Tier-1 portability hooks (Slice 125). `(require :asdf)` and
  `(asdf:load-system :some-system)` both work — v1.4.0 / v1.4.19.

### Debugger, on-error, dribble

- Minimal debugger with function-call backtrace frames — Slice 114.
- `--on-error debug|exit|abort|appease` command-line-ordered
  handling (Slice 117 + v1.4.9).
- REPL defaults to `debug`, batch to `exit` (Slice 118).
- `DRIBBLE` via callback-stream infrastructure (v1.4.4–6).

### Bignum-aware ops

Arithmetic auto-promotes across the tower; bitwise `ASH` / `LOGAND` /
`LOGOR` / `LOGXOR` bignum-aware (Slice 53) — but the
[deferred-bignum-ops](issues/open/deferred-bignum-ops.issue) issue
tracks the remaining edge cases (a few paths that still fall back to
fixnum arithmetic, `ISQRT` on very large bignums).

## What's still missing / still open

Now bounded and concentrated. Cross-referenced against
[issues/open/](issues/open/) (22 non-backup `.issue` files,
2026-07-07):

### Still open per issue tracker

- **[ansi-test](issues/open/ansi-test.issue)** — no submodule, no
  harness, no `make ansi-test` target. Blocker for the last quality
  gate.
- **[missing-functions](issues/open/missing-functions.issue)** —
  systematic audit of the ~120 remaining CL symbols. This document's
  overall-coverage table is the entry point.
- **[precise-condition-subclasses](issues/open/precise-condition-subclasses.issue)** —
  kernel signals `SIMPLE-ERROR` where a specific ANSI subclass is
  required.
- **[clos-condition-hierarchy-parents](issues/open/clos-condition-hierarchy-parents.issue)** —
  style-warning parent chain breaks `(TYPEP c 'WARNING)`.
- **[ratio-floor-round-truncate](issues/open/ratio-floor-round-truncate.issue)** —
  1-arg `FLOOR` / `ROUND` / `CEILING` / `TRUNCATE` reject a `RATIO`.
- **[subtypep-certainty](issues/open/subtypep-certainty.issue)** —
  returns `(NIL NIL)` for disjoint known types.
- **[open-element-type-default](issues/open/open-element-type-default.issue)** —
  `OPEN` rejects `:ELEMENT-TYPE :DEFAULT`.
- **[minimal-compilation](issues/open/minimal-compilation.issue)** —
  real `COMPILE` and eager `MACROEXPAND-ALL` at `LOAD` time.
- **[deferred-bignum-ops](issues/open/deferred-bignum-ops.issue)** —
  remaining bitwise + `ISQRT` paths that aren't bignum-aware.
- **[defstruct-conc-name-package](issues/open/defstruct-conc-name-package.issue)** —
  accessors interned in `*PACKAGE*` instead of the struct's home
  package.
- **[package-print-object-method](issues/open/package-print-object-method.issue)** —
  packages print as `#<PACKAGE 0x…>` instead of `#<PACKAGE "name">`.
- **[uiop-bocl-porting](issues/open/uiop-bocl-porting.issue)** — Tier
  1 done; further UIOP hooks remain.
- **[asdf-modules](issues/open/asdf-modules.issue)** — `REQUIRE`
  couldn't find asdf module at v1.4.19 (worked around; needs
  cleanup).
- **[asdf-source-registry-inherit-scan-slow](issues/open/asdf-source-registry-inherit-scan-slow.issue)** —
  ASDF `DIRECTORY` tree-walk hangs.
- **[slow-apropos](issues/open/slow-apropos.issue)** — 17 s for
  `(apropos "MODULE")`.
- **[deferred-signal-hooks](issues/open/deferred-signal-hooks.issue)** —
  extend `*ABORT-HOOK*` to `SIGTERM` / Core.
- **[deferred-sacla-logic-bugs](issues/open/deferred-sacla-logic-bugs.issue)** —
  residual GC + logic bugs in the SACLA harness.
- **[deferred-cons-paged-heap](issues/open/deferred-cons-paged-heap.issue)** —
  16-byte cons arena (perf).
- **[generational-gc](issues/open/generational-gc.issue)** —
  generational nursery further work.
- **[slice22-cls-vs-class-type-error](issues/open/slice22-cls-vs-class-type-error.issue)** —
  state-dependent `%CLS` / `CLASS` abort.
- **[structured-pathnames](issues/open/structured-pathnames.issue)** —
  core done v1.3.14; refinements remain.
- **[stale-test-suite-failures](issues/open/stale-test-suite-failures.issue)** —
  kernel-only suites lack prelude load.

### Still missing but not (yet) filed

- `UPDATE-INSTANCE-FOR-REDEFINED-CLASS` — not wired into the
  `DEFCLASS` re-evaluation path (was in the old TODO.md as a low-
  priority item).
- `DEFINE-METHOD-COMBINATION` full support — only the `standard`
  combination is implemented; user-defined combinations are stubs.
- Most of the MOP surface: `FIND-METHOD` variants,
  `METHOD-QUALIFIERS`, `COMPUTE-APPLICABLE-METHODS-USING-CLASSES`,
  class-precedence-list finalisation hooks.
- Pretty printer: `PPRINT-LOGICAL-BLOCK`, `PPRINT-POP`,
  `PPRINT-EXIT-IF-LIST-EXHAUSTED`, `PPRINT-INDENT`,
  `PPRINT-NEWLINE`, `PPRINT-TAB`, `SET-PPRINT-DISPATCH`.

## Variables still missing

Mostly compiler / debugger control:
`*COMPILE-FILE-PATHNAME*`, `*COMPILE-FILE-TRUENAME*`,
`*COMPILE-PRINT*`, `*COMPILE-VERBOSE*` (honoured but not fully wired
to a real compiler), a couple of `*DEBUGGER-*` and `*DEBUG-IO*`-
family details, `*MODULES*` is present but not fully surfaced.

## Constants still missing

FP-limit family: `LEAST-POSITIVE-*`, `LEAST-NEGATIVE-*`,
`LONG-FLOAT-EPSILON`, `SHORT-FLOAT-EPSILON`, `SINGLE-FLOAT-EPSILON`,
`DOUBLE-FLOAT-EPSILON` (some are aliases in bocl's single-float
world; needs explicit expose). `BOOLE-*` family is present.
`CHAR-CODE-LIMIT`, `MOST-POSITIVE-FIXNUM`, `MOST-NEGATIVE-FIXNUM`,
`PI`, `INTERNAL-TIME-UNITS-PER-SECOND` all defined.

## Bottom line

Where the v0.0.20 snapshot showed 25 % overall coverage against 978
CL external symbols, master (v1.4.30) is at roughly **88 %**. The
remaining gaps are enumerated in [issues/open/](issues/open/) plus a
handful of MOP hooks and the pretty printer. The next quality gate
is [ansi-test integration](issues/open/ansi-test.issue) (PLAN.md §3).
