# bocl Common Lisp symbol-status inventory

Snapshot at v0.0.20. Cross-references every external symbol of the
`COMMON-LISP` package (978 in total per ANSI CL) against what bocl
currently provides — built-ins in the C kernel, special operators in
`evale`, or macros/functions defined in `bootstrap/prelude.lisp`.

## Overall coverage

| Kind         | Implemented | Total | % |
|--------------|-------------|-------|---|
| Special operator | 21 | 25 | **84 %** |
| Function     | 175 | 636 | **28 %** |
| Macro        | 21 | 91 | **23 %** |
| Variable     | 11 | 50 | **22 %** |
| Constant     | 11 | 62 | **18 %** |
| Type / decl  | 3  | 114 | **3 %** |
| **Total**    | **242** | **978** | **25 %** |

The split makes sense: the kernel implements most of the evaluation
core (special operators and the printer/reader/numeric/list surface),
and almost nothing of CLOS, types, the formatting/pretty-printer
machinery, or the iteration macros yet.

## Architecture plan: C kernel vs Lisp side

A guiding principle: **anything that can be expressed in CL using what
the kernel already provides should live in `bootstrap/*.lisp`, not in
C**. The C side carries only:

1. Special operators (the four still missing — see below).
2. Primitives that need direct memory access or POSIX system calls
   (file streams, GC interaction, `set-symbol-value`, …).
3. The minimum sequence layer required to express the rest of CL on
   top (already done: `mapcar`, `funcall`, etc.).
4. Arithmetic on the kernel-native numeric types.

Everything else — `DEFCLASS`, `DEFSTRUCT`, `LOOP`, `FORMAT`, `DO`,
`DESTRUCTURING-BIND`, the bulk of the sequence/string libraries — is
straightforward Lisp once macros and FUNCALL/APPLY work.

## The four still-missing special operators

These are kernel C work. Adding them is small but mandatory because
they can't be expressed as macros over what bocl has.

| Special-op | C effort | Notes |
|---|---|---|
| `LET*`        | small  | Like LET but binds sequentially — separate `evale` branch (cannot be a macro over LET because variables must scope to subsequent inits). |
| `MACROLET`    | small  | Add `env_make_local_macro` wrapper paths in evale's local-macro lookup (the wrapper already exists). |
| `SYMBOL-MACROLET` | small | Same, but on variable lookups; check `env_local_symbol_macro_p` before treating the symbol as an ordinary variable. |
| `LOAD-TIME-VALUE` | trivial | Without a real compiler, this can be a no-op that just evaluates its form at read-eval time. |

## Macros to add to `bootstrap/prelude.lisp` next (Lisp side)

These are all macro definitions over the already-shipped primitives:

| Macro | Notes |
|---|---|
| `SETF`              | The big one. Generic place expander; needs a registry of place handlers. Start with `(setf (gethash k h) v)`, `(setf (car x) v)`, `(setf (cdr x) v)`, `(setf (symbol-value s) v)`, `(setf (symbol-function s) v)`. |
| `CASE` / `ECASE`    | Trivial expansion to a chain of IFs. |
| `TYPECASE` / `ETYPECASE` | Same shape as CASE but uses TYPEP. |
| `MULTIPLE-VALUE-BIND` | Expands to a MULTIPLE-VALUE-CALL with a lambda. |
| `MULTIPLE-VALUE-LIST` | `(multiple-value-call #'list form)`. |
| `MULTIPLE-VALUE-SETQ` | Similar. |
| `NTH-VALUE`         | M-V-CALL with a nth-extraction lambda. |
| `DESTRUCTURING-BIND` | Recursive lambda-list expansion. Medium complexity. |
| `DO` / `DO*`        | Tagbody + Go loop. Mechanical. |
| `IGNORE-ERRORS`     | HANDLER-CASE catching ERROR, returning `(values nil c)`. |
| `ASSERT`            | Expansion to (UNLESS test (ERROR …)). |
| `CHECK-TYPE`        | Expansion to (UNLESS (TYPEP …) (ERROR …)). |
| `WITH-OPEN-FILE`    | UNWIND-PROTECT around OPEN + body + CLOSE. |
| `WITH-OPEN-STREAM`  | Same pattern. |
| `DECLAIM` / `DECLARE` | No-op for the unenforced phase; later, store in symbol plist. |
| `RETURN`            | `(return-from nil …)`. |

