# bocl — Remaining Work (post-v0.0.56)

## Completed (in working tree, to be committed)

- [x] **GC live-pool bloat fix** — `bocl_read_eval_cstring` and `bocl_load_cstring`
  now wrap each evaluation with `open_live_pool`/`close_live_pool`, preventing
  unlimited accumulation of intermediate objects in the GC live pool.

- [x] **`LOOP FOR k BEING EACH HASH-KEY/HASH-VALUE OF ht`** — BEING sub-clause
  added to the LOOP macro's FOR-AS handler in `bootstrap/prelude.lisp`.

- [x] **`FORMAT ~I`** (indent) and **`FORMAT ~N`** (conditional newline)
  — new `case 'I':` and `case 'N':` in printer.c.

- [x] **`*PRINT-LENGTH*` enforcement** — `prin1` now truncates list output
  with `...` when `*print-length*` is a non-negative integer.

- [x] **`PRINT-OBJECT` safe printing for CLOS instances** — `prin1` and `princ`
  now check `clos_instance_p()` and emit `#<CLASSNAME addr>` using
  `structure_get` instead of crashing via `class_name(clos_cls)`.

- [x] **`CHANGE-CLASS`** and **`UPDATE-INSTANCE-FOR-DIFFERENT-CLASS`** — minimal
  implementation that re-tags the instance and carries slot values forward.

- [x] **CLOS `:before` / `:after` / `:around` method qualifiers** — already
  implemented in the existing `%apply-methods-standard` dispatch (verified).

## Completed (Slice 33 — v0.0.57)

- [x] **Logical pathnames** — full conforming implementation:
  `LOGICAL-PATHNAME`, `LOGICAL-PATHNAME-P`, `LOGICAL-PATHNAME-TRANSLATIONS`,
  `SET-LOGICAL-PATHNAME-TRANSLATIONS`, `TRANSLATE-LOGICAL-PATHNAME`,
  `LOAD-LOGICAL-PATHNAME-TRANSLATIONS`, `PATHNAME-MATCH-P`, `WILD-PATHNAME-P`;
  `PATHNAME-HOST`, `PATHNAME-DEVICE` (Windows C: detection), `PATHNAME-VERSION`;
  updated `PATHNAME`, `NAMESTRING`, `MAKE-PATHNAME`, `MERGE-PATHNAMES`,
  `ENOUGH-NAMESTRING`, `PATHNAMEP`.
  Also fixed: `TYPEP` for structure classes (was returning NIL for non-CLOS
  structure instances due to `(t nil)` fallback instead of
  `(t (funcall *%kernel-typep* object type))`).

## Still Remaining

### Low Priority

- [ ] **Full `PRINT-OBJECT` auto-dispatch** — currently `prin1`/`princ` emit
  `#<CLASSNAME addr>` for all CLOS instances; to call a user-defined
  `print-object` method needs a guard for "no applicable method" to avoid
  recursion.  Deferred until the condition system can catch the error.

- [ ] **`UPDATE-INSTANCE-FOR-REDEFINED-CLASS`** — wire into the class-redefinition
  path when `DEFCLASS` is re-evaluated on an existing class.

- [ ] **`DEFINE-METHOD-COMBINATION`** full support — stubs exist; implement
  custom combination types (currently only `standard` combination works).

- [ ] **Logical pathname stubs** — `LOGICAL-PATHNAME-TRANSLATIONS`,
  `SET-LOGICAL-PATHNAME-TRANSLATIONS` currently signal errors; should return
  empty tables.

- [ ] **`RATIO` / complex numbers** — only integers and floats exist;
  `RATIONAL`/`RATIONALIZE` return the float as-is.  Full ratio support would
  require a new kernel type.

- [ ] **ansi-test integration** — see PLAN.md §3.  Add submodule, harness,
  and `make ansi-test` target.  Gate: pass count only goes up.
