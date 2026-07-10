/* bytecode.c — byte-code VM compiled-function object (issue vm-1).
   vm-1 is DESIGN + the runtime object only: the predicates, the constructor and
   the accessors for CompiledFunction / CompiledClosure (see bytecode.h for the
   ISA).  The VM execution loop is vm-2; until then bytecode_apply() signals a
   clear error rather than mis-dispatching. */

#include "bytecode.h"
#include "kernel.h"
#include "kernel_types.h"
#include "kernel_objects.h"
#include "kernel_functions.h"
#include "memory.h"
#include "boerror.h"
#include <setjmp.h>

extern Ref Object_new(Ref class, Slots* slots);   /* defined in kernel.c (as in reader.c) */

/* ---- dynamic non-local-exit frames for the VM (vm-5 part 2b) ----------------
   CATCH/THROW, UNWIND-PROTECT and dynamic (special) binding are DYNAMICALLY scoped:
   they live on the process-global frame stack g_frame_stack (eval.c) so a THROW or an
   unwind crosses call boundaries and interoperates with tree-walked frames.  (BLOCK/
   TAGBODY are LEXICAL and already compile to plain jumps — vm-5 part 1 — so the VM
   never needs eval.c's push_frame/pop_frame, which also maintain the per-environment
   lexical list.)  We only touch g_frame_stack here; the cascade protocol (setjmp
   returns non_local_exit_unwind/target, run cleanup, pop, unwind to the next frame)
   is identical to eval.c's, so mixed VM/tree-walker unwinds work. */
extern Frame* g_frame_stack;   /* eval.c:44 (non-static; not otherwise declared) */

static void vm_push_frame(Frame* f){
    f->next = g_frame_stack; g_frame_stack = f;
    f->env_next = NULL; f->frame = NULL; f->target = NULL; }
static Frame* vm_pop_frame(void){
    Frame* r = g_frame_stack; if(r){ g_frame_stack = r->next; } return r; }
static Frame* vm_find_catch_frame(Ref tag){
    for(Frame* f=g_frame_stack; f; f=f->next){
        if(f->type==frame_catch && f->parameters.catch.object==tag){ return f; } }
    return NULL; }
/* Continue the unwind cascade after POPPED has been removed: hand control to the new
   top with the right code (target if it is POPPED's destination, else unwind). */
static _Noreturn void vm_unwind_next_frame(Frame* popped){
    Frame* next = g_frame_stack;
    if(next==NULL){ FATAL("vm_unwind_next_frame: %s","no next frame; target not reached"); }
    non_local_exit_t code = (next==popped->target) ? non_local_exit_target : non_local_exit_unwind;
    next->target = popped->target;
    longjmp(next->non_local_exit, (int)code); }

/* One VM dynamic-frame record.  FRAME is first so the active record is recovered
   after a longjmp as (VmDyn*)g_frame_stack (the frame receiving control is the stack
   top).  Fields other than FRAME are read only via that recovered pointer or via the
   volatile depth counter, never via a stale automatic — safe across setjmp/longjmp. */
enum { VMK_CATCH=1, VMK_PROTECT, VMK_DBIND, VMK_PROGV };
typedef struct VmDyn {
    Frame  frame;          /* MUST be first */
    size_t handler_pc;     /* CATCH: resume pc on throw; PROTECT: cleanup pc */
    word   handler_sp;     /* sp to restore on entry to the handler / cleanup */
    int    kind;
    int    unwinding;      /* PROTECT: set when the cleanup is running due to an unwind */
    MultipleValues saved;  /* PROTECT: the protected form's values (normal path) */
    Ref    dsym;           /* DBIND: the special symbol */
    Ref    dsaved_value;   /* DBIND: its previous value */
    int    dsaved_boundp;  /* DBIND: whether it was bound */
    Ref    dlist;          /* PROGV: saved list ((sym boundp . oldvalue) ...) */
} VmDyn;
#define VM_DYN_MAX 256

/* Restore a PROGV frame's special variables from its saved ((sym boundp . old) ...). */
static void vm_progv_restore(VmDyn* a){
    for(Ref r=a->dlist; consp(r); r=cons_cdr(r)){
        Ref rec = cons_car(r);
        Ref sym = cons_car(rec);
        Ref bp  = cons_car(cons_cdr(rec));
        Ref old = cons_cdr(cons_cdr(rec));
        if(!nullp(bp)){ set_symbol_value(sym, old); } else { symbol_make_unbound(sym); } }}

