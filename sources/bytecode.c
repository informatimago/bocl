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

extern Ref Object_new(Ref class, Slots* slots);   /* defined in kernel.c (as in reader.c) */

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
        case BC_VALUES:{ uint8_t n = code[pc++];   /* minimal MV: primary only for vm-2 */
                         Ref v0 = (n>0) ? S[sp-1] : NIL_Symbol; sp -= n; PUSH(v0); break; }
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
