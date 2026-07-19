# bocl completion plan

Companion to [README.org](README.org) and [design/design.org](design/design.org). This file is the operational plan for turning bocl into a fully conforming Common Lisp implementation validated against ansi-test.

## Project conventions

### Version policy

The current version is held in [sources/version.h](sources/version.h) as
`BOCL_VERSION_MAJOR.BOCL_VERSION_MINOR.BOCL_VERSION_PATCH`. The banner,
`--version`, and the Lisp-visible `(LISP-IMPLEMENTATION-VERSION)` all read
from there.

**Rule**: every commit that touches `sources/`, `tests/`, `bootstrap/`, or
any `documents` material (PDFs, .org files) must bump `BOCL_VERSION_PATCH`
by exactly one. PR descriptions and commit messages should reference the
resulting version. MAJOR and MINOR only move on intentional milestone
bumps (e.g. ansi-test passing, first self-host).

### Release procedure

**Rule**: before tagging a release `vM.m.d` and branching
`release-M.m.d`, refresh the status documents so the release snapshot
is self-describing:

1. Update [CL-STATUS.md](CL-STATUS.md) — re-survey the CL-symbol
   coverage table, fold in everything landed since the previous
   snapshot, and re-cross-reference the "still open" list against
   [issues/open/](issues/open/).
2. Update [TODO.md](TODO.md) — consolidate the "Landed since" section
   for the released version band, regroup "Still Remaining" to match
   the current [issues/open/index.txt](issues/open/index.txt) triage,
   and move verified closures to the "Completed items" section.
3. Update [README.org](README.org) — the "Current status" section
   (version number, coverage, test/CI state) and the "Future plans"
   section (drop delivered items, reflect the current
   [issues/open/index.txt](issues/open/index.txt) triage).

The release tag must point at a commit where all three documents
describe that very version.

## 1. Audit (current state, 2026-07-07 — v1.4.30)

bocl follows a two-layer bootstrap: a small **C kernel** plus a **Lisp library** under [bootstrap/](bootstrap/) loaded on top of it. The original audit (2026-05-24, v0.0.10 era) estimated 30–40 % of the bootstrap target; master (v1.4.30) is now at roughly **88 %** by CL-external-symbol coverage — see [CL-STATUS.md](CL-STATUS.md).

### Working in the kernel

