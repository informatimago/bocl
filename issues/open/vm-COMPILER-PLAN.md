# plan-2 byte-code compiler — implementation plan & handoff (vm-3 .. vm-9)

READ THIS FIRST when continuing the byte-code VM work. It captures the design
decisions already made, the conventions, the file map, the gotchas, and a
concrete per-phase plan so the remaining compiler can be built without re-deriving
everything. Work stays on the **`vm`** branch.

---

## 0. Status (as of v1.8.1)

DONE on `vm` (both tested, full suite green — commits v1.8.0, v1.8.1):
- **vm-1** — the ISA spec + the compiled-function runtime object. See
  `sources/bytecode.h` (THE authoritative ISA: opcode enum, operand encodings,
  full stack-effect table, BC_ARITY_PACK) and `sources/bytecode.c` +
  `kernel_types.h` (CompiledFunction/CompiledClosure structs).
- **vm-2** — the VM dispatch core: `bytecode_apply()` in `sources/bytecode.c`
  runs the non-control opcodes. Operand stack + locals are GC-rooted
  simple-vectors; portable C17 `switch` loop; CALL/CALL-NAMED go through
  `kernel_apply` (uniform interop). Verified by `tests/test_misc.c::test_vm_execute`
  (hand-assembled `(+ x 1)`, `(if x 1 2)`, `(car l)`, a backward-JUMP reverse loop).

Already on `master` (context): cfasl-1..5 (compact binary fasl, `sources/cfasl.*`),
hot-2/hot-3 (v1.7.x). Key finding: **bocl eval is CPU-per-operation bound, not
allocation bound** (GC ~2% prelude / ~12% loop code) — the VM is the real lever.

- **vm-3** — the compiler front-end / lexical analyzer (v1.8.2). Lisp module
  `bootstrap/vm-compiler.lisp` (loaded at end of prelude); `%VM-ANALYZE` walks a
  `(LAMBDA ...)` into the §4 IR with every reference resolved to a `(depth index)`
  flat-locals address; expands global + local macros; marks captures; the new
  `%GLOBALLY-SPECIAL-P` kernel prim (`sources/bocl.c`) is the special-var predicate.
  Verified by `tests/test_misc.c::test_vm_analyze`.

- **vm-4** — codegen core (v1.8.3). `%VM-COMPILE` lowers the analyzer IR to the
  vm-2 ISA and boxes it via the new `%MAKE-COMPILED-FUNCTION` prim; runs on the VM.
  Emitter + EQL constant pool + stack-depth tracking; single-pass assembler with
  FIXED 5-octet jump displacements (code is an octet STRING — §3.4 resolved). New
  prims `%MAKE-COMPILED-FUNCTION` / `%BYTECODE-OPCODES` (opcode numbers straight
  from the C enum). Verified by `test_misc.c::test_vm_codegen` (differential vs the
  tree-walker). No TCO yet (CALL+RETURN); closures/control/&key/&aux/special-LET
  error clearly; COMPILE/COMPILE-FILE not rewired (vm-8).

REMAINING: vm-5 (codegen control) →
vm-6 (closures + interop) → vm-7 (CLOS inline cache) → vm-8 (byte-code fasl) →
vm-9 (self-host prelude + validate). Issues in `issues/open/vm-*.issue`.

**vm-5 starts here:** the analyzer already emits the control IR nodes (`:block`,
`:return-from`, `:tagbody`, `:go`, `:catch`, `:throw`, `:unwind-protect`,
`:mv-call`, `:mv-prog1`, `:progv`, `:load-time-value`) and dynamic-bind LET
entries (`(:special name init)`); `%VM-GEN` currently errors on all of them.
Implement their codegen + the VM opcode handlers (BLOCK_OPEN/RETURN_FROM/
CATCH_OPEN/THROW/UNWIND_PROTECT/UNWIND_LEAVE/MV_*), mirroring eval.c's Frame /
setjmp machinery, plus dynamic (special) binding save/restore. Also lift the
&optional-default / supplied-p / &key / &aux restriction from
`%VM-CHECK-CORE-LAMBDA-LIST` by emitting a prologue.

---

## 1. Conventions (MANDATORY — same as the rest of the project)

- **Branch**: `vm` (off master). Push each completed issue: `git push gitlab vm`.
- **Version**: bump `BOCL_VERSION_PATCH` in `sources/version.h` on EVERY commit that
  touches code/tests/docs/bootstrap; put the version in the commit subject. Current
  = 1.8.1 → next code commit is v1.8.2. (Issue-only commits do NOT bump.)