## Macros that need real work

| Macro | Why it's big |
|---|---|
| `DEFSTRUCT` | Generates constructor, accessors, predicate, copier, type. Several hundred lines. Sits on top of `make-structure-class`, which the kernel already has. |
| `DEFCLASS` / `DEFGENERIC` / `DEFMETHOD` | Full CLOS. A few thousand lines of Lisp (CLOSette-style port). |
| `LOOP`   | The clause language is essentially its own embedded compiler. |
| `FORMAT` | The directive engine is huge but mechanical. |
| `DEFINE-CONDITION` | DEFCLASS for the condition hierarchy. Needs CLOS. |

## Functions: the top-50 missing, by area

Selected for highest payoff per implementation effort.

### Sequence / list (Lisp side)
`REDUCE`, `FIND`, `FIND-IF`, `POSITION`, `POSITION-IF`, `COUNT`,
`COUNT-IF`, `REMOVE`, `REMOVE-IF`, `SUBSEQ`, `BUTLAST`, `MAPLIST`,
`MAPCON`, `MAPCAN`, `CONCATENATE`, `MERGE`, `SORT`, `STABLE-SORT`,
`UNION`, `INTERSECTION`, `SET-DIFFERENCE`, `ADJOIN`, `PAIRLIS`.

### Strings (Lisp side)
`STRING=`, `STRING<`, `STRING>`, `STRING-UPCASE`, `STRING-DOWNCASE`,
`STRING-CAPITALIZE`, `STRING-TRIM`, `STRING-LEFT-TRIM`,
`STRING-RIGHT-TRIM`, `SEARCH`, `SUBSTITUTE`, `CHAR-UPCASE`,
`CHAR-DOWNCASE`, `DIGIT-CHAR`, `DIGIT-CHAR-P`, `ALPHA-CHAR-P`,
`ALPHANUMERICP`.

### Numeric (mixed)
- **C kernel** (need direct math): `SIN`, `COS`, `TAN`, `ASIN`, `ACOS`,
  `ATAN`, `EXP`, `LOG`, `SQRT`, `EXPT`, `SINH`, `COSH`, `TANH`.
- **Lisp side**: `GCD`, `LCM`, `MAX`/`MIN` for n-ary (have 2-ary in C),
  `SIGNUM`, `ISQRT`, `RATIONAL`, `RATIONALIZE`.

### I/O (mostly Lisp on top of kernel streams)
`WRITE`, `WRITE-LINE`, `WRITE-STRING`, `WRITE-CHAR`, `READ-LINE`,
`READ-CHAR-NO-HANG`, `Y-OR-N-P`, `FRESH-LINE`, `PPRINT`, `FORMAT`
(big macro), `OPEN`-with-defaults, `WITH-OUTPUT-TO-STRING`,
`WITH-INPUT-FROM-STRING`.

### Numeric coercion
`CEILING`, `FLOOR`, `ROUND`, `TRUNCATE` already shipped; need
`FCEILING`, `FFLOOR`, `FROUND`, `FTRUNCATE`, `COMPLEX`, `REALPART`,
`IMAGPART` (last three blocked on complex-number support).

### Hash-table (Lisp side)
`MAKE-HASH-TABLE` keyword shims, `WITH-HASH-TABLE-ITERATOR`,
`HASH-TABLE-REHASH-SIZE` etc. — accessor functions only.

### CLOS surface (huge)
Whole `DEFCLASS` family. Plan: port CLOSette.