| Subsystem | Files | Notes |
|---|---|---|
| Mark-sweep + **generational nursery GC**, multi-zone heap, write barrier | [sources/memory.c](sources/memory.c) | Solid (v1.0.x nursery landed Slices 168–174) |
| Tagged Refs, ~80 declared class objects incl. `Bignum` / `Ratio` / `Complex` / `Condition` / `Package` / `Restart` | [sources/kernel_types.h](sources/kernel_types.h), [sources/kernel_objects.c](sources/kernel_objects.c) | Type tags + CLOS metaclass behaviour |
| Cons / list ops | [sources/cons.c](sources/cons.c) | Solid |
| Symbols + **true per-package obarrays** (present + external) | [sources/kernel.c](sources/kernel.c), [sources/packages.c](sources/packages.c) | Full ANSI package semantics (Slice 46 v0.0.71; must-package = 0 at Slice 113) |
| Characters, strings, vectors, bit-vectors, multi-dim arrays, displaced/adjustable/fill-pointer | [sources/character.c](sources/character.c), [sources/bostring.c](sources/bostring.c), [sources/vector.c](sources/vector.c), [sources/bit_vector.c](sources/bit_vector.c) | Solid (Slices 94–104) |
| Structures + full CLOS (`DEFCLASS`, `DEFGENERIC`, `DEFMETHOD`, `MAKE-INSTANCE`, `:before`/`:after`/`:around`, `:allocation :class`, `CHANGE-CLASS`, `REINITIALIZE-INSTANCE`, `SHARED-INITIALIZE`) | [sources/structure.c](sources/structure.c) + [bootstrap/prelude.lisp](bootstrap/prelude.lisp) | Core covered; MOP hooks + user-defined method combinations still stubbed |
| Reader — full macro set (`#'`, `#\`, `#+`/`#-`, `#\|…\|#`, `pkg:sym`, `#=`/`##`, `#.`, `SET-MACRO-CHARACTER`, `#C`, `#S`, `#nA`, `\|…\|`, `READTABLE-CASE`) | [sources/reader.c](sources/reader.c) | Complete for ANSI conforming code |
| Printer — every `*PRINT-*` control var wired up (`*PRINT-CIRCLE*` includes `#n=`/`#n#`) | [sources/printer.c](sources/printer.c) | Solid; pretty printer not implemented |
| Streams (string + fd + broadcast/concatenated/synonym/two-way + callback) | [sources/stream.c](sources/stream.c) | Solid |
| Pathnames + **logical pathnames** (`TRANSLATE-LOGICAL-PATHNAME`, `PATHNAME-MATCH-P`, `WILD-PATHNAME-P`) + `PARSE-NAMESTRING` | [sources/filename.c](sources/filename.c) | Slice 33 v0.0.57; core done (`structured-pathnames` issue tracks refinements) |
| BSD sockets kernel + `BOCL-SOCKET` usocket-compatible package | [sources/socket.c](sources/socket.c), [bootstrap/socket.lisp](bootstrap/socket.lisp) | v1.4.20–23 |
| Environments + frames + special operators (all 25) + minimal debugger with backtrace | [sources/environment.c](sources/environment.c), [sources/eval.c](sources/eval.c) | Solid |
| Full arithmetic tower (integer / bignum / ratio / complex / float) + transcendentals | [sources/arithmetic.c](sources/arithmetic.c), [bootstrap/bignum.lisp](bootstrap/bignum.lisp) | Auto-promotion; edge cases tracked in [deferred-bignum-ops](issues/open/deferred-bignum-ops.issue) and [ratio-floor-round-truncate](issues/open/ratio-floor-round-truncate.issue) |
| Hash tables — O(1) bucket-backed | [sources/hash_table.c](sources/hash_table.c) | Solid (Slice 166) |
| Conditions + restart machinery + full ANSI condition hierarchy + `DEFINE-CONDITION` on top of CLOS | [sources/conditions.c](sources/conditions.c) + [sources/eval.c](sources/eval.c) + [bootstrap/prelude.lisp](bootstrap/prelude.lisp) | Solid; [precise-condition-subclasses](issues/open/precise-condition-subclasses.issue) tracks where kernel still signals `SIMPLE-ERROR` |
| ~200+ generated `kernel_*` built-ins + hand-registered arithmetic / socket / package / hash-table primitives | [sources/built_in_functions.c](sources/built_in_functions.c) + others | Solid |

### Missing or broken in the kernel

| Gap | Where | Severity |
|---|---|---|
| Real `COMPILE` and eager `MACROEXPAND-ALL` at `LOAD` time (currently `COMPILE` is minimal, `LOAD` interprets) | [minimal-compilation issue](issues/open/minimal-compilation.issue) | Deferred |
| `DEFINE-METHOD-COMBINATION` full support (only standard combination) | [bootstrap/prelude.lisp](bootstrap/prelude.lisp) | Required for full CLOS |
| Pretty printer (`PPRINT-LOGICAL-BLOCK`, dispatch table) | — | Required |
| MOP hooks (`FIND-METHOD` variants, `METHOD-QUALIFIERS`, `COMPUTE-APPLICABLE-METHODS-USING-CLASSES`, class-precedence-list finalisation) | — | Required |
| `UPDATE-INSTANCE-FOR-REDEFINED-CLASS` (class-redefinition path) | — | Required |
| A handful of edge-case type errors and stubs — see [issues/open/](issues/open/) | | Bug fixes |

### Lisp side

[bootstrap/prelude.lisp](bootstrap/prelude.lisp) is ~10 700 lines and is loaded on every startup unless `--no-prelude`. [bootstrap/socket.lisp](bootstrap/socket.lisp) (~720 lines) provides the `BOCL-SOCKET` package. ASDF 3.3.7 is shipped as a submodule at [modules/asdf-src/](modules/asdf-src) and built into a single-file `asdf.lisp` by [modules/Makefile](modules/Makefile) — `(require :asdf)` and `(asdf:load-system :some-system)` both work (v1.4.19).

### Tests

47 `test_*.c` CUnit suites under [tests/](tests/) linked against `libbocl`, plus the SACLA `must-*` and `should-*` suites under `bootstrap/sacla/lisp/tests/`. `must-*` is GREEN at v1.3.0; `should-*` remains partially failing per [should-tests-type-errors](issues/open/should-tests-type-errors.issue). CI at [.gitlab-ci.yml](.gitlab-ci.yml) runs a matrix (Debian gcc/clang/tcc + Alpine musl + FreeBSD + macOS) plus a dedicated ASan/UBSan sanitiser job.

### Overall

**~88 %** of the bootstrap target. Phase 5 (Lisp library bring-up) is essentially complete: CLOS, LOOP, FORMAT, full sequence/string/numeric libraries, conditions, restarts, ASDF, sockets, dribble, debugger. The remaining large item is [ansi-test integration](issues/open/ansi-test.issue) — see §3.

---

## 2. Phased work plan

### Phase 0 — build hygiene (1–2 days)

- Make non-sanitised the default macOS build; gate ASan behind `SANITIZE=yes`. Fixes the dyld `libclang_rt.asan_osx_dynamic.dylib` failure on prebuilt binaries.
- Auto-detect a host CL (`ccl`/`sbcl`/`clisp`) for code generation; document the requirement.
- `make regenerate` target — regen `kernel_objects.*`, `built_in_functions.*`, `kernel_structure_functions.*`, `structure_functions.*`. Keep generated outputs committed.
- `make clean-backups` target — remove `*.~N~` files; gitignore them.
- Resurrect `gitlab-ci.yml-test` into `.gitlab-ci.yml` so existing reader tests run in CI.

### Phase 1 — make `EVAL` actually evaluate ✅ DONE

The single unblocker. Landed in commit *(see git log)*.

What shipped:

- `bocl_eval` now calls `evale` with a lazily-allocated global lexical environment held by the GC rootset.
- `kernel_apply_function` / `kernel_apply_closure` moved from `kernel.c` (where they were `NOT_IMPLEMENTED_YET`) into [eval.c](sources/eval.c), where they can use the frame machinery. Mandatory args only — `&optional` / `&rest` / `&key` / `&aux` raise an explicit error.
- `evale` dispatch rewritten: drops the placeholder `f()`, tries the env's function namespace first (FLET/LABELS), falls back to `symbol_function`. Macros dispatch via `env_local_macro_p` / `env_global_macro_p` wrappers.
- `SETQ` implemented with multi-pair semantics: lexical assign if found in env, else global `set_symbol_value`.
- `FLET` and `LABELS` bind `env_local_function` wrappers; the dispatch turns them into transient `Closure`s captured over the binding env.
- `DEFUN`, `DEFPARAMETER`, `DEFVAR`, `DEFCONSTANT` as pseudo-special-operators in the kernel (move to the Lisp side once the macro system lands).
- `(function …)` and `((lambda …) …)` head positions both work.
- `environment_find_variable` / `_find_function` / `_find_block` / `_find_catch` implemented (they were `NOT_IMPLEMENTED_YET`).
- `env_variable_value` bug fix — was returning the *name* slot instead of the value slot of every wrapper class.

Incidental bug fixes en route:

- **Reader**: `parse_list` and `parse_string` did not advance past their closing delimiter (`)` / `"`); an outer `parse_list` then saw the inner's close-paren as its own. Fixed.
- **Variable subsystem**: `make_special_variable` checked `special_variable_p` (so it errored on idempotent `defparameter`); now checks `constant_variable_p` per the error message, and both `make_*_variable` are idempotent.
- **CATCH / THROW**: neither evaluated its tag form; `CATCH`'s body wrongly skipped symbol/integer forms; `THROW`'s and `RETURN-FROM`'s error path called `error()` with the format string in the function-name slot. All fixed.
- **`NON_LOCAL_EXIT`** macro popped the current frame before `unwind_next_frame`, which crashed when the target frame was outermost (`frame->next == NULL`). Rewrote so the `UNWIND` state machine pops each frame on setjmp re-entry.

**Milestone met**: `(defun double (n) (let ((x n)) x)) (double 4)` → `4`. Multiplication waits for Phase 2.

Tests: [tests/test_eval.c](tests/test_eval.c) — 17 CUnit cases covering quote / self-eval / if / progn / setq (single + multi-pair) / let / defparameter / defun (identity + let body) / lambda head / `(function …)` / flet / labels / block / `(block … (return-from …))` / catch+throw. All green.

### Phase 2 — numbers + arithmetic in the kernel ✅ DONE

Shipped in a new [sources/arithmetic.c](sources/arithmetic.c) (and `.h`),
registered at startup via `kernel_initialize_arithmetic()`, called from
`kernel_initialize()` right after the auto-generated `built_in_functions`.
Hand-registered rather than going through `generate-built-in-functions.lisp`
so the work is self-contained and reviewable; the generator can absorb
these later.

What landed (all 2-arg primitives; n-ary forms stay for the Lisp side):

- **arithmetic**: `+  -  *  /` with overflow detection via
  `__builtin_*_overflow`; mixed int/float promotes to float; integer `/`
  promotes to float when the division is inexact (no ratios yet).
- **comparisons**: `=  /=  <  >  <=  >=` (numeric — so `(= 1 1.0) ⇒ T`).
- **predicates**: `ZEROP  PLUSP  MINUSP  ODDP  EVENP`.
- **magnitude**: `1+  1-  ABS  MIN  MAX`.
- **integer division family**: `TRUNCATE  FLOOR  CEILING  ROUND` all
  return 2 values (quotient, remainder); `ROUND` uses banker's rounding
  (ties to even). Plus `MOD` (sign of divisor) and `REM` (sign of dividend).
  1-arg forms supported (divisor defaults to 1).
- **bitwise**: `LOGAND  LOGOR  LOGXOR  LOGNOT  ASH  INTEGER-LENGTH`.
- **coercion**: `FLOAT`, `INTEGER` (truncating), and a narrow `COERCE`
  that handles `FLOAT / SINGLE-FLOAT / DOUBLE-FLOAT / SHORT-FLOAT /
  LONG-FLOAT / INTEGER / FIXNUM / NUMBER` targets. Full `COERCE` stays
  on the Lisp side.

Deferred to the Lisp library (per design): ratios, complex, bignum
(in [bootstrap/bignum.lisp](bootstrap/bignum.lisp)).

Incidental reader fix: `scan_integer` accepted a sign anywhere in the
token, so `1+` parsed as the integer `1` (and the `+` was silently
dropped, leaving callers with a misleading "malformed form" error). Now
the sign is only honoured at position 0.

**Milestones met**:
- `(defun sq (n) (* n n)) (sq 4)` → `16`.
- `(defun fct (n) (if (= n 0) 1 (* n (fct (1- n))))) (fct 5)` → `120`,
  `(fct 6)` → `720`. Self-recursion works.
- `(* 10000 10000 10000)` → `1000000000000`. Variadic arithmetic landed
  in v0.0.10 — `+ - * / = /= < > <= >= MIN MAX` all accept 0/1/n args
  per CL (e.g. `(+) ⇒ 0`, `(*) ⇒ 1`, `(- x) ⇒ -x`, `(/ x) ⇒ 1/x`).

**Numeric range**: the kernel has no fixnum/bignum split — every
integer is a word. `MOST-POSITIVE-FIXNUM = WORD_MAX` and
`MOST-NEGATIVE-FIXNUM = WORD_MIN` are exposed as kernel constants.
Bignums are planned for the Lisp side (see
[bootstrap/bignum.lisp](bootstrap/bignum.lisp)) and will narrow
`MOST-POSITIVE-FIXNUM` once loaded.

Tests: [tests/test_arithmetic.c](tests/test_arithmetic.c) — 30 CUnit
cases covering every operator with edge cases (zero, negative, mixed
int+float, division-by-zero promotion, MOD vs REM sign, banker's
rounding ties-to-even, bitwise `ASH` shift-by-zero/large/negative,
`INTEGER-LENGTH` on negatives, variadic forms including identities,
fixnum constants, `(lisp-implementation-{type,version})`). All green.

### Phase 3 — file load + packages ✅ DONE

Two halves, both landed.

**Half 1 — file LOAD (v0.0.12)**:

- `stream_open` in [sources/stream.c](sources/stream.c) now supports `:input`,
  `:output`, `:io`, `:probe` with the standard `:if-exists` /
  `:if-does-not-exist` combinations. Added `stream_close`.
- `bocl_load_cstring` rewritten to actually open the file, read forms until
  EOF, eval each through `bocl_eval`, close, return `T`. Honours
  `*LOAD-VERBOSE*` and `*LOAD-PRINT*` (now initialised in the kernel, not
  main.c, so tests work too).
- New kernel built-in `(LOAD filespec)`, registered at startup by
  `bocl_register_load()`.
- Incidental fixes:
  - `physical_pathname(string)` was aliasing the string's `OctetArray` as a
    `VectorOfOctet` — pathname round-trip produced garbage.
  - `kernel_read` on empty input fed the `EOF` byte (0xFF) into
    `parse_token`, which interned an empty-named symbol; now bail right
    after `skip_spaces` if at EOF.

**Half 2 — minimal package system (v0.0.13)**:

- New [sources/packages.{c,h}](sources/packages.c) defines a `Package`
  struct (name / nicknames / symbols / exported / used) added to the
  `Slots` union; three packages pre-built at startup:
  `COMMON-LISP` (nickname `CL`), `COMMON-LISP-USER` (nickname `CL-USER`,
  uses `COMMON-LISP`), `KEYWORD`.
- `*PACKAGE*` defined; defaults to `CL-USER`.
- Built-ins: `FIND-PACKAGE`, `PACKAGE-NAME`, `PACKAGE-NICKNAMES`,
  `MAKE-PACKAGE`, `USE-PACKAGE`, `EXPORT`, `INTERN`, `FIND-SYMBOL`,
  `PACKAGEP`, `LIST-ALL-PACKAGES`. (The auto-generated `kernel_INTERN`
  / `kernel_FIND_SYMBOL` are still defined but the package-aware
  versions override them at startup.)
- `(IN-PACKAGE designator)` is recognised by `evale` as a
  pseudo-special-operator (CL spec makes it a macro) — actually switches
  `*PACKAGE*`.
- `bocl_in_package` C entry point is wired through to set `*PACKAGE*`;
  the old "Assume a package system is implemented" stub message is gone.

**Known limitation, by design**: all symbols still live in the single
global `KERNEL_obarray`. Two packages can't currently hold distinct
symbols with the same name — they share one symbol per name. The PLAN's
"internal/external symbol tables per package" is therefore a partial
implementation that does the bookkeeping but not the isolation. Real
isolation is queued for the bignum/CLOS phase; ansi-test's package
identity tests will fail until then.

**Milestone met**:
```lisp
;; demo.lisp
(in-package :common-lisp-user)
(defparameter *x* 42)
(defun double (n) (* n 2))
```
```
$ echo '(load "demo.lisp") (print (double *x*))' | bocl --batch -q
=> 84
```

Tests:
- [tests/test_load.c](tests/test_load.c) — 6 cases, all green.
- [tests/test_packages.c](tests/test_packages.c) — 8 cases, all green.

### Phase 4 — hash tables, conditions, full reader ✅ DONE

Landed in three slices.

**4.1 — hash tables (v0.0.14)**: Alist-backed `HashTable` (per design),
new [sources/hash_table.c](sources/hash_table.c). Built-ins:
`MAKE-HASH-TABLE`, `HASH-TABLE-P`, `HASH-TABLE-COUNT`, `HASH-TABLE-TEST`,
`GETHASH` (returns two values), `SETHASH` (the `(setf gethash)` primitive
— SETF macros not yet loaded), `REMHASH`, `CLRHASH`, `MAPHASH`. The four
test functions `EQ / EQL / EQUAL / EQUALP` all work; the new `EQ` and
`EQUALP` built-ins were missing from the kernel before this slice.

Incidental: `EQUAL` on strings was case-insensitive (using
`string_equal`); fixed to be case-sensitive per CL spec.

**4.2 — condition system (v0.0.15)**: New [sources/conditions.c](sources/conditions.c)
with a `Condition` slot type. `HANDLER-CASE` special-operator in
[eval.c](sources/eval.c), `SIGNAL` / `ERROR` / `WARN` / `MAKE-CONDITION`
/ `CONDITIONP` / `SIMPLE-CONDITION-FORMAT-CONTROL` / `-FORMAT-ARGUMENTS`
built-ins. Frames carry the firing clause + condition so the handler
body can bind a variable to the condition. `HANDLER-BIND`, restarts,
and `CERROR` are deferred (the dynamic-context machinery they need is
more involved than HANDLER-CASE's unwind).

Incidental: `copy_list` in [cons.c](sources/cons.c) had a missing
`tail = new_cell` advance and a trailing `rplacd` that clobbered the
result — `(list 'a 'b 'c)` returned `(a)`. Symptom hidden because the
kernel uses the C variadic `list()` helper internally. Now fixed.

**4.3 — full reader (v0.0.16)**: New dispatch macros in
[reader.c](sources/reader.c):
- `#\(`, `#\)`, `#\"` and other punctuation character literals (pre-
  existing bug: `parse_character` didn't advance past the literal char,
  so the outer `parse_list` saw it as a syntactic close-paren).
- `#'name` → `(FUNCTION name)`.
- `#|…|#` nestable block comments.
- `#+feature` / `#-feature` with `AND` / `OR` / `NOT` compound
  feature expressions. Symbols in the feature expression are coerced to
  keywords (CL spec reads them with `*PACKAGE*` bound to KEYWORD).
- `pkg:sym` and `pkg::sym` package-qualified symbol syntax. Since
  Phase 3's package isolation isn't real yet, both forms intern in the
  shared `KERNEL_obarray` and return the bare-name symbol; the package
  designator is validated but otherwise ignored. `KEYWORD:foo` reads
  as `:foo`.

Skip semantics for `#+/#-` use an internal `reader_skip_marker`
sentinel that `parse_list` and `kernel_read` filter out.

Incidental: `kernel_initialize_features` was interning the
`*FEATURES*` symbol via `S("*FEATURES*")` (quoted), which stringifies
to the literal `"*FEATURES*"` (with the quotes in the name). Bug
existed since Phase 0 but didn't bite until the reader looked up
`*FEATURES*` directly. Fixed.

Tests: [tests/test_hash_table.c](tests/test_hash_table.c) (11),
[tests/test_conditions.c](tests/test_conditions.c) (8),
[tests/test_reader_macros.c](tests/test_reader_macros.c) (8). Plus
[tests/test_reader.c](tests/test_reader.c) is now 100% green — the
last pre-existing `#\(` failure is gone.

Still deferred to a later phase:
- `HANDLER-BIND` and restarts (`RESTART-CASE / -BIND`, `FIND-RESTART`,
  `INVOKE-RESTART`, `CERROR`).
- Reader: `#=` / `##` (circular structures), `#.` (eval at read time),
  `set-macro-character`, `set-dispatch-macro-character`, custom
  readtables.

### Phase 5 — load the Lisp library (2–3 weeks)

Bring up [bootstrap/](bootstrap/) on top of the kernel, in order:

1. [bootstrap/bootstrap-packages.lisp](bootstrap/bootstrap-packages.lisp), [bootstrap/bootstrap-cl-package.lisp](bootstrap/bootstrap-cl-package.lisp)
2. [bootstrap/operators.lisp](bootstrap/operators.lisp)
3. [bootstrap/minimal-compiler.lisp](bootstrap/minimal-compiler.lisp) — macros
4. [bootstrap/tiny-reader.lisp](bootstrap/tiny-reader.lisp), [bootstrap/tiny-printer.lisp](bootstrap/tiny-printer.lisp), [bootstrap/simple-reader.lisp](bootstrap/simple-reader.lisp), [bootstrap/simple-sexp.lisp](bootstrap/simple-sexp.lisp)
5. [bootstrap/bignum.lisp](bootstrap/bignum.lisp), ratios, complex
6. CLOS — port a CLOSette-style implementation
7. `format`, `loop`, sequence library — new files

One Lisp-level test file per step under `tests/lisp/`.

#### Phase 5 progress (slices landed)

**Slice 1 — LET\*, MACROLET, SYMBOL-MACROLET, LOAD-TIME-VALUE (v0.0.21)**:
All four special operators wired into `evale`. `LET*` sequences its
bindings; `MACROLET` establishes local macro definitions visible in its
body; `SYMBOL-MACROLET` expands symbol references inline; `LOAD-TIME-VALUE`
evaluates at load time and caches the result. Test: `test_setf.c` (covers
SYMBOL-MACROLET round-trip). All green.

**Slice 2 — SETF + DEFINE-SETF-EXPANDER (v0.0.22)**:
`SETF` macro expands via built-in place descriptors for `CAR`, `CDR`,
`AREF`, `GETHASH`, `SYMBOL-VALUE`, plus any user-defined expanders.
`DEFINE-SETF-EXPANDER` stores the expander in the function cell of the
accessor symbol. Tests: [tests/test_setf.c](tests/test_setf.c) — 12 cases, all green.

**Slice 3 — control macros (v0.0.23)**:
`WHEN`, `UNLESS`, `COND`, `CASE`, `TYPECASE`, `DO`, `DO*`, `DOTIMES`,
`DOLIST`, `LOOP` (simple iteration), `AND`, `OR`, `NOT`, `PROG1`,
`PROG2`, `PROG`, `TAGBODY`/`GO`. Tests:
[tests/test_control.c](tests/test_control.c) — all green.

**Slice 4 — sequence library (v0.0.24)**:
`APPEND`, `REVERSE`, `NREVERSE`, `LENGTH`, `ELT`, `SUBSEQ`, `COPY-SEQ`,
`CONCATENATE`, `MAP`, `MAP-INTO`, `REDUCE`, `COUNT`, `FIND`, `POSITION`,
`REMOVE`, `DELETE`, `SUBSTITUTE`, `NSUBSTITUTE`, `SORT`, `STABLE-SORT`,
and their `-IF` / `-IF-NOT` variants. Tests:
[tests/test_sequences.c](tests/test_sequences.c) and
[tests/test_seq_lib.c](tests/test_seq_lib.c) — all green.

**Slice 5 — string / character library (v0.0.25)**:
`STRING-UPCASE`, `STRING-DOWNCASE`, `STRING-CAPITALIZE`, `STRING-TRIM`,
`STRING=`, `STRING<`, `STRING>`, `STRING-EQUAL` (case-insensitive),
`CHAR-UPCASE`, `CHAR-DOWNCASE`, `CHAR-ALPHABETIC-P`, `CHAR-NUMERIC-P`,
`ALPHA-CHAR-P`, `DIGIT-CHAR-P`, `CHAR-CODE`, `CODE-CHAR`, `CHAR=`,
`CHAR<`, `STRING-TO-NUMBER`, `NUMBER-TO-STRING`, `PARSE-INTEGER`.
Tests: [tests/test_strings.c](tests/test_strings.c) — all green.

**Slice 6 — numeric extras (v0.0.26)**:
`EXPT`, `SQRT`, `EXP`, `LOG`, `SIN`, `COS`, `TAN`, `ASIN`, `ACOS`,
`ATAN`, `SINH`, `COSH`, `TANH`, `FLOOR`, `CEILING`, `TRUNCATE`, `ROUND`
(extended — already in Phase 2; this slice added `GCD`, `LCM`,
`ISQRT`, `SIGNUM`, `CONJUGATE`, `PHASE`, `REALPART`, `IMAGPART`, `CIS`,
`NUMERATOR`, `DENOMINATOR`, `FLOAT-DIGITS`, `FLOAT-PRECISION`,
`SCALE-FLOAT`, `DECODE-FLOAT`, `INTEGER-DECODE-FLOAT`,
`FLOAT-RADIX`).
Tests: [tests/test_numeric_extras.c](tests/test_numeric_extras.c) — all green.

**Slice 7 — DESTRUCTURING-BIND (v0.0.28)**:
Full destructuring pattern matching including nested lists, `&optional`,
`&rest`, `&key`, `&aux`, `&whole`, `&body`. Shared with DEFMACRO's
lambda-list parser. Tests:
[tests/test_destructuring.c](tests/test_destructuring.c) — all green.

**Slice 8 — DEFSTRUCT (v0.0.29)**:
`DEFSTRUCT` expands into a structure class plus generated `make-*`,
`*-p`, and slot accessor functions. Supports `:conc-name`,
`:constructor`, `:predicate`, `:include`. Tests:
[tests/test_structures.c](tests/test_structures.c) — all green.

**Slice 9a — HANDLER-BIND + restarts (v0.0.30)**:
`HANDLER-BIND` establishes dynamic condition handlers without unwinding;
`RESTART-CASE`, `RESTART-BIND`, `FIND-RESTART`, `INVOKE-RESTART`,
`CERROR`, `WITH-SIMPLE-RESTART`. Tests:
[tests/test_handler_bind.c](tests/test_handler_bind.c) and
[tests/test_restarts.c](tests/test_restarts.c) — all green.

**Slice 11 — LOOP macro (v0.0.33)**:
Full `LOOP` macro with: iteration drivers `for/in`, `for/on`, `for/across`,
`for/=then`, `for/from/to/below/by`; `repeat`; `while`/`until` guards;
accumulation clauses `sum`, `count`, `maximize`, `minimize`, `collect`,
`append`; termination tests `always`/`never`/`thereis`; auxiliary clauses
`with`, `initially`, `finally`; named blocks with `named`/`return`;
parallel `and` for-clauses. Implemented in `bootstrap/prelude.lisp`.
GC live-pool fix: `structure_class_instance_direct_slots` and
`structure_class_instance_slots` in `sources/structure.c` no longer close
their pool before returning, preventing GC from sweeping the returned cons
lists while the caller still holds them. Tests:
[tests/test_loop.c](tests/test_loop.c) — 20 cases.

**Slice 12 — list / assoc / tree utilities (v0.0.34)**:
Association-list functions: `ASSOC`, `ASSOC-IF`, `ASSOC-IF-NOT`, `RASSOC`,
`RASSOC-IF`, `RASSOC-IF-NOT`. List utilities: `LIST*`, `COPY-TREE`,
`TAILP`, `NCONC` (variadic), `DELETE-IF`, `DELETE-IF-NOT`,
`REMOVE-DUPLICATES`, `DELETE-DUPLICATES`. Tree operations: `SUBST`,
`SUBST-IF`, `SUBST-IF-NOT`, `TREE-EQUAL`. Property lists: `GET`,
`REMPROP`, `(SETF GET)`. Type predicates: `NUMBERP`, `INTEGERP`, `FLOATP`,
`RATIONALP`, `REALP`, `COMPLEXP`, `CHARACTERP`, `VECTORP`, `ARRAYP`,
`PACKAGEP`, `PATHNAMEP`, `STREAMP`, `HASH-TABLE-P`, `RANDOM-STATE-P`.
Setf expanders for `SECOND`, `THIRD`, `FOURTH`, `CAAR`, `CADR`, `CDAR`,
`CDDR`, `CADDR`, `CADDDR`, `AREF`. All implemented in
`bootstrap/prelude.lisp`. Tests:
[tests/test_list_utils.c](tests/test_list_utils.c) — 15 cases, all green.

**Slice 13 — UNWIND-PROTECT fix + CLOSE/byte-I/O + print vars (v0.0.35)**:
`UNWIND-PROTECT` result-passing was broken: the protected-form result was
overwritten by the cleanup forms. Fixed by separating `protect_results` from
`tmp_results` and adding the missing `PASS_VALUES()` after the unwind block.
`CLOSE` (`:abort` keyword ignored, idempotent on already-closed streams),
`READ-BYTE`, `WRITE-BYTE`. Dynamic print variables `*PRINT-READABLY*`,
`*PRINT-ESCAPE*`, `*PRINT-PRETTY*`, `*PRINT-CIRCLE*`, `*PRINT-BASE*`,
`*PRINT-LEVEL*`, `*PRINT-LENGTH*`, `*PRINT-GENSYM*`, `*PRINT-RADIX*`,
`*PRINT-CASE*`, `*PRINT-ARRAY*`, `*PRINT-LINES*`,
`*PRINT-MISER-WIDTH*`, `*PRINT-RIGHT-MARGIN*`.
`WRITE-TO-STRING` with full `&rest keys` interface honouring `:escape`.

**Slice 18 — LOOP when/unless/if, generic REDUCE, DO-SYMBOLS, stream extras (v0.0.41)**:
LOOP conditional sub-clauses: `WHEN`, `UNLESS`, and `IF`/`ELSE` for `COLLECT`, `DO`, `RETURN`, and `SUM` actions.
Generic `REDUCE` over lists, vectors, strings; `:from-end`, `:initial-value`, and `:key` keyword support.
`DO-SYMBOLS`, `DO-EXTERNAL-SYMBOLS`, `DO-ALL-SYMBOLS` package-iteration macros.
Stream extras: `LISTEN` (implemented in prelude via `PEEK-CHAR`), `READ-CHAR-NO-HANG`, `CHAR-READY-P`.
GC bug fix: `structure_class_instance_direct_slots` used `push()` inside a `dolist` loop; `push()` closes its own live pool, stranding the growing alist outside GC protection between iterations.  Fixed by replacing `push(x, &alist)` with `alist = cons(x, alist)` so the accumulator stays pool-visible throughout.
Bug fix: `variable_value(S(NIL))` / `variable_value(S(T))` in `built_in_functions.c` replaced with direct `NIL_Symbol` / `T_Symbol` — NIL and T are not ordinary variables in bocl.
Bug fix: `fmemopen(buf, 0, "r")` returns NULL on macOS; empty string-streams now use a 1-byte static buffer seeked to EOF.
Tests: [tests/test_slice18.c](tests/test_slice18.c) — 19 cases, all green.

**Slice 17 — generic sequences, -if-not variants, pathname accessors, map-into (v0.0.40)**:
`EVERY`, `SOME` extended to strings and vectors.
`FIND-IF`, `FIND`, `POSITION-IF`, `POSITION`, `COUNT-IF`, `COUNT`, `REMOVE-IF`, `REMOVE` all genericised to handle lists, strings, and vectors via `ELT`/`LENGTH`.
`FIND-IF-NOT`, `POSITION-IF-NOT`, `COUNT-IF-NOT`, `REMOVE-IF-NOT` — new -if-not variants delegating to their -if counterparts.
`SORT`/`STABLE-SORT` extended: vector input coerced to list, sorted, written back.
Pathname accessors: `PATHNAME-NAME`, `PATHNAME-TYPE`, `PATHNAME-DIRECTORY`, `PATHNAME-HOST`, `PATHNAME-DEVICE`, `PATHNAME-VERSION`, `FILE-NAMESTRING`, `DIRECTORY-NAMESTRING`, `HOST-NAMESTRING`, `ENOUGH-NAMESTRING`, `MERGE-PATHNAMES`, `MAKE-PATHNAME` (all POSIX string-based, no C changes needed).
`MAP-INTO`.
`WITH-STANDARD-IO-SYNTAX` macro.

**Slice 16 — char comparisons, generic subseq, coerce, string concatenate (v0.0.39)**:
`CHAR=`, `CHAR/=`, `CHAR<`, `CHAR>`, `CHAR<=`, `CHAR>=` (exact case-sensitive character comparisons).
`CHAR-LESSP`, `CHAR-GREATERP`, `CHAR-NOT-GREATERP`, `CHAR-NOT-LESSP`, `CHAR-NOT-EQUAL` (case-insensitive variants).
`DIGIT-CHAR` (convert integer weight to digit character in given radix).
`SUBSEQ` extended to handle strings (via `SUBSEQ-STRING`) and vectors.
`COERCE` Lisp wrapper around C `kernel_COERCE` adding sequence type conversions:
LIST→VECTOR, VECTOR→LIST, LIST→STRING, STRING→LIST, STRING→VECTOR, and identity cases.
`CONCATENATE` extended for `STRING` result-type.

**Slice 15 — TYPE-OF, AREF, CHAR/SCHAR, SLEEP, time ops, MAP, list utils (v0.0.38)**:
`TYPE-OF` (returns a type specifier symbol for any object).
`AREF` and `(SETF AREF)` (via `AREF-SET`) for 1-D simple-vectors.
`CHAR`, `SCHAR`, `(SETF CHAR)` / `(SETF SCHAR)` (via `CHAR-SET`).
`SLEEP` (non-negative real seconds, uses `nanosleep`).
`LISTEN` (character available predicate, uses `select`).
`CLEAR-INPUT` (discards buffered input, `fpurge`/`tcflush`).
`DECODE-UNIVERSAL-TIME` (9 values: sec min hr day mon yr dow dst tz).
`ENCODE-UNIVERSAL-TIME` (accepts optional time-zone offset).
`MULTIPLE-VALUE-LIST` and `NTH-VALUE` macros in `bootstrap/prelude.lisp`
(were missing despite being used by other macros).
`WITH-OPEN-STREAM` macro.
`SUBTYPEP` (basic type-hierarchy query; returns `(values subtypep certainp)`).
`MAP` (general sequence mapping with NIL / LIST / VECTOR / STRING result-type).
`MAPL` (maplist variant that returns the original first list).
`NBUTLAST` (destructive butlast).
`LDIFF` (fresh list of elements before a given tail).
`GENTEMP` (intern a unique symbol, counter-based prefix).
`setf` expanders for `AREF`, `CHAR`, `SCHAR` added to prelude.
Bug-fix: removed duplicate (non-`static`) `kernel_CHAR_SET` declaration from
`built_in_functions.h` / `built_in_functions.c` that conflicted with the
`static` definition in `sequences.c`.
fmemopen FILE\* leak fix: `string_output_stream_get_string()` now `fclose`s
the backing FILE\* and marks the stream closed after consuming it, preventing
accumulation of open FILE\* handles under repeated WITH-OUTPUT-TO-STRING use.
Tests: [tests/test_slice15.c](tests/test_slice15.c) — 15 cases, all green.

**Slice 14 — arrays, sequences, random, file-system, timing (v0.0.36)**:
`MAKE-ARRAY` (1-D, `:initial-element`, `:initial-contents`),
`ARRAY-RANK`, `ARRAY-DIMENSIONS`, `ARRAY-TOTAL-SIZE`,
`ARRAY-IN-BOUNDS-P`, `ADJUSTABLE-ARRAY-P`, `ARRAY-HAS-FILL-POINTER-P`,
`SIMPLE-VECTOR-P`. `MAKE-LIST` (`:initial-element`), `COPY-SEQ`.
`RANDOM` (integer and float limits, splitmix64 PRNG),
`MAKE-RANDOM-STATE`. `PROBE-FILE`, `DELETE-FILE`, `RENAME-FILE`.
`GET-UNIVERSAL-TIME`, `GET-INTERNAL-REAL-TIME`, `GET-INTERNAL-RUN-TIME`,
`INTERNAL-TIME-UNITS-PER-SECOND` (1 000 000 — microsecond resolution).
`FILL`, `REPLACE`, `MISMATCH` in `bootstrap/prelude.lisp`.
`CONCATENATE` extended to accept `VECTOR` / `SIMPLE-VECTOR` result-type.
Bug-fix: removed duplicate `WRITE-TO-STRING` definition that shadowed the
Slice-13 `&rest keys` version, causing `test_format` to hang.
`arithmetic.c` clean-up: added full prototype declarations in `arithmetic.h`
and eliminated two `-Wbad-function-cast` warnings.
Tests: [tests/test_misc.c](tests/test_misc.c) — 14 cases, all green.

**Slice 10 — FORMAT + I/O primitives + GC overhaul (v0.0.31)**:
`FORMAT` (directives `~A ~S ~% ~& ~~ ~D ~B ~O ~X ~R ~F ~E ~G ~I ~<…~>
~(…~) ~[…~]` and `~* ~? ~^`), `WRITE-STRING`, `WRITE-CHAR`,
`READ-FROM-STRING`, `WITH-OUTPUT-TO-STRING`, `PRINC-TO-STRING`,
`WRITE-TO-STRING`. Tests:
[tests/test_format.c](tests/test_format.c) — 19 cases, all green.

GC overhaul (driven by crashes and hangs uncovered when GC was
re-enabled under load from this slice's prelude):

- **Child pointer order fix** in `mark()`: `valid_block_start_p` must
  precede `marked_block_p` to avoid dereferencing an invalid pointer.
  Eliminated the SIGSEGV that occurred when GC ran during prelude load.
- **O(1) free-block insertion**: replaced the O(N) sorted-list insert
  with a simple prepend; total sweep time drops from O(N²) to O(N).
- **Coalescing sweep**: a single O(N) pass merges physically adjacent
  dead blocks into one larger free block, reducing fragmentation.
- **Segregated free lists** (`large_list` ≥ 128 B, `small_list` < 128 B)
  with largest-block-first promotion: O(1) allocation for all bocl
  objects (all ≤ 96 B, so every `large_list` entry satisfies any
  request).
- **FILE\* leak fix**: `bocl_read_cstring`, `bocl_read_eval_loop_cstring`,
  and `kernel_read_from_string` each created a `fmemopen`-backed stream
  and never closed it. After thousands of prelude reads, `_fwalk`
  (called by `getc` to flush all open FILE\* handles) became O(N) per
  character, making the test suite hang. Fixed by calling `stream_close`
  after each use.

#### Phase 5 progress after Slice 18

Between v0.0.42 and v1.4.30 a further 150+ slices landed. Rather than
one entry per slice, grouped by theme:

**v0.0.42 – v0.0.60 (Slices 19–35)**

- **Slice 19** — minimal CLOS: `DEFCLASS`, `DEFMETHOD`, `MAKE-INSTANCE`.
- **Slice 20** — function utilities, `DEFTYPE`, compound `TYPEP`.
- **Slice 21** — `DEFINE-CONDITION` + CLOS-based conditions.
- **Slices 22–33** — I/O built-ins; `MAKE-LIST` / `AREF-SET` / `RANDOM`;
  `FORMAT` extensions (`~^` / `~@D` / `~[…~]` / `~?` / `EVAL` /
  `MACROEXPAND`); string bounds/base; multi-seq predicates;
  `DO-HASH-TABLE`; `&AUX` / `PRINT-UNREADABLE-OBJECT`; compound
  `TYPEP` / `FORMAT ~F`; `APPLY` on a symbol / `COERCE` on a char;
  GC live-pool + `CHANGE-CLASS`; **logical pathnames** (Slice 33
  v0.0.57).
- **Slice 35** (v0.0.60) — GC-rooting audit + full condition system
  for user code + installable static + shared libs + `make install` /
  `make dist` layout.

**v0.0.61 – v0.9.x (Slices 36–164)**

- GC-rooting long-tail (Slices 36, 71, 76, 82, 96, 100, 117–120, 146,
  173).
- **CI Phases 0–4**: cached x86_64 base images, tcc matrix job, Alpine
  musl job, FreeBSD QEMU job.
- **True packages phase A/B/C** with **per-package symbol tables**
  (Slices 46–49, v0.0.71–75) — the "single global obarray" limitation
  from Phase 3 is gone.
- Fill-pointer / adjustable / displaced arrays (Slices 94–104).
- Multi-dim + bit arrays (Slices 100–102).
- Reader completeness: `READTABLE-CASE`, `|…|`, `#C`,
  `#\Newline` and named-char syntax, package-qualified symbol
  printing (Slice 57), `#nA` array literal (Slices 62–114).
- Restart machinery: `RESTART-BIND`, `COMPUTE-RESTARTS`,
  `HANDLER-CASE :no-error` (Slices 84–88).
- **Bignum engine** ([bootstrap/bignum.lisp](bootstrap/bignum.lisp))
  + kernel `Bignum` type (Slices 72–75, 122).
- ansi-test prereqs: `*QUERY-IO*`, `DEFINE-METHOD-COMBINATION` (stub),
  `FIND-METHOD` (Slice 159); `(SETF foo)` functions (Slice 161);
  `*LOAD-TRUENAME*` (Slice 163); `DEFGENERIC (:method …)` (Slice
  164); O(1) bucketed hash tables (Slice 166).

**v1.0.x (Slices 165–174)** — release + **generational nursery GC**
with write barrier + proactive default. Kernel becomes production-
grade in memory management.

**v1.1.x – v1.2.x (Slices 45–113)** — SACLA `must-*` suite driven
green. Print control vars, `*PRINT-CIRCLE*` `#n=`/`#n#`, print
case/gensym/base/level/length/radix (Slices 55–74); bignum-aware
bitwise (Slice 53); aggregate `#nA` printing; `*STOP-HOOK*` /
`*CONT-HOOK*` (Slice 54); minimal debugger (Slice 51); minimal
`COMPILE` (Slice 49); missing math + util functions (Slice 50);
`&KEY` validation. `must-character` green (75), `must-string` (77),
`must-do` (78), `must-eval` (79), `must-hash-table` (80),
`must-data-and-control` (82, 109); reader complete (85–101);
`SETF` eval-order (103); `MACRO-FUNCTION` env (105); condition/
restart ordering (108–112); **package-system ANSI conformance**
(Slice 113 → must-package 31 → 0, GREEN). Slice 114 finished the
debugger backtrace. **v1.3.0 = must-\* GREEN.**

**v1.3.x (Slices 115–125)** — ASDF + modules.

- `make install PREFIX=/opt/local` FHS layout (Slices 114–116).
- `--on-error` split: REPL debug, batch exit (Slice 118).
- Package `PRINT-OBJECT` (Slice 119).
- `SORT` segfault fix: iterative `%merge` (Slice 120).
- Control-stack overflow → catchable `STORAGE-CONDITION` (Slice
  121).
- Module system: `BOCL:*MODULE-LIST*` + manifest `REQUIRE` (Slice
  122).
- Module install plumbing + `modules/` (Slice 123).
- ASDF as git submodule [modules/asdf-src/](modules/asdf-src) (Slice
  124).
- UIOP Tier-1 portability (Slice 125).
- v1.3.14: structured pathnames + `DEFGENERIC` / `DEFMETHOD (setf
  name)` + `(setf DOCUMENTATION)`.
- v1.3.15: CLOS-aware `SUBTYPEP` / `TYPEP`.
- v1.3.16: computed `:initform` per-instance.
- v1.3.17: `:allocation :class` (shared class storage).
- **v1.4.0 = ASDF 3.3.7 loads and runs.**

**v1.4.x** — sockets, ASDF using, printer specials, CI hardening.

- Clang / gcc warning cleanup (v1.4.1–3).
- Callback stream + `DRIBBLE` (v1.4.4–6).
- `FORMAT ~D` mincol/padchar/comma-params on `~D`/`~B`/`~O`/`~X`/`~R`
  (v1.4.8).
- `--on-error` command-line-ordered (v1.4.9).
- `PARSE-NAMESTRING` (CLHS 19.2.2).
- `TYPEP` compound types with `:*` bounds + `FUNCTION` / `BOOLEAN`
  / `KEYWORD` / `ATOM` / `BIT` / `(un)signed-byte` / `mod`.
- `REINITIALIZE-INSTANCE` + `SHARED-INITIALIZE` fix.
- `LOOP ALWAYS` / `NEVER` / `THEREIS` timing fix (textual position).
- `MAKE-PATHNAME :DIRECTORY NIL` supplied-p vs defaults.
- `WITH-COMPILATION-UNIT` (CLHS 3.2.2.3).
- CLOS preserves multiple values through generic-function dispatch.
- Method's `&KEY` lambda-list accepts keywords named by sibling
  methods.
- BSD sockets kernel + `BOCL-SOCKET` package (v1.4.20–21) — macOS/
  BSD build fixes.
- ASDF USING-ASDF: `(asdf:load-system)` loads + runs a real system
  (v1.4.19).
- Install ships `bootstrap/socket.lisp` (v1.4.23).
- UIOP patch idempotent + self-healing (v1.4.22).
- CI: native macOS job on the thalassa shell runner.
- `*PRINT-CIRCLE*` SIGSEGV fix on strings/bit-vectors (v1.4.24).
- Wire test suites to the correct environment; honest `GITLAB_CI`
  guards (v1.4.25).
- Printer specials bound in kernel; standardize test operator names
  (v1.4.26).
- Category-3 behavioural failures in core suites; `LAST` type-check
  (v1.4.27).
- Run bocl-internal test suites in `COMMON-LISP` (v1.4.28).
- `TYPEP` restore `DEFTYPE` expansion (v1.4.29).
- Test cl-exports loader self-contained (v1.4.30).
- ci/tests: fail the build on test failure.

**Milestone summary at v1.4.30**: 25 special operators / most macros
/ ~90 % of CL functions / full numeric tower / true packages / full
CLOS core / condition system with restarts / full reader macros / all
`*PRINT-*` specials wired / logical pathnames / sockets / ASDF 3.3.7
loading / SACLA must-\* GREEN. See [CL-STATUS.md](CL-STATUS.md) for
the symbol-level view and [issues/open/](issues/open/) for the live
queue.

### Phase 6 — exhaustive C-level kernel test suite (**partial**)

Present under [tests/](tests/) (47 CUnit files linking `libbocl`):
`test_reader`, `test_eval`, `test_arithmetic`, `test_load`,
`test_packages`, `test_hash_table`, `test_conditions`,
`test_reader_macros`, `test_setf`, `test_control`, `test_sequences`,
`test_seq_lib`, `test_strings`, `test_numeric_extras`,
`test_destructuring`, `test_structures`, `test_handler_bind`,
`test_restarts`, `test_loop`, `test_list_utils`, `test_format`,
`test_macros`, `test_misc`, `test_clos`, `test_ci`, plus a
`test_slice{15,16,17,18,20,21,22,23,24,25,26,27,28,29,30,31,32,33}.c`
series scoped to individual slices.

Still to add per the original per-subsystem plan:

- `test_memory.c` — GC stress: ≫ zone-size allocation, force
  collect, root survival, live-pool overflow, nursery vs old-gen
  promotion.
- `test_kernel_objects.c` — `class-of` per type; `SUBCLASSP`
  transitivity.
- `test_cons.c` — accessors on dotted / proper / circular lists.
- `test_symbol.c` — intern identity, plist, value/function cells,
  `MAKUNBOUND`, `FBOUNDP`.
- `test_integer.c`, `test_float.c` — arithmetic edges + bignum /
  ratio / complex promotion.
- `test_character.c`, `test_string.c`, `test_vector.c`,
  `test_bit_vector.c`.
- `test_structure.c` — define / accessors / inheritance.
- `test_printer.c` — `(read (print x))` round-trip.
- `test_stream.c` — string + file + broadcast / concatenated / two-
  way streams, EOF.
- `test_filename.c` — pathname parsing + logical pathname
  translation.
- `test_environment.c` — lexical / dynamic / non-local exits.
- `test_built_in.c` — every entry in
  [sources/built_in_functions.h](sources/built_in_functions.h)
  called via `bocl_call`, results checked.

`make test` runs the suite; CI now fails on any test failure
(v1.4.30). Combined JUnit XML report not yet emitted.

### Phase 7 — sanitiser + portability (**partial**)

Done:
- ASan + UBSan gated behind `SANITIZE=yes` in
  [sources/common.mk](sources/common.mk); dedicated CI job
  `sanitise:debian-gcc` in [.gitlab-ci.yml](.gitlab-ci.yml).
- Build matrix: Debian gcc / clang / tcc, Alpine musl, FreeBSD (via
  QEMU), macOS (thalassa shell runner).

Still to do:
- Valgrind-clean Linux job.
- OpenBSD in the matrix.
- Explicit "nightly" cadence for the sanitiser job.

### Effort estimate

The original 10–12 week estimate has been consumed. Remaining work is
now concentrated in three areas: ansi-test integration (§3), the last
~120 CL symbols in [CL-STATUS.md](CL-STATUS.md), and the 22 open
issues in [issues/open/](issues/open/).

---

## 3. ansi-test integration (**not yet started at v1.4.30**)

Phase 5 is done — the "do not point bocl at ansi-test until Phase 5"
caveat below no longer holds. This is the next quality gate. Tracked
in [ansi-test.issue](issues/open/ansi-test.issue). On master today
there is no `ansi-test/` submodule, no `tests/ansi-test-harness/`, no
`make ansi-test` target.

Use Paul Dietz' ansi-test (https://gitlab.common-lisp.net/ansi-test/ansi-test) — `deftest`/`regression-test` framework, organised by CLHS section.

### Layout

```
bocl/
  ansi-test/                       # git submodule, pinned revision
  tests/ansi-test-harness/
    run-ansi-test.lisp             # loads suite, applies known-fail map
    expected-failures.lisp         # symbol → reason
    run.sh                         # invokes ./sources/bocl with the harness
```

### Driver

```sh
./sources/bocl --batch --quiet --no-init \
  --load tests/ansi-test-harness/run-ansi-test.lisp \
  --eval '(bocl-ansi-test:run-all :report "tests/ansi-test.xml")' \
  --quit
```

`run-ansi-test.lisp`:

1. Push `ansi-test/` onto `*default-pathname-defaults*`.
2. `(load "ansi-test/init.lsp")`.
3. Suite loads its own section files.
4. Read `expected-failures.lisp` and mark those `deftest` names as `:skip` / `:expected-failure`; emit `unexpected-pass` / `unexpected-fail` separately.
5. Write JUnit XML to the given path.

### Phased enablement

Match the kernel phases:

1. `data-and-control-flow.lsp`, `symbols.lsp`, `conses.lsp` — after Phase 1+2.
2. `numbers.lsp`, `characters.lsp`, `strings.lsp` — after Phase 2.
3. `packages.lsp`, `pathnames.lsp`, `streams.lsp`, `files.lsp` — after Phase 3.
4. `hash-tables.lsp`, `conditions.lsp`, `printer.lsp`, `reader.lsp` — after Phase 4.
5. `iteration.lsp`, `sequences.lsp`, `evaluation-and-compilation.lsp`, `environment.lsp` — after Phase 5.
6. `clos.lsp` — last.

Phase 5 has now landed; all six enablement steps can be attempted at once. Any section still failing at first-run enablement goes in `expected-failures.lisp`. CI gate: pass count only goes up.

### Reporting

- `bocl-ansi-test:run-all` writes JUnit XML.
- Root `Makefile` gains a `make ansi-test` target.
- GitLab pipeline adds an `ansi-test` stage uploading the XML as a job artifact.

### Caveats

- Phase 5 (reader, packages, CLOS, conditions, format) is done as of v1.4.30, so the harness can now be brought up.
- The project's own license to exploit "undefined" or "implementation-defined" behaviour (per [README.org](README.org)) will produce justified mismatches; those go in `expected-failures.lisp` with a comment, not silently ignored.
