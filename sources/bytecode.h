#ifndef bytecode_h
#define bytecode_h
/* bytecode.h — bocl byte-code VM: instruction set + compiled-function object
   (issue vm-1-isa-and-function-layout, plan-2-bytecode-vm).

   THIS HEADER IS THE ISA SPEC.  The VM loop (vm-2), the code generator (vm-4/5)
   and the byte-code fasl (vm-8) all target exactly what is written here.  vm-1 is
   DESIGN ONLY — it defines the opcodes, their operand encodings, the stack-effect
   table, and the runtime compiled-function object; no execution happens yet (the
   VM loop is vm-2).

   WHY A STACK VM: it is the simplest target to compile to and the most portable —
   the dispatch is a plain C `switch` (a computed-goto fast path is an OPTIONAL
   #ifdef __GNUC__ acceleration in vm-2, never required).  No native codegen / JIT
   / assembler, so the "one-command C17 build under POSIX" invariant holds.

   VALUE REPRESENTATION: the VM reuses the existing tagged `Ref` unchanged — the
   operand stack, locals and constant pool all hold Refs, so the VM and the
   tree-walker share values with ZERO conversion and can inter-call freely.

   ------------------------------------------------------------------------------
   OPERAND ENCODING (immediates that follow an opcode byte, in code order):
     u8     : one raw byte (small counts: CALL n, DROP n, VALUES n).
     uvar   : LEB128 unsigned varint  (constant / symbol / local / template index,
              local depth).  Reuses the cfasl varint wire form (see cfasl.h).
     svar   : zig-zag LEB128 signed varint — a RELATIVE jump displacement measured
              from the byte immediately AFTER the full instruction.
   Multi-operand opcodes list their operands left-to-right in the table below.

   STACK-EFFECT NOTATION:  before -- after   (top of stack on the right).
   `vN` = the N values delivered by the last multiple-value producer.

   ------------------------------------------------------------------------------
   INSTRUCTION SET (opcode byte = the enum value below)

   -- stack / constants --------------------------------------------------------
   BC_PUSH_CONST  uvar(k)     -- ; push constants[k]                 ( -- x )
   BC_PUSH_NIL                -- ; push NIL                          ( -- nil )
   BC_PUSH_T                  -- ; push T                            ( -- t )
   BC_DUP                     -- ; duplicate top                     ( x -- x x )
   BC_POP                     -- ; discard top                       ( x -- )
   BC_DROP        u8(n)       -- ; discard the top n                 ( x1..xn -- )

   -- lexical locals (hot-2 (depth,index) addresses) ---------------------------
   BC_LOAD_LOCAL  uvar(d) uvar(i)  -- ; push slot i of the frame d parents up
   BC_STORE_LOCAL uvar(d) uvar(i)  -- ; pop -> that slot (leaves the value)  (x -- x)
     depth 0 = the current (innermost) frame; depth counts lexical parents.

   -- globals / function cells (constant pool holds the symbol) -----------------
   BC_LOAD_GLOBAL   uvar(s)   -- ; push SYMBOL-VALUE   of constants[s]   ( -- x )
   BC_STORE_GLOBAL  uvar(s)   -- ; SET  SYMBOL-VALUE   of constants[s]   ( x -- x )
   BC_LOAD_FUNCTION uvar(s)   -- ; push SYMBOL-FUNCTION of constants[s]  ( -- fn )

   -- control flow (svar offset relative to the next instruction) ---------------
   BC_JUMP           svar(o)  -- ; pc += o                            ( -- )
   BC_JUMP_IF_NIL    svar(o)  -- ; pop x; if x is NIL pc += o         ( x -- )
   BC_JUMP_IF_NOT_NIL svar(o) -- ; pop x; if x is non-NIL pc += o     ( x -- )

   -- calls / return -----------------------------------------------------------
   BC_CALL       u8(n)        -- ; callee then n args on top; call, push 1 result
                                   ( fn a1..an -- r )   (multiple values in the MV buffer)
   BC_CALL_NAMED uvar(s) u8(n)-- ; like CALL but the callee is SYMBOL-FUNCTION of
                                   constants[s] (fetched by the VM)  ( a1..an -- r )
   BC_TAIL_CALL  u8(n)        -- ; tail position: reuse the frame, does not return here
   BC_RETURN                  -- ; return top-of-stack (+ the MV buffer) to the caller

   -- multiple values ----------------------------------------------------------
   BC_VALUES   u8(n)          -- ; take the top n into the MV buffer   ( x1..xn -- v1 )
   BC_MV_CALL                 -- ; MULTIPLE-VALUE-CALL: gather producers' values (vm-5)
   BC_MV_BIND  u8(n)          -- ; bind n locals from the MV buffer (vm-5)
   BC_MV_LIST                 -- ; replace the top (a producer's primary) with the LIST
                                   of all its values from the MV buffer  ( v1 -- (v1..vk) )
                                   MULTIPLE-VALUE-CALL/-PROG1 codegen uses this + the
                                   APPEND / APPLY / VALUES-LIST builtins (vm-5).

   -- non-local exit frame markers (bodies are already lexical; details vm-5) ---
   BC_BLOCK_OPEN  uvar(k)     -- ; establish a BLOCK named constants[k]; catch tag
   BC_RETURN_FROM uvar(k)     -- ; RETURN-FROM constants[k] with top-of-stack
   BC_CATCH_OPEN              -- ; CATCH: pop the tag, establish a catch frame
   BC_THROW                   -- ; THROW: pop tag then value
   BC_UNWIND_PROTECT          -- ; enter protected form; cleanup thunk on the stack
   BC_UNWIND_LEAVE            -- ; leave/run the pending cleanup

   -- closures (capture design in vm-6) ----------------------------------------
   BC_MAKE_CLOSURE uvar(t)    -- ; instantiate the compiled template constants[t]
                                   over the current environment       ( -- clo )
   BC_LOAD_UPVAL   uvar(i)    -- ; push captured upvalue i of the running closure

   -- housekeeping -------------------------------------------------------------
   BC_NOP                     -- ; no operation
   BC_HALT                    -- ; stop the VM (top-level / debugging)

   New opcodes MUST be appended (never renumbered) so a byte-code fasl (vm-8)
   stays loadable; BC_OPCODE_COUNT is the count for table sizing. */