int compiled_function_p(Ref object){ return class_of(object)==COMPILED_FUNCTION_Class; }
int compiled_closure_p(Ref object){  return class_of(object)==COMPILED_CLOSURE_Class; }

/* ---- CompiledFunction template ---------------------------------------------
   All eight slots are Refs, so objcount=8 and the GC traces them generically
   (same discipline as Function_slots).  Inputs are rooted across allocate(). */
static CompiledFunction* CompiledFunction_slots(Ref name, Ref lambda_list, Ref code,
                                                Ref constants, Ref arity, Ref max_stack,
                                                Ref n_locals, Ref source){
    uword pool=open_live_pool();
    add_to_live_pool(name);       add_to_live_pool(lambda_list); add_to_live_pool(code);
    add_to_live_pool(constants);  add_to_live_pool(arity);       add_to_live_pool(max_stack);
    add_to_live_pool(n_locals);   add_to_live_pool(source);
    CompiledFunction* that=allocate(sizeof(*that));
    close_live_pool(pool);
    that->objcount    = 8;
    that->lisp_name   = name;
    that->lambda_list = lambda_list;
    that->code        = code;
    that->constants   = constants;
    that->arity       = arity;
    that->max_stack   = max_stack;
    that->n_locals    = n_locals;
    that->source      = source;
    return that;}

Ref compiled_function_new(Ref name, Ref lambda_list, Ref code, Ref constants,
                          Ref arity, Ref max_stack, Ref n_locals, Ref source){
    return Object_new(COMPILED_FUNCTION_Class,
                      (Slots*)CompiledFunction_slots(name,lambda_list,code,constants,
                                                     arity,max_stack,n_locals,source));}

static CompiledClosure* CompiledClosure_slots(Ref function, Ref upvalues){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(upvalues);
    CompiledClosure* that=allocate(sizeof(*that));
    close_live_pool(pool);
    that->objcount = 2;
    that->function = function;
    that->upvalues = upvalues;
    return that;}

Ref compiled_closure_new(Ref function, Ref upvalues){
    return Object_new(COMPILED_CLOSURE_Class,(Slots*)CompiledClosure_slots(function,upvalues));}

/* ---- Accessors (checked) ---------------------------------------------------- */
Ref compiled_function_name(Ref cf){        check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.lisp_name;}
Ref compiled_function_lambda_list(Ref cf){ check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.lambda_list;}
Ref compiled_function_code(Ref cf){        check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.code;}
Ref compiled_function_constants(Ref cf){   check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.constants;}
Ref compiled_function_arity(Ref cf){       check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.arity;}
Ref compiled_function_max_stack(Ref cf){   check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.max_stack;}
Ref compiled_function_n_locals(Ref cf){    check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.n_locals;}
Ref compiled_function_source(Ref cf){      check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.source;}
Ref compiled_closure_function(Ref cc){     check_type(cc,COMPILED_CLOSURE_Class);  return cc->slots->compiled_closure.function;}
Ref compiled_closure_upvalues(Ref cc){     check_type(cc,COMPILED_CLOSURE_Class);  return cc->slots->compiled_closure.upvalues;}

/* ============================================================================ */
/* vm-2: the dispatch core — operand stack, locals, and the opcode loop.        */
/* ============================================================================ */

extern Array* Array_slots(Ref element_type, halfword rank, halfword* dimensions);

/* A length-n simple-vector of NIL, for locals / the operand stack.  Its elements
   are a GC-traced block (non-moving, so the raw element pointer stays valid for
   the run), which roots every Ref the VM parks there — no separate scan region. */
static Ref make_vm_vector(word n){
    halfword dims[1]; dims[0]=(halfword)n;
    Array* a = Array_slots(T_Symbol, 1, dims);
    Ref v = Object_new(VECTOR_Class, (Slots*)a);
    Ref* el = v->slots->array.elements->elements;
    for(word i=0;i<n;i++){ el[i]=NIL_Symbol; }
    return v;}

static const char* cf_name_cstr(Ref cf){
    Ref name = compiled_function_name(cf);
    return symbolp(name) ? string_cstring(symbol_name(name)) : "anonymous byte-code function";}

/* LEB128 readers over the code octets; *pc advances past the operand. */
static uint64_t vm_read_uvar(const octet* code, size_t* pc){
    uint64_t v=0; int shift=0; uint8_t b;
    do { b=code[(*pc)++]; v |= (uint64_t)(b & 0x7fu) << shift; shift+=7; } while(b & 0x80u);
    return v;}