### Conditions / restarts (mixed)
- **C kernel**: `HANDLER-BIND`, `RESTART-CASE`, `RESTART-BIND`,
  `INVOKE-RESTART`, `FIND-RESTART`. Need handler-runs-in-dynamic-
  context and restart-frame plumbing on top of the existing
  `frame_handler` + `frame_restart` types.
- **Lisp side**: `CERROR` (over `ERROR` + `INVOKE-RESTART`),
  `MUFFLE-WARNING`, the standard restart functions.

### Type system (mostly Lisp)
`DEFTYPE`, `SUBTYPEP`, `TYPECASE`, `CHECK-TYPE`, `THE` (kernel has the
special-op but no real check), `COERCE` for non-trivial conversions.

## Variables still missing

Mostly the printer/reader control variables:
`*PRINT-*` (16 of them), `*READ-*` (5), `*DEFAULT-PATHNAME-DEFAULTS*`,
`*RANDOM-STATE*`, `*MODULES*`, `*GENSYM-COUNTER*`. None of these need
C work — `define_parameter` from a small Lisp init file is enough.

## Constants still missing

`PI`, the `BOOLE-*` family (used by `BOOLE`), `CHAR-CODE-LIMIT` is
done, `LEAST-POSITIVE-*`, `LEAST-NEGATIVE-*`, `LONG-FLOAT-EPSILON`,
the various FP limits. Define them with `define_constant` from C
(some) or `defconstant` in Lisp (most).

## Implementation order

Now that we have macros + APPLY + sequence basics:

1. **Four missing special operators** in the C kernel (`LET*`,
   `MACROLET`, `SYMBOL-MACROLET`, `LOAD-TIME-VALUE`). Small.
2. **SETF** as a Lisp macro with a registry of place expanders.
   Critical — unblocks dozens of CL idioms.
3. **Trivial control macros**: `CASE`, `ECASE`, `TYPECASE`,
   `MULTIPLE-VALUE-BIND`, `DO` / `DO*`, `IGNORE-ERRORS`, `ASSERT`,
   `CHECK-TYPE`, `RETURN`, `WITH-OPEN-FILE`. All small DEFMACROs.
4. **Sequence library** (Lisp): `REDUCE`, `FIND`, `POSITION`, `COUNT`,
   `REMOVE`, `SORT`, `MAPCAN`, etc.
5. **String library** (Lisp): comparison, case conversion, trimming.
6. **Numeric extras** (mixed): `EXPT`, transcendentals (C);
   `SIGNUM`, `GCD`, `LCM` (Lisp).
7. **DESTRUCTURING-BIND** (Lisp).
8. **DEFSTRUCT** (Lisp, on top of `make-structure-class`).
9. **HANDLER-BIND** + restart system in C; `CERROR` follows.
10. **CLOS** — port CLOSette on top of all of the above.
11. **LOOP** — the macro.
12. **FORMAT** — the directive engine.
13. **DEFINE-CONDITION** + ANSI condition hierarchy.

## Appendix A: the 242 implemented symbols

Stored at `/tmp/clhs-implemented.txt` after running the inventory
pipeline; not duplicated here. The full lists, by kind, are derivable
via:

```sh
sbcl --noinform --no-userinit --no-sysinit --non-interactive --eval '
  (let ((out ()))
    (do-external-symbols (s "COMMON-LISP")
      (let ((kind (cond ((special-operator-p s) "special-op")
                        ((macro-function s)     "macro")
                        ((fboundp s)            "function")
                        ((constantp s)          "constant")
                        ((boundp s)             "variable")
                        (t                      "type-or-decl"))))
        (push (format nil "~A:~A" (symbol-name s) kind) out)))
    (with-open-file (o "/tmp/clhs-kinds.txt" :direction :output
                       :if-exists :supersede)
      (dolist (l (sort out (function string<)))
        (format o "~A~%" l))))'
```