#include <stdint.h>
#include <stddef.h>

typedef enum {
    BC_PUSH_CONST = 0,
    BC_PUSH_NIL,
    BC_PUSH_T,
    BC_DUP,
    BC_POP,
    BC_DROP,
    BC_LOAD_LOCAL,
    BC_STORE_LOCAL,
    BC_LOAD_GLOBAL,
    BC_STORE_GLOBAL,
    BC_LOAD_FUNCTION,
    BC_JUMP,
    BC_JUMP_IF_NIL,
    BC_JUMP_IF_NOT_NIL,
    BC_CALL,
    BC_CALL_NAMED,
    BC_TAIL_CALL,
    BC_RETURN,
    BC_VALUES,
    BC_MV_CALL,
    BC_MV_BIND,
    BC_BLOCK_OPEN,
    BC_RETURN_FROM,
    BC_CATCH_OPEN,
    BC_THROW,
    BC_UNWIND_PROTECT,
    BC_UNWIND_LEAVE,
    BC_MAKE_CLOSURE,
    BC_LOAD_UPVAL,
    BC_NOP,
    BC_HALT,
    BC_MV_LIST,          /* appended in vm-5 (never renumber earlier opcodes) */
    BC_CATCH_CLOSE,      /* appended in vm-5 part 2b: normal exit from a CATCH body */
    BC_DYNAMIC_BIND,     /* vm-5 part 2b: bind a special var (constant sym), push a frame */
    BC_DYNAMIC_UNBIND,   /* vm-5 part 2b: normal exit — restore the top special binding */
    BC_UNWIND_NORMAL,    /* vm-5 part 2b: UNWIND-PROTECT normal completion (save + to cleanup) */
    BC_UNWIND_END,       /* vm-5 part 2b: end of an UNWIND-PROTECT cleanup (restore / cascade) */
    BC_PROGV_BIND,       /* vm-5: PROGV — bind a runtime list of specials, push a frame */
    BC_PROGV_UNBIND,     /* vm-5: PROGV — normal exit: restore all + pop */
    BC_OPCODE_COUNT
} bytecode_op_t;

/* Current byte-code format version — bumped when the opcode encodings change so a
   byte-code fasl (vm-8) can refuse an incompatible image.  v2: BC_MV_LIST.
   v3: BC_CATCH_CLOSE + CATCH_OPEN/THROW.  v4: BC_DYNAMIC_BIND/UNBIND.
   v5: BC_UNWIND_PROTECT + BC_UNWIND_NORMAL/END.  v6: BC_PROGV_BIND/UNBIND. */