- **Commit trailer** (verbatim, last line):
  `Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>`
- **Build**: `cd sources && make build`  (warning-clean, strict C17).
  Profiled build: `make clean && make build CPPFLAGS=-DBOCL_PROFILE` (then
  `(bocl:profile-reset) ... (bocl:profile-report)`; remember to rebuild normal after).
- **Test one suite**: `cd tests && make test_misc && LD_LIBRARY_PATH=../sources ./test_misc`
  (look for `failure count = 0`).
- **Full suite** (gate EVERY issue — the evaluator/VM is core): run in background,
  `cd tests && make test`; verify no `failure count = [1-9]` and 42 `# Testing` lines.
- Do NOT disturb the user's working copy at `/home/claude/src/public/bocl` (master is
  checked out there; update remote master with `git push gitlab <branch>:master`, never
  a local `checkout master` here).
- Temp files under `$CLAUDE_JOB_DIR/tmp`.

---

## 2. The ISA (summary — `sources/bytecode.h` is authoritative)

Stack VM, opcode = 1 byte + inline operands. Operand encodings:
- `u8` — one raw byte (small counts: CALL n, DROP n, VALUES n).
- `uvar` — LEB128 unsigned varint (const/symbol/local/template index, local depth).
- `svar` — zig-zag LEB128 signed varint — a jump displacement RELATIVE to the byte
  after the full instruction. (`vm_read_uvar`/`vm_read_svar` in bytecode.c.)

Opcodes (enum `bytecode_op_t`, values 0..30, APPEND-ONLY): PUSH_CONST, PUSH_NIL,
PUSH_T, DUP, POP, DROP, LOAD_LOCAL d i, STORE_LOCAL d i, LOAD_GLOBAL s, STORE_GLOBAL s,
LOAD_FUNCTION s, JUMP o, JUMP_IF_NIL o, JUMP_IF_NOT_NIL o, CALL n, CALL_NAMED s n,
TAIL_CALL n, RETURN, VALUES n, MV_CALL, MV_BIND n, BLOCK_OPEN k, RETURN_FROM k,
CATCH_OPEN, THROW, UNWIND_PROTECT, UNWIND_LEAVE, MAKE_CLOSURE t, LOAD_UPVAL i, NOP, HALT.

Implemented in vm-2: PUSH_CONST/NIL/T, DUP/POP/DROP, LOAD/STORE_LOCAL (depth 0 only),
LOAD/STORE_GLOBAL, LOAD_FUNCTION, JUMP/JUMP_IF_*, CALL/CALL_NAMED, VALUES (primary
only), RETURN, NOP, HALT. Everything else errors "control/closure ops are vm-5/6".

Compiled-function object (`compiled_function_new(name, lambda_list, code, constants,
arity, max_stack, n_locals, source)`): `code` is the byte string, `constants` a
simple-vector (constant pool), `arity` = `BC_ARITY_PACK(req,opt,rest,key,aok)` fixnum,
`max_stack`/`n_locals` fixnums, `source` the source lambda expr (for FLE/disassemble).
Accessors `compiled_function_*` in bytecode.c. `compiled_closure_new(fn, upvalues)`.

---

## 3. Design decisions LOCKED IN (do not relitigate)

