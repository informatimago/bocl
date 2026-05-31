# bocl — Remaining Work (post-Slice-31, v0.0.55)

## High Priority

- [ ] **GC live-pool bloat fix** (`sources/bocl.c`)
  `bocl_read_eval_cstring` and `bocl_load_cstring` don't call
  `open_live_pool`/`close_live_pool`, so every evaluation permanently
  pins intermediate objects. Tests balloon to GB+ RSS as slices pile up.
  Fix: wrap `bocl_eval` calls with pool save/restore, add result to outer pool.

- [ ] **`LOOP FOR k BEING EACH HASH-KEY OF ht`** (`bootstrap/prelude.lisp`)
  Extend the LOOP macro's FOR-AS clause to recognise the BEING sub-clause.
  Workaround `loop-for-hash` exists; now add native LOOP syntax.

## Medium Priority

- [ ] **`FORMAT ~I`** indent directive (`sources/printer.c`)
  Emit spaces to reach column N. One new `case 'I':` in printer.c.

- [ ] **`FORMAT ~N`** conditional newline (`sources/printer.c`)
  Emit newline only when not at column 0. One new `case 'N':` in printer.c.

- [ ] **`*PRINT-LEVEL*` / `*PRINT-LENGTH*` enforcement** (`sources/printer.c`)
  Variables already exist; add depth/length truncation to `prin1` when non-NIL.

- [ ] **CLOS `:before` / `:after` / `:around` method qualifiers**
  Primary dispatch works; qualifiers are parsed but not called in the
  right order. Fix the method dispatch chain in `bootstrap/prelude.lisp`.

- [ ] **`PRINT-OBJECT` dispatch for user CLOS instances**
  Currently falls through to `#<CLASS address>`. Wire `print-object`
  generic to be called from `prin1` for objects that have a method.

- [ ] **`CHANGE-CLASS`** (`bootstrap/prelude.lisp`)
  Add a minimal `(change-class instance new-class &rest initargs)` that
  re-tags the instance with the new class and runs `update-instance-for-different-class`.

## Low Priority

- [ ] **`UPDATE-INSTANCE-FOR-REDEFINED-CLASS`**
  Stub exists; wire it into the class-redefinition path.

- [ ] **`DEFINE-METHOD-COMBINATION`** full support
  Stubs exist; implement custom combination types.

- [ ] **Logical pathname stubs** — `LOGICAL-PATHNAME-TRANSLATIONS`, `SET-LOGICAL-PATHNAME-TRANSLATIONS`
  Currently error; return empty tables instead.

- [ ] **`RATIO` / complex numbers**
  Only integers and floats exist. `RATIONAL`/`RATIONALIZE` return the float as-is.
  Full ratio support would require a new kernel type.

- [ ] **ansi-test integration**
  See PLAN.md §3. Add the git submodule, harness, and `make ansi-test` target.
  Gate: pass count only goes up.