static int64_t vm_read_svar(const octet* code, size_t* pc){
    uint64_t zz = vm_read_uvar(code, pc);
    return (int64_t)(zz >> 1) ^ -(int64_t)(zz & 1);}

/* Run one compiled function / closure.  ARGUMENTS is a Lisp list; the VM binds it
   into the locals per the packed arity, then loops over the bytecode.  Nested
   CALLs dispatch through kernel_apply (uniform interop; the stack-based fast path
   is vm-6).  Non-control opcodes only (control / NLX are vm-5). */
void bytecode_apply(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);

    Ref cf       = compiled_closure_p(function) ? compiled_closure_function(function) : function;
    Ref upvalues = compiled_closure_p(function) ? compiled_closure_upvalues(function) : NIL_Symbol;
    add_to_live_pool(cf);
    add_to_live_pool(upvalues);
    check_type(cf, COMPILED_FUNCTION_Class);

    Ref  codestr   = compiled_function_code(cf);
    if(!stringp(codestr)){ ERROR("byte-code must be an octet (string) vector for now%s",""); }
    const octet* code = string_octets(codestr);
    Ref  constants = compiled_function_constants(cf);
    word arity     = integer_value(compiled_function_arity(cf));
    word nloc      = integer_value(compiled_function_n_locals(cf));
    word maxst     = integer_value(compiled_function_max_stack(cf));

    Ref locals = make_vm_vector(nloc);  add_to_live_pool(locals);
    Ref stack  = make_vm_vector(maxst); add_to_live_pool(stack);
    Ref* L = locals->slots->array.elements->elements;
    Ref* S = stack->slots->array.elements->elements;
    void* Sbar = stack->slots->array.elements;   /* write-barrier container, stable */
    Ref* K = constants->slots->array.elements->elements;   /* constant pool, direct */

    /* --- bind arguments into locals[0..] : required, optional, rest --- */
    {   word req = BC_ARITY_REQUIRED(arity), opt = BC_ARITY_OPTIONAL(arity);
        int rest = BC_ARITY_REST_P(arity);
        Ref a = arguments; word li = 0;
        for(word i=0;i<req;i++){
            if(!consp(a)){ ERROR("%s: too few arguments", cf_name_cstr(cf)); }
            L[li++] = cons_car(a); a = cons_cdr(a); }
        for(word i=0;i<opt;i++){
            if(consp(a)){ L[li++] = cons_car(a); a = cons_cdr(a); } else { L[li++] = NIL_Symbol; } }
        if(rest){ L[li++] = a; }
        else if(consp(a)){ ERROR("%s: too many arguments", cf_name_cstr(cf)); }
        gc_write_barrier(locals->slots->array.elements);
    }

    #define PUSH(v)  do { S[sp] = (v); gc_write_barrier(Sbar); sp++; } while(0)
    #define POP()    (S[--sp])
    #define TOP()    (S[sp-1])

    /* Dynamic (NLX) frames established by THIS activation.  NVF is volatile because
       it is read after a longjmp to locate the active frame; PC/SP need not be (the
       longjmp-return paths only ASSIGN them from a recovered frame, never read the
       stale value first).  The array lives on the C stack of this activation, so its
       jmp_bufs stay valid for its whole lifetime. */
    VmDyn vdyn[VM_DYN_MAX];
    volatile word nvf = 0;

    size_t pc = 0; word sp = 0;
    results->count = 1; results->values[0] = NIL_Symbol;
    for(;;){
        uint8_t op = code[pc++];
        switch(op){
        case BC_PUSH_CONST:{ uint64_t k = vm_read_uvar(code,&pc); PUSH(K[k]); break; }
        case BC_PUSH_NIL:  PUSH(NIL_Symbol); break;
        case BC_PUSH_T:    PUSH(T_Symbol);   break;
        case BC_DUP:     { Ref x = TOP(); PUSH(x); break; }
        case BC_POP:       sp--; break;
        case BC_DROP:    { uint8_t n = code[pc++]; sp -= n; break; }
        case BC_LOAD_LOCAL:{ uint64_t d = vm_read_uvar(code,&pc); uint64_t i = vm_read_uvar(code,&pc);
                             if(d==0){ PUSH(L[i]); }
                             else { PUSH(svref(upvalues, integer_from_word((word)i))); } /* display; vm-6 */
                             break; }
        case BC_STORE_LOCAL:{ uint64_t d = vm_read_uvar(code,&pc); uint64_t i = vm_read_uvar(code,&pc);
                             if(d==0){ L[i] = TOP(); gc_write_barrier(locals->slots->array.elements); }
                             else { svref_set(upvalues, integer_from_word((word)i), TOP()); }
                             break; }
        case BC_LOAD_GLOBAL:{ uint64_t s = vm_read_uvar(code,&pc); Ref sym = K[s];
                             if(!symbol_boundp(sym)){ ERROR("Unbound variable: %s", string_cstring(symbol_name(sym))); }
                             PUSH(symbol_value(sym)); break; }
        case BC_STORE_GLOBAL:{ uint64_t s = vm_read_uvar(code,&pc); set_symbol_value(K[s], TOP()); break; }
        case BC_LOAD_FUNCTION:{ uint64_t s = vm_read_uvar(code,&pc); Ref sym = K[s];
                             PUSH(symbol_function(sym)); break; }
        case BC_JUMP:          { int64_t o = vm_read_svar(code,&pc); pc = (size_t)((int64_t)pc + o); break; }
        case BC_JUMP_IF_NIL:   { int64_t o = vm_read_svar(code,&pc); if(nullp(POP())){ pc = (size_t)((int64_t)pc + o); } break; }
        case BC_JUMP_IF_NOT_NIL:{ int64_t o = vm_read_svar(code,&pc); if(!nullp(POP())){ pc = (size_t)((int64_t)pc + o); } break; }
        case BC_CALL:{
            uint8_t n = code[pc++];
            Ref callee = S[sp-n-1];
            Ref args = NIL_Symbol;
            {   uword cp = open_live_pool(); add_to_live_pool(callee);
                for(word i=sp-1;i>=sp-n;i--){ add_to_live_pool(args); args = cons(S[i], args); }
                add_to_live_pool(args);
                MultipleValues r = {0};
                kernel_apply(callee, args, &r);
                sp -= (n+1);
                PUSH(r.count>0 ? r.values[0] : NIL_Symbol);
                *results = r;   /* keep MV for a following RETURN */
                close_live_pool(cp);
            }
            break; }
        case BC_CALL_NAMED:{
            uint64_t s = vm_read_uvar(code,&pc); uint8_t n = code[pc++];
            Ref callee = symbol_function(K[s]);
            Ref args = NIL_Symbol;
            {   uword cp = open_live_pool(); add_to_live_pool(callee);
                for(word i=sp-1;i>=sp-n;i--){ add_to_live_pool(args); args = cons(S[i], args); }
                add_to_live_pool(args);
                MultipleValues r = {0};
                kernel_apply(callee, args, &r);
                sp -= n;
                PUSH(r.count>0 ? r.values[0] : NIL_Symbol);
                *results = r;
                close_live_pool(cp);
            }
            break; }
        case BC_VALUES:{ uint8_t n = code[pc++];
                         if(n==0){ results->count=0; PUSH(NIL_Symbol); break; }
                         /* the n values are S[sp-n .. sp-1]; primary = S[sp-n] */
                         for(uint8_t i=0;i<n && i<MULTIPLE_VALUES_LIMIT;i++){ results->values[i]=S[sp-n+i]; }
                         results->count = (n<MULTIPLE_VALUES_LIMIT)?n:MULTIPLE_VALUES_LIMIT;
                         Ref v0 = S[sp-n]; sp -= n; PUSH(v0); break; }
        case BC_MV_LIST:{ /* replace the producer's primary (top) with the LIST of all
                             its values (from the MV buffer) — MULTIPLE-VALUE-CALL/-PROG1. */
                          word cnt = (word)results->count;
                          uword cp = open_live_pool();
                          Ref lst = NIL_Symbol;
                          for(word i=cnt-1;i>=0;i--){
                              Ref v = results->values[i]; add_to_live_pool(v);
                              add_to_live_pool(lst); lst = cons(v, lst); }
                          add_to_live_pool(lst);
                          sp--;               /* drop the primary */
                          PUSH(lst);
                          close_live_pool(cp);
                          break; }
        case BC_CATCH_OPEN:{
            /* operand: svar offset to the handler (resume point on a matching THROW).
               The tag is on top of the stack. */
            int64_t off = vm_read_svar(code,&pc);
            size_t handler_pc = (size_t)((int64_t)pc + off);
            Ref tag = POP();
            add_to_live_pool(tag);                 /* g_frame_stack is NOT GC-scanned */
            if(nvf >= VM_DYN_MAX){ ERROR("VM: too many nested dynamic frames%s",""); }
            VmDyn* d = &vdyn[nvf];
            d->frame.type = frame_catch;
            d->frame.parameters.catch.object = tag;
            d->frame.results = results;
            d->kind = VMK_CATCH;
            d->handler_pc = handler_pc;
            d->handler_sp = sp;
            int rc = setjmp(d->frame.non_local_exit);
            if(rc==0){ vm_push_frame(&d->frame); nvf++; break; }   /* normal: run the body */
            /* A THROW/unwind reached us; recover the receiving frame (stack top). */
            VmDyn* a = (VmDyn*)g_frame_stack;
            vm_pop_frame(); nvf--;
            if(rc==non_local_exit_target){         /* this CATCH is the destination */
                Ref primary = (results->count>0) ? results->values[0] : NIL_Symbol;
                sp = a->handler_sp; pc = a->handler_pc;
                PUSH(primary);                     /* MV already in results */
                break;
            }
            vm_unwind_next_frame(&a->frame);       /* just passing through — _Noreturn */
        }
        case BC_CATCH_CLOSE:{                       /* normal CATCH exit: drop the frame */
            vm_pop_frame(); nvf--; break; }
        case BC_DYNAMIC_BIND:{
            /* operand: constant index of the special symbol; new value on top.
               Save the old value cell, install the new one, push a restoring frame. */
            uint64_t si = vm_read_uvar(code,&pc);
            Ref sym = K[si];
            Ref newval = POP();
            add_to_live_pool(sym); add_to_live_pool(newval);
            if(nvf >= VM_DYN_MAX){ ERROR("VM: too many nested dynamic frames%s",""); }
            VmDyn* d = &vdyn[nvf];
            d->frame.type = frame_dynamic_binding;
            d->frame.results = results;
            d->kind = VMK_DBIND;
            d->dsym = sym;
            d->dsaved_boundp = symbol_boundp(sym);
            d->dsaved_value = d->dsaved_boundp ? symbol_value(sym) : NIL_Symbol;
            add_to_live_pool(d->dsaved_value);
            set_symbol_value(sym, newval);
            int rc = setjmp(d->frame.non_local_exit);
            if(rc==0){ vm_push_frame(&d->frame); nvf++; break; }
            /* unwinding through: restore the value cell, then continue the cascade */
            VmDyn* a = (VmDyn*)g_frame_stack;
            if(a->dsaved_boundp){ set_symbol_value(a->dsym, a->dsaved_value); }
            else { symbol_make_unbound(a->dsym); }
            vm_pop_frame(); nvf--;
            vm_unwind_next_frame(&a->frame);
        }
        case BC_DYNAMIC_UNBIND:{                    /* normal exit: restore the top binding */
            VmDyn* a = (VmDyn*)g_frame_stack;
            if(a->dsaved_boundp){ set_symbol_value(a->dsym, a->dsaved_value); }
            else { symbol_make_unbound(a->dsym); }
            vm_pop_frame(); nvf--; break; }
        case BC_PROGV_BIND:{
            /* stack: [... symbols-list values-list].  Bind each symbol to its value
               (or make it unbound when VALUES is shorter), remembering the old cells. */
            Ref values = POP();
            Ref symbols = POP();
            add_to_live_pool(symbols); add_to_live_pool(values);
            if(nvf >= VM_DYN_MAX){ ERROR("VM: too many nested dynamic frames%s",""); }
            VmDyn* d = &vdyn[nvf];
            d->frame.type = frame_dynamic_binding;
            d->frame.results = results;
            d->kind = VMK_PROGV;
            Ref saved = NIL_Symbol;
            {   uword cp = open_live_pool(); add_to_live_pool(saved);
                Ref s = symbols, v = values;
                while(consp(s)){
                    Ref sym = cons_car(s);
                    add_to_live_pool(sym);
                    int bp = symbol_boundp(sym);
                    Ref old = bp ? symbol_value(sym) : NIL_Symbol;
                    add_to_live_pool(old);
                    Ref rec = cons(sym, cons(bp?T_Symbol:NIL_Symbol, old));
                    add_to_live_pool(rec); add_to_live_pool(saved);
                    saved = cons(rec, saved); add_to_live_pool(saved);
                    if(consp(v)){ set_symbol_value(sym, cons_car(v)); v = cons_cdr(v); }
                    else { symbol_make_unbound(sym); }
                    s = cons_cdr(s); }
                close_live_pool(cp); }
            d->dlist = saved; add_to_live_pool(saved);
            int rc = setjmp(d->frame.non_local_exit);
            if(rc==0){ vm_push_frame(&d->frame); nvf++; break; }
            VmDyn* a = (VmDyn*)g_frame_stack;
            vm_progv_restore(a);
            vm_pop_frame(); nvf--;
            vm_unwind_next_frame(&a->frame);
        }
        case BC_PROGV_UNBIND:{                      /* normal exit: restore all + pop */
            VmDyn* a = (VmDyn*)g_frame_stack;
            vm_progv_restore(a);
            vm_pop_frame(); nvf--; break; }
        case BC_UNWIND_PROTECT:{
            /* operand: svar offset to the cleanup code.  Establish a protect frame; the
               cleanup runs on BOTH normal completion (BC_UNWIND_NORMAL) and unwind. */
            int64_t off = vm_read_svar(code,&pc);
            size_t cleanup_pc = (size_t)((int64_t)pc + off);
            if(nvf >= VM_DYN_MAX){ ERROR("VM: too many nested dynamic frames%s",""); }
            VmDyn* d = &vdyn[nvf];
            d->frame.type = frame_protect;
            d->frame.results = results;
            d->kind = VMK_PROTECT;
            d->handler_pc = cleanup_pc;
            d->handler_sp = sp;                    /* base, before the protected form */
            d->unwinding = 0;
            int rc = setjmp(d->frame.non_local_exit);
            if(rc==0){ vm_push_frame(&d->frame); nvf++; break; }   /* run the protected form */
            /* unwinding through: run the cleanup (still on the frame stack), then let
               BC_UNWIND_END continue the cascade.  frame.target was set by the cascade.
               Save the in-flight values (staged for the ultimate target) so the cleanup
               code — which writes the shared MV buffer — cannot clobber them. */
            VmDyn* a = (VmDyn*)g_frame_stack;
            a->unwinding = 1;
            a->saved = *results;
            sp = a->handler_sp; pc = a->handler_pc;
            break;
        }
        case BC_UNWIND_NORMAL:{
            /* protected form completed normally (value on stack, MV in results): save the
               values, drop the frame (its cleanup now runs unprotected), fall to cleanup. */
            VmDyn* a = &vdyn[nvf-1];
            a->unwinding = 0;
            a->saved = *results;
            vm_pop_frame();                        /* nvf kept: slot holds saved + reserves space */
            sp = a->handler_sp;                    /* discard the protected value (it is saved) */
            break;
        }
        case BC_UNWIND_END:{
            VmDyn* a = &vdyn[nvf-1];
            if(a->unwinding){                      /* unwind path: pop + continue the cascade */
                *results = a->saved;               /* restore the in-flight values for the target */
                vm_pop_frame(); nvf--;
                vm_unwind_next_frame(&a->frame);   /* _Noreturn */
            }
            *results = a->saved;                   /* normal path: restore the protected values */
            Ref primary = (a->saved.count>0) ? a->saved.values[0] : NIL_Symbol;
            nvf--;
            PUSH(primary);
            break;
        }
        case BC_THROW:{
            /* stack: [... tag value-primary]; results holds the value's MV. */
            (void)POP();                            /* value primary (also results[0]) */
            Ref tag = POP();
            Frame* target = vm_find_catch_frame(tag);
            if(target==NULL){ ERROR("THROW: no CATCH is active for the thrown tag%s",""); }
            if(target->results != results){ *(target->results) = *results; }
            Frame* top = g_frame_stack;
            if(top==NULL){ ERROR("THROW: %s","no active frame"); }
            top->target = target;
            longjmp(top->non_local_exit,
                    (top==target)?(int)non_local_exit_target:(int)non_local_exit_unwind);
        }
        case BC_RETURN:{ Ref v = (sp>0) ? TOP() : NIL_Symbol;
                         if(results->count<=1 || results->values[0]!=v){ results->count=1; results->values[0]=v; }
                         goto done; }
        case BC_NOP:  break;
        case BC_HALT: goto done;
        default: ERROR("VM: unimplemented opcode %d (control/closure ops are vm-5/6)", (int)op);
        }
    }
    done:
    #undef PUSH
    #undef POP
    #undef TOP
    close_live_pool(pool);}
