# bocl completion plan

Companion to [README.org](README.org) and [design/design.org](design/design.org). This file is the operational plan for turning bocl into a fully conforming Common Lisp implementation validated against ansi-test.

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

### Phase 2 — numbers + arithmetic in the kernel (1 week)

Implement in [sources/integer.c](sources/integer.c) / [sources/float.c](sources/float.c) and register via the generator:

- `+ - * /` (2-arg first; n-ary form in Lisp library)
- `= /= < > <= >= ZEROP PLUSP MINUSP ODDP EVENP`
- `1+ 1- ABS MIN MAX MOD REM TRUNCATE FLOOR CEILING ROUND`
- bitwise: `LOGAND LOGOR LOGXOR LOGNOT ASH INTEGER-LENGTH`
- coercion: `COERCE`, `FLOAT`, `INTEGER`

Defer ratios / complex / bignum to [bootstrap/bignum.lisp](bootstrap/bignum.lisp).

Tests: per-op edge cases, overflow, mixed int/float promotion.

### Phase 3 — file load + packages (1 week)

- File-stream open/read/close in [sources/stream.c](sources/stream.c). Buffered char-by-char read suffices.
- Rewrite `bocl_load_cstring` ([sources/bocl.c:58](sources/bocl.c:58)): open → loop `read` then `eval` → close. Honour `*LOAD-VERBOSE*` / `*LOAD-PRINT*`.
- Minimal package system in the kernel: global registry, internal/external symbol tables per package, `INTERN`, `FIND-PACKAGE`, `IN-PACKAGE`, `EXPORT`, `IMPORT`, `USE-PACKAGE`, `*PACKAGE*`. Full semantics (shadowing, name-conflict restarts) move to the Lisp side.

Tests: load a known file and verify side effects; intern + symbol identity across packages.

### Phase 4 — hash tables, conditions, full reader (2 weeks)

- Hash tables as alists internally (per design), exposing `MAKE-HASH-TABLE GETHASH (SETF GETHASH) REMHASH MAPHASH HASH-TABLE-COUNT HASH-TABLE-TEST CLRHASH`.
- Condition system: `SIGNAL ERROR CERROR WARN HANDLER-CASE HANDLER-BIND RESTART-CASE RESTART-BIND FIND-RESTART INVOKE-RESTART`. Reuse `frame_handler`.
- Reader: `#=`, `#.`, `#+`/`#-`, `set-macro-character`, `set-dispatch-macro-character`, case modes.

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