#define BYTECODE_VERSION 6

/* ---- Arity descriptor -------------------------------------------------------
   A compiled function's argument shape, packed in a fixnum so the VM's entry
   sequence is a couple of masks, not a lambda-list re-parse (this is the CPU win
   the tree-walker lacks — bind_required_parameters re-parses on every call).
     bits  0..7  : number of REQUIRED parameters
     bits  8..15 : number of &OPTIONAL parameters
     bit   16    : &REST / &BODY present
     bit   17    : &KEY present (the accepted keys live in the constant pool)
     bit   18    : &ALLOW-OTHER-KEYS
   Functions whose lambda list exceeds these fields (huge arities, nested
   destructuring in macros) fall back to the tree-walker (vm-6). */
#define BC_ARITY_REQUIRED(a)   ((int)( (a)        & 0xff))
#define BC_ARITY_OPTIONAL(a)   ((int)(((a) >> 8)  & 0xff))
#define BC_ARITY_REST_P(a)     (((a) >> 16) & 1)
#define BC_ARITY_KEY_P(a)      (((a) >> 17) & 1)
#define BC_ARITY_AOK_P(a)      (((a) >> 18) & 1)
#define BC_ARITY_PACK(req,opt,rest,key,aok) \
    ( ((req)&0xff) | (((opt)&0xff)<<8) | ((rest)?1<<16:0) | ((key)?1<<17:0) | ((aok)?1<<18:0) )
/* bit 19 — RAW entry (vm-6 part 2): the VM binds the WHOLE argument list into slot 0
   and does no other parameter processing; a compiled PROLOGUE destructures it into the
   analyzer's slots.  Used for lambda lists the fast entry can't express (an &optional
   with a non-NIL default or supplied-p, &key, or &aux). */
#define BC_ARITY_RAW           (1 << 19)
#define BC_ARITY_RAW_P(a)      (((a) >> 19) & 1)

/* ---- Compiled-function object ----------------------------------------------
   struct CompiledFunction / CompiledClosure live in kernel_types.h; they are
   FUNCTION-family kernel objects (COMPILED_FUNCTION_Class / COMPILED_CLOSURE_Class,
   already in the object zoo).  All slots are Refs (GC-traced).  A CompiledFunction
   is a TEMPLATE (shared, immutable); a CompiledClosure pairs a template with its
   captured upvalues (vm-6).  The CL FUNCTIONP and the APPLY path (vm-6) accept
   both alongside FUNCTION / CLOSURE / BUILT-IN-FUNCTION. */
typedef struct Object* Ref;   /* canonical typedef in kernel_types.h; identical redef legal in C11+ */

/* Predicates (class_of == the compiled class). */
int compiled_function_p(Ref object);
int compiled_closure_p(Ref object);

/* Construct a compiled-function template.  code is an (unsigned-byte 8) vector,
   constants a simple-vector, arity a BC_ARITY_PACK fixnum, max_stack / n_locals
   fixnums, source the source lambda expression (or NIL) for FUNCTION-LAMBDA-
   EXPRESSION / DISASSEMBLE, name a symbol or NIL. */
Ref compiled_function_new(Ref name, Ref lambda_list, Ref code, Ref constants,
                          Ref arity, Ref max_stack, Ref n_locals, Ref source);
Ref compiled_closure_new(Ref compiled_function, Ref upvalues);

/* Accessors (checked). */
Ref compiled_function_name(Ref cf);
Ref compiled_function_lambda_list(Ref cf);
Ref compiled_function_code(Ref cf);
Ref compiled_function_constants(Ref cf);
Ref compiled_function_arity(Ref cf);
Ref compiled_function_max_stack(Ref cf);
Ref compiled_function_n_locals(Ref cf);
Ref compiled_function_source(Ref cf);
Ref compiled_closure_function(Ref cc);
Ref compiled_closure_upvalues(Ref cc);

/* APPLY a compiled function / closure.  vm-1 provides a stub that signals an
   error; vm-2 replaces it with the VM loop.  kernel_apply dispatches to it. */
struct MultipleValues;
void bytecode_apply(Ref function, Ref arguments, struct MultipleValues* results);

#endif