1. **Flat-locals model.** All of a function's lexical variables — parameters AND
   every LET/LET*/nested binding — get distinct slot indices in ONE frame (the
   function's locals vector). So `LOAD-LOCAL depth 0 idx` addresses them all; a
   nested LET does NOT create a new VM frame, it consumes more slots. `depth>0`
   ONLY crosses an enclosing lambda boundary (closure capture, vm-6). This matches
   vm-2's single-locals-vector-per-call VM and is the analyzer's slot model (§5).
   `n_locals` = the max slot count the function reaches.

2. **Value representation** = the existing tagged `Ref`, unchanged. VM stack /
   locals / constants all hold Refs; zero conversion with the tree-walker.

3. **Uniform call path (for now).** CALL/CALL-NAMED build a Lisp arg-list and go
   through `kernel_apply` (one C frame per Lisp call, like the tree-walker), so
   byte-code / built-in / tree-walked callees all interop immediately. vm-6 adds a
   stack-based fast path AND the no-C-recursion frame trampoline + VM stack-overflow
   signal (BOCL:OUT-OF-STACK-MEMORY-ERROR). Don't block vm-3/4/5 on that.

4. **Code representation.** vm-2 reads `code` as a STRING of octets (bocl strings
   are 1 octet/char). bytecode.h nominally says `(unsigned-byte 8)` vector.
   RECONCILE in vm-4: either have the Lisp compiler emit an (unsigned-byte 8)
   simple-array AND extend the VM's `code` reader to accept it (keep string), or
   have the compiler emit a string. Simplest: compiler builds an adjustable
   (unsigned-byte 8) vector, `coerce`/copy to a simple one, and teach the VM a
   `vm_code_octets(code, &len)` helper handling both. Low-risk either way.

5. **Compiler lives in Lisp**, in a NEW file `bootstrap/vm-compiler.lisp` loaded at
   the END of `bootstrap/prelude.lisp` (mirror the existing socket.lisp loader at
   the very end of the prelude — same `*load-truename*`-relative pattern). Keeping
   it a separate module keeps the prelude readable and lets vm-9 self-compile it.
   Package: put helpers in COMMON-LISP as internal `%vm-...` symbols (prelude runs
   in CL), export the user entry points (COMPILE / DISASSEMBLE already exist — vm-8
   makes COMPILE produce byte-code).

6. **GC rooting rule (learned the hard way in cfasl):** `Object_new` (kernel.c:70)
   opens a live-pool frame, roots its inputs, allocates, then BALANCES with
   `close_live_pool` — so a freshly-built object is NOT left rooted; the caller must
   `add_to_live_pool` it. Any C that builds a graph across further allocations must
   root intermediates. The VM sidesteps this by parking Refs in GC-traced
   simple-vectors (stack/locals). Storing a Ref into a heap vector needs
   `gc_write_barrier(vector->slots->array.elements)` (see svref_set / the VM PUSH).

---

## 4. IR (the analyzer's output; codegen input) — proposed, refine in vm-3

An IR node is a list tagged by a keyword. The analyzer (vm-3) emits it; codegen
(vm-4/5) walks it. Proposed nodes:
```
(:const  value)                     ; self-eval / quoted literal -> PUSH-CONST (pool it)
(:lref   depth index)               ; lexical var  -> LOAD-LOCAL depth index
(:gref   symbol)                    ; special/global/free var -> LOAD-GLOBAL
(:lset   depth index value-ir)      ; -> value; STORE-LOCAL depth index
(:gset   symbol value-ir)           ; -> value; STORE-GLOBAL
(:if     test-ir then-ir else-ir)   ; -> JUMP-IF-NIL/JUMP
(:progn  ir ...)                    ; POP between, last value kept ({} -> PUSH-NIL)
(:call   fn-ir arg-ir ...)          ; general call -> push fn, push args, CALL n
(:call-named symbol arg-ir ...)     ; global fn -> push args, CALL-NAMED s n
(:function symbol)                  ; (function foo) global -> LOAD-FUNCTION
(:lambda arity nlocals maxstack body-ir source)  ; nested fn -> a compiled template
(:let    ((slot . init-ir) ...) body-ir)         ; parallel bind then body
(:the    ir)                        ; type ignored -> passthrough
;; control (vm-5): (:block name body) (:return-from name ir) (:tagbody ...) (:go tag)
;;                 (:catch tag body) (:throw tag ir) (:unwind-protect body cleanup)
;;                 (:mv-call fn producers) (:progv ...) (:load-time-value ...)
```
Alternative accepted by the issue: skip a separate IR and have the analyzer hand
addresses straight to a single-pass codegen. Either is fine; a small IR is easier
to unit-test (vm-3 acceptance wants the analyzer tested independently).

---

## 5. vm-3 — compiler front-end: MACROEXPAND-ALL + lexical analysis

Deliverable: Lisp analyzer producing the §4 IR (addresses resolved), unit-tested.

- **Front**: `(macroexpand-all form env)` already exists (prelude ~9455) and
  handles MACROLET/SYMBOL-MACROLET; run it so the analyzer sees only special
  operators + calls.
- **Compilation environment** (flat-locals, §3.1). Suggested representation — a list
  of FUNCTION LEVELS (innermost first); each level a mutable 2-slot object
  `#(next-slot scopes)`: `next-slot` grows across LETs in that function; `scopes` is
  a stack of lexical scopes, each an alist `((name slot special-p) ...)`. Helpers:
  new-level, push/pop-scope, bind (allocate a slot in the current scope, return it),
  resolve (walk scopes inner→outer; count FUNCTION levels crossed = depth; return
  `(depth slot special-p)` or NIL=free/global).
- **SPECIAL detection**: a var is special if globally proclaimed OR declared special
  in an enclosing scope. You will likely need a Lisp-visible "is NAME globally
  special?" predicate — check first for an existing one; the C side is
  `special_variable_p` (sources/variable.c:34, keyed on the symbol plist `$SPECIAL`).
  If none is exposed, add a tiny kernel primitive `%GLOBALLY-SPECIAL-P` (mirror the
  `%CFASL-*` registration in bocl.c ~line 1970s). Special vars resolve to `(:gref)`
  (dynamic path), never a lexical address; a LET of a special is a dynamic bind
  (that codegen/VM support is vm-5's PROGV-like path — for vm-3 just mark it).
- **Analyze**: symbol→(lref|gref); self-eval atom/quote→(:const); if/progn/setq/
  let/let*/function/lambda/the/call per §4. Nested `(lambda ...)` pushes a new
  FUNCTION level; captured outer vars get depth>0 (record them for vm-6 closure
  conversion). FLET/LABELS add function-name→slot entries (a parallel function
  namespace); LABELS bodies see themselves. Control forms → emit the (:block ...)
  etc. nodes (leave codegen to vm-5) so the analyzer is complete.
- **ACCEPTANCE**: analyze nested LET, a closure over an outer var, FLET recursion, a
  SPECIAL declaration; dump correct addresses. Unit-test independently of codegen
  (call the analyzer from C via `e("(%vm-analyze '(lambda ...) ...)")` in test_misc,
  or Lisp tests). Add a `%vm-disassemble-ir`/pretty-printer to eyeball output.

## 6. vm-4 — codegen core

Deliverable: Lisp codegen for the analyzer's IR → a CompiledFunction. Compile
atoms/calls/IF/PROGN/LET/LET*/SETQ/QUOTE/FUNCTION/THE/LAMBDA. Then `(compile nil
'(lambda ...))` returns a runnable byte-code function.

- An **emitter**: a growable (unsigned-byte 8) buffer + `emit-op`, `emit-uvar`,
  `emit-svar`, plus a **constant pool** builder (a table object→index; PUSH-CONST /
  CALL-NAMED / LOAD-GLOBAL indices come from it). Track `max-stack` (simulate the
  stack effect while emitting) and `n-locals` (from the analyzer's slot count).
- **Jumps/labels**: emit with a placeholder, patch the svar after the target is
  known. Since svar is variable-length, either reserve a fixed 2-3 byte slot and
  pad, or do a two-pass fixpoint. Simplest first cut: emit a fixed-width (e.g.
  always encode jump offsets in a fixed 3-byte little-endian signed field with a
  distinct FIXED-JUMP encoding) OR two-pass. Recommend two-pass label resolution
  (compute positions, then encode). Keep it simple; correctness over density.
- Assemble via `compiled_function_new` (expose it to Lisp as a `%make-compiled-
  function` kernel primitive — register in bocl.c like the cfasl prims — OR build
  the object in Lisp if a Lisp constructor is exposed; a C primitive is simplest).
- **RECONCILE the code representation** (§3.4): make the VM accept the compiler's
  code vector type.
- **ACCEPTANCE**: `(funcall (compile nil '(lambda (x) (+ x 1))) 41)` => 42, running
  the VM; compare a batch of compiled fns against the tree-walker; the compiled
  bodies run materially faster on a loop micro-benchmark. Suite green.

## 7. vm-5 — codegen for control / NLX

BLOCK/RETURN-FROM, TAGBODY/GO, CATCH/THROW, UNWIND-PROTECT, MULTIPLE-VALUE-* , PROGV,
LOAD-TIME-VALUE, EVAL-WHEN, MACROLET/SYMBOL-MACROLET (already expanded), FLET/LABELS.
Implement the NLX opcode handlers in the VM (bytecode.c) using the SAME Frame /
setjmp machinery the tree-walker uses (eval.c: frame_block/frame_catch/
frame_protect + UNWIND). A VM BLOCK/CATCH pushes a Frame with a setjmp target;
RETURN-FROM/THROW unwind to it. MV via the MultipleValues buffer + VALUES/MV-CALL/
MV-BIND opcodes. Special (dynamic) LET binds via the symbol value-cell save/restore
(like eval.c LET) under an unwind-protect frame. ACCEPTANCE: control/condition
suites pass for compiled code.

## 8. vm-6 — closures + interop (uniform APPLY)

Closure capture: `MAKE-CLOSURE tmpl` builds a CompiledClosure over the current
frame's captured cells (a display / upvalue vector); `LOAD-UPVAL i` and
`LOAD-LOCAL depth>0` read them. Design the capture: box the captured locals (or copy
by value if not mutated after capture — analyze in vm-3). Uniform APPLY: a
stack-based calling convention shared by the VM and callees, avoiding the arg-list
re-cons of §3.3, plus the no-C-recursion trampoline + VM stack-overflow signal.
Mixed calls (bytecode↔built-in↔tree-walked) must all work. ACCEPTANCE: closures
over outer vars, mutation-through-closure, apply/funcall parity; suite green.

## 9. vm-7 — CLOS inline cache

Generic-function dispatch in byte-code with a per-call-site inline cache
(monomorphic + small polymorphic), miss recomputes via COMPUTE-APPLICABLE-METHODS +
method-combination; invalidate on add/remove-method / class redefinition (a
generation counter). Mind the kernel CLASS vs CLOS %cls duality for the cache key
(use class_designator_name, not class_name). Factor the cache so a future shared
module could serve both the VM and a tree-walker fast path (plan-3 hot-5).

## 10. vm-8 — byte-code fasl (reuses cfasl-1..4)

Serialize CompiledFunction/CompiledClosure (bytecode + constant pool + arity + name
+ source) via the cfasl streaming object model (sources/cfasl.*). Add cfasl tags
for the compiled objects (append to `cfasl_tag_t`, bump CFASL_VERSION). COMPILE /
COMPILE-FILE produce byte-code (extend the v1.6.4 COMPILE-FILE: emit compiled
templates instead of / alongside macroexpanded forms). LOAD installs them. Keep the
BYTECODE_VERSION check. ACCEPTANCE: a byte-code fasl loads + runs, faster than the
text/source path.

## 11. vm-9 — debug, self-host, validate

Backtrace/condition frames for VM frames (the debugger must show compiled frames —
extend the Frame metadata like eval.c's block-frame function/environment). Compile
the PRELUDE (or ASDF corpus) to byte-code and self-host. Benchmark vs the
tree-walker (target 5-30x on real code per the plan). Full suite green. Confirm the
one-command C17 build still holds (computed-goto stays optional under #ifdef).

---

## 12. File map (where things are)

- `sources/bytecode.h` — ISA spec + object API + BC_ARITY_* macros + BYTECODE_VERSION.
- `sources/bytecode.c` — predicates, CompiledFunction/Closure constructors+accessors,
  `bytecode_apply` (the VM loop), `vm_read_uvar/svar`, `make_vm_vector`.
- `sources/kernel_types.h` — `CompiledFunction`/`CompiledClosure` structs (~line 373);
  the `Slots` union (~line 492) includes them.
- `sources/kernel.c` — `kernel_apply` (~2575) dispatches compiled fns to
  `bytecode_apply`; `Object_new` (70), `Array_slots` (141), `function_new` pattern (2511).
- `sources/kernel_objects.c` — COMPILED_FUNCTION_Class / COMPILED_CLOSURE_Class boot.
- `sources/built_in_functions.c` — `kernel_FUNCTIONP` accepts compiled fns (~805).
- `sources/eval.c` — the tree-walker: evale (452), LET (735), bind_required_parameters
  (1850), the Frame/UNWIND machinery for vm-5 to mirror.
- `sources/variable.c` — `special_variable_p` (34) for vm-3's special detection.
- `sources/cfasl.{h,c}` — the object serializer for vm-8.
- `bootstrap/prelude.lisp` — macroexpand-all (~9455), COMPILE-FILE (~9805), the
  end-of-file socket.lisp loader pattern to copy for loading vm-compiler.lisp.
- `tests/test_misc.c` — `test_vm_compiled_function`, `test_vm_execute` (extend here;
  it already includes bytecode.h + memory.h; `vm_code_str`/`vm_call` helpers exist).

## 13. Gotchas / lessons

- GC rooting (§3.6) — the single biggest hazard; parked-in-vector discipline is safe.
- Allocation is CHEAP in bocl — optimize CPU (dispatch, no re-parse), not allocs.
- `S(NAME)` caches the interned symbol (static Ref) — cheap; use freely in C.
- `#\Newline` is NEL (133) not LF; strings are 1 octet/char (byte == char).
- Runaway bocl REPL procs ignore SIGTERM — use `timeout -s KILL`, bounded loops.
- callgrind is impractical (prelude load dominates ~100x); use A/B timing + the
  -DBOCL_PROFILE counters (allocations/funcalls/gc) instead.
- Keep the switch VM the portable default; computed-goto only under `#ifdef __GNUC__`.
