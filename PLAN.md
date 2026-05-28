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

## 1. Audit (current state, 2026-05-24)

bocl follows a two-layer bootstrap: a small **C kernel** plus a **Lisp library** under [bootstrap/](bootstrap/) loaded on top of it. Per the project's own design notes (README.org, design/design.org, bootstrap/TODO), the kernel is intentionally minimal — just enough to LOAD the Lisp side.

### Working in the kernel

| Subsystem | Files | Notes |
|---|---|---|
| Mark-sweep GC, multi-zone heap, live-pool roots | [sources/memory.c](sources/memory.c) | Solid |
| Tagged Refs, 62 declared class objects | [sources/kernel_types.h](sources/kernel_types.h), [sources/kernel_objects.c](sources/kernel_objects.c) | Type tags only; no CLOS metaclass behaviour |
| Cons / list ops | [sources/cons.c](sources/cons.c) | ~90 % |
| Symbols (intern, value, plist, fboundp) | [sources/kernel.c](sources/kernel.c) | ~85 %, single obarray, no real packages |
| Characters, strings, vectors, bit-vectors | [sources/character.c](sources/character.c), [sources/bostring.c](sources/bostring.c), [sources/vector.c](sources/vector.c), [sources/bit_vector.c](sources/bit_vector.c) | 50–70 % |
| Structures (DEFSTRUCT runtime) | [sources/structure.c](sources/structure.c), [sources/structure_functions.c](sources/structure_functions.c), [sources/kernel_structure_functions.c](sources/kernel_structure_functions.c) | ~85 % — best covered chapter |
| Reader | [sources/reader.c](sources/reader.c) | ~70 %; no reader macros, no `#=`/`#.` |
| Printer | [sources/printer.c](sources/printer.c) | ~60 %; no `*print-*` control vars wired up |
| Streams (string + fd) | [sources/stream.c](sources/stream.c) | ~50 % |
| Pathnames | [sources/filename.c](sources/filename.c) | ~60 % |
| Environments + frames | [sources/environment.c](sources/environment.c), [sources/eval.c](sources/eval.c) | Frame model in place |
| Special operators in `evale` | [sources/eval.c](sources/eval.c) | QUOTE IF PROGN LOCALLY LET PROGV M-V-CALL M-V-PROG1 THE EVAL-WHEN UNWIND-PROTECT BLOCK RETURN-FROM CATCH THROW TAGBODY GO — but call path broken (see below) |
| 197 generated `kernel_*` built-ins | [sources/built_in_functions.c](sources/built_in_functions.c) | Wrappers compile |

### Missing or broken in the kernel

| Gap | Where | Severity |
|---|---|---|
| `bocl_eval` is a no-op stub | [sources/bocl.c:14](sources/bocl.c:14) | **Blocker** |
| `kernel_apply_function`, `kernel_apply_closure` are `NOT_IMPLEMENTED_YET` | [sources/kernel.c](sources/kernel.c) ~1555/1561 | **Blocker** — no user-defined fn can be called |
| `evale` dispatches through a placeholder `static Ref f(...){return NULL;}` | [sources/eval.c:232](sources/eval.c:232) | **Blocker** |
| `SETQ`, `LABELS`, `FLET` (real), `MACROLET`, `SYMBOL-MACROLET` | [sources/eval.c](sources/eval.c) | Blocker / required |
| `bocl_load_cstring` returns early; `STREAM-OPEN` not done | [sources/bocl.c:58](sources/bocl.c:58) | Blocker for `LOAD` |
| Packages: `*PACKAGE*`, `INTERN` in a named pkg, `IN-PACKAGE`, `EXPORT`, `USE-PACKAGE` | kernel symbol table, [sources/bocl.c](sources/bocl.c) | Blocker |
| Arithmetic: `+ - * / = < > MOD REM TRUNCATE FLOOR ABS EXPT …` | absent | Blocker |
| Hash tables (class declared, no impl) | — | Required |
| Condition system, `HANDLER-CASE`, restarts (frames declared, body stubbed) | [sources/eval.c:62](sources/eval.c:62) | Required |
| CLOS / MOP (only class scaffolding) | — | Required |
| FORMAT, LOOP, sequence functions | expected on Lisp side | Required |
| Multi-dim arrays, `:initial-offset`, displaced | [sources/kernel.c:1012](sources/kernel.c) | Required |
| Reader macros (`set-macro-character`, `#=`, `#.`, `#+`/`#-`) | [sources/reader.c](sources/reader.c) | Required for ansi-test corpus |
| Numeric tower (ratios, complex, bignum) | bignum present as Lisp draft only | Required |

### Lisp side

[bootstrap/](bootstrap/) contains drafts (`minimal-compiler.lisp`, `tiny-eval.lisp`, `tiny-reader.lisp`, `tiny-printer.lisp`, `tiny-repl.lisp`, `bootstrap-packages.lisp`, `bootstrap-cl-package.lisp`, `operators.lisp`, `bignum.lisp`). None is loaded today because `LOAD` and `EVAL` are stubbed.

### Tests

Only [tests/test_reader.c](tests/test_reader.c) is active (CUnit, links libbocl). No kernel-eval, no built-in, no GC tests.

### Overall

≈ 30–40 % of the bootstrap target. The two single-points-of-failure are `bocl_eval` and `kernel_apply_function`; fix those and most of the rest is breadth, not design.

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

### Phase 6 — exhaustive C-level kernel test suite

Independent of ansi-test, a CUnit suite per kernel subsystem under `tests/`:

- `test_memory.c` — GC stress: ≫ zone-size allocation, force collect, root survival, live-pool overflow.
- `test_kernel_objects.c` — `class-of` per type; `SUBCLASSP` transitivity.
- `test_cons.c` — accessors on dotted / proper / circular lists.
- `test_symbol.c` — intern identity, plist, value/function cells, makunbound, fboundp.
- `test_integer.c`, `test_float.c` — arithmetic edges (Phase 2).
- `test_character.c`, `test_string.c`, `test_vector.c`, `test_bit_vector.c`.
- `test_structure.c` — define / accessors / inheritance.
- `test_reader.c` — extend existing.
- `test_printer.c` — `(read (print x))` round-trip.
- `test_stream.c` — string + file streams, EOF.
- `test_filename.c` — pathname parsing.
- `test_environment.c` — lexical / dynamic / non-local exits.
- `test_eval.c` — every special operator, multiple values, tail position.
- `test_built_in.c` — every entry in [sources/built_in_functions.h](sources/built_in_functions.h) called via `bocl_call`, results checked.

`make test` runs them all with a combined JUnit XML report; failures fail CI.

### Phase 7 — sanitiser + portability

- ASan + UBSan run nightly.
- Build matrix Linux (gcc, clang), macOS, OpenBSD; POSIX + C99 surface only.
- Valgrind clean on Linux.

### Effort estimate

≈ **10–12 weeks** of focused work. Phases 1–3 are gating, 4–5 are bulk, 6–7 are quality.

---

## 3. ansi-test integration

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

Until Phase 5 lands, every section is in `expected-failures.lisp`. CI gate: pass count only goes up.

### Reporting

- `bocl-ansi-test:run-all` writes JUnit XML.
- Root `Makefile` gains a `make ansi-test` target.
- GitLab pipeline adds an `ansi-test` stage uploading the XML as a job artifact.

### Caveats

- ansi-test needs reader, packages, CLOS, conditions, format — do not point bocl at it until Phase 5.
- The project's own license to exploit "undefined" or "implementation-defined" behaviour (per [README.org](README.org)) will produce justified mismatches; those go in `expected-failures.lisp` with a comment, not silently ignored.
