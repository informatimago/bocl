#include <math.h>
#include <stdbool.h>
#include "arithmetic.h"
#include "kernel.h"
#include "cons.h"

/* -------------------------------------------------------------------- *
 *  Helpers                                                              *
 * -------------------------------------------------------------------- */

/* Coerce a Ref to floating, accepting INTEGER or FLOAT. */
static floating as_floating(Ref x){
    if(integerp(x)){ return (floating)integer_value(x); }
    if(floatp(x))  { return float_value(x); }
    TYPE_ERROR(x,NUMBER_Class);}

/* Check that x is a NUMBER (INTEGER or FLOAT). */
static void check_number(Ref x){
    if(!(integerp(x)||floatp(x))){
        TYPE_ERROR(x,NUMBER_Class);}}

/* Promote two numbers: return true iff at least one is a float; the integer
   values are written into *iout (only valid if returns false). */
static bool either_float(Ref a, Ref b){
    return floatp(a) || floatp(b);}

/* -------------------------------------------------------------------- *
 *  Variadic arithmetic: + - * /                                         *
 *                                                                       *
 *  Mixed int/float promotes to float. int+int detects overflow via the  *
 *  __builtin_*_overflow intrinsics and signals ARITHMETIC_ERROR.        *
 *                                                                       *
 *  Per CL: (+) -> 0, (*) -> 1, (- x) -> -x, (/ x) -> 1/x,                *
 *  (- a b c) -> a-b-c, (/ a b c) -> a/b/c.                              *
 * -------------------------------------------------------------------- */

/* Combine two numbers with a binary op (+, -, *, / coded as a single char). */
static Ref num_combine(char op, Ref a, Ref b){
    check_number(a); check_number(b);
    if(either_float(a,b)){
        floating av=as_floating(a), bv=as_floating(b), r=0.0;
        switch(op){
          case '+': r = av + bv; break;
          case '-': r = av - bv; break;
          case '*': r = av * bv; break;
          case '/':
              if(bv==0.0){ ARITHMETIC_ERROR("Division by zero%s",""); }
              r = av / bv; break;
          default: FATAL("num_combine: unknown op %c",op);}
        return float_from_floating(r);
    } else {
        word av=integer_value(a), bv=integer_value(b), r=0;
        switch(op){
          case '+':
              if(__builtin_add_overflow(av,bv,&r)){
                  ARITHMETIC_ERROR("Integer overflow in (+ %"WORD_FORMAT" %"WORD_FORMAT")",av,bv);}
              break;
          case '-':
              if(__builtin_sub_overflow(av,bv,&r)){
                  ARITHMETIC_ERROR("Integer overflow in (- %"WORD_FORMAT" %"WORD_FORMAT")",av,bv);}
              break;
          case '*':
              if(__builtin_mul_overflow(av,bv,&r)){
                  ARITHMETIC_ERROR("Integer overflow in (* %"WORD_FORMAT" %"WORD_FORMAT")",av,bv);}
              break;
          case '/':
              if(bv==0){ ARITHMETIC_ERROR("Division by zero%s",""); }
              if(av%bv != 0){
                  /* Inexact integer division promotes to float (no ratios yet). */
                  return float_from_floating((floating)av/(floating)bv);}
              if(av==WORD_MIN && bv==-1){
                  ARITHMETIC_ERROR("Integer overflow in (/ %"WORD_FORMAT" -1)",av);}
              r = av / bv;
              break;
          default: FATAL("num_combine: unknown op %c",op);}
        return integer_from_word(r);}}

void kernel_PLUS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref acc=I(0);
    add_to_live_pool(acc);
    Ref tail=arguments;
    if(consp(tail)){ acc=cons_car(tail); check_number(acc); tail=cons_cdr(tail); }
    while(consp(tail)){ acc=num_combine('+',acc,cons_car(tail)); tail=cons_cdr(tail); }
    results->count=1; results->values[0]=acc;
    close_live_pool(pool);}

void kernel_TIMES(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref acc=I(1);
    add_to_live_pool(acc);
    Ref tail=arguments;
    if(consp(tail)){ acc=cons_car(tail); check_number(acc); tail=cons_cdr(tail); }
    while(consp(tail)){ acc=num_combine('*',acc,cons_car(tail)); tail=cons_cdr(tail); }
    results->count=1; results->values[0]=acc;
    close_live_pool(pool);}

void kernel_MINUS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref tail=arguments;
    if(!consp(tail)){ ERROR("(-) needs at least one argument%s",""); }
    Ref acc=cons_car(tail); check_number(acc); tail=cons_cdr(tail);
    if(!consp(tail)){
        /* (- x) == (- 0 x) */
        acc=num_combine('-',I(0),acc);
    } else {
        while(consp(tail)){ acc=num_combine('-',acc,cons_car(tail)); tail=cons_cdr(tail); }}
    results->count=1; results->values[0]=acc;
    close_live_pool(pool);}

void kernel_DIVIDE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref tail=arguments;
    if(!consp(tail)){ ERROR("(/) needs at least one argument%s",""); }
    Ref acc=cons_car(tail); check_number(acc); tail=cons_cdr(tail);
    if(!consp(tail)){
        /* (/ x) == (/ 1 x) */
        acc=num_combine('/',I(1),acc);
    } else {
        while(consp(tail)){ acc=num_combine('/',acc,cons_car(tail)); tail=cons_cdr(tail); }}
    results->count=1; results->values[0]=acc;
    close_live_pool(pool);}

/* -------------------------------------------------------------------- *
 *  Comparisons: =  /=  <  >  <=  >=                                     *
 * -------------------------------------------------------------------- */

static bool num_eq(Ref a, Ref b){
    check_number(a); check_number(b);
    return either_float(a,b)
            ? as_floating(a)==as_floating(b)
            : integer_value(a)==integer_value(b);}

static bool num_lt(Ref a, Ref b){
    check_number(a); check_number(b);
    return either_float(a,b)
            ? as_floating(a)<as_floating(b)
            : integer_value(a)<integer_value(b);}

/* Variadic monotonic comparator: (cmp a b c) ≡ (and (cmp a b) (cmp b c)).
   pairp(prev,curr) is the predicate the user picked (e.g. num_lt for <). */
typedef bool (*num_pair_pred)(Ref,Ref);

static Ref num_cmp_monotonic(Ref function,Ref arguments,num_pair_pred pairp){
    Ref tail=arguments;
    if(!consp(tail)){
        ERROR("%s needs at least one argument",
              string_cstring(symbol_name(function_name(function))));}
    Ref prev=cons_car(tail); check_number(prev); tail=cons_cdr(tail);
    while(consp(tail)){
        Ref curr=cons_car(tail); check_number(curr);
        if(!pairp(prev,curr)){ return NIL_Symbol; }
        prev=curr; tail=cons_cdr(tail);}
    return T_Symbol;}

/* (/= a b c …) is NOT monotonic — it requires every pair distinct. */
static Ref num_neq_all_distinct(Ref function,Ref arguments){
    if(!consp(arguments)){
        ERROR("%s needs at least one argument",
              string_cstring(symbol_name(function_name(function))));}
    /* All pairwise comparison: O(n²) is fine for the small n at the REPL. */
    Ref outer=arguments;
    while(consp(outer)){
        Ref a=cons_car(outer); check_number(a);
        Ref inner=cons_cdr(outer);
        while(consp(inner)){
            Ref b=cons_car(inner); check_number(b);
            if(num_eq(a,b)){ return NIL_Symbol; }
            inner=cons_cdr(inner);}
        outer=cons_cdr(outer);}
    return T_Symbol;}

static bool num_le_pred(Ref a, Ref b){ return !num_lt(b,a); }
static bool num_gt_pred(Ref a, Ref b){ return  num_lt(b,a); }
static bool num_ge_pred(Ref a, Ref b){ return !num_lt(a,b); }

#define ARITH_CMPN(cname,predfn)                                    \
    void cname(Ref function,Ref arguments,MultipleValues* results){ \
        uword pool=open_live_pool();                                \
        add_to_live_pool(function); add_to_live_pool(arguments);    \
        results->count=1;                                           \
        results->values[0]=num_cmp_monotonic(function,arguments,predfn); \
        close_live_pool(pool);}

ARITH_CMPN(kernel_NUM_EQ, num_eq)
ARITH_CMPN(kernel_LT,     num_lt)
ARITH_CMPN(kernel_GT,     num_gt_pred)
ARITH_CMPN(kernel_LE,     num_le_pred)
ARITH_CMPN(kernel_GE,     num_ge_pred)

void kernel_NUM_NEQ(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    results->count=1;
    results->values[0]=num_neq_all_distinct(function,arguments);
    close_live_pool(pool);}

/* -------------------------------------------------------------------- *
 *  Predicates: zerop  plusp  minusp  oddp  evenp                        *
 * -------------------------------------------------------------------- */

#define ARITH_PRED1(cname,exprx)                                    \
    void cname(Ref function,Ref arguments,MultipleValues* results){ \
        uword pool=open_live_pool();                                \
        add_to_live_pool(function);                                 \
        add_to_live_pool(arguments);                                \
        Ref current=arguments;                                      \
        Ref x=pop_mandatory_argument(function,&current);            \
        check_no_more_arguments(function,current);                  \
        check_number(x);                                            \
        results->count=1;                                           \
        results->values[0]=as_boolean(exprx);                       \
        close_live_pool(pool);}

ARITH_PRED1(kernel_ZEROP,  (floatp(x)?float_value(x)==0.0:integer_value(x)==0))
ARITH_PRED1(kernel_PLUSP,  (floatp(x)?float_value(x)>0.0 :integer_value(x)>0))
ARITH_PRED1(kernel_MINUSP, (floatp(x)?float_value(x)<0.0 :integer_value(x)<0))

/* ODDP and EVENP are defined only for integers. */
void kernel_ODDP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    if(!integerp(x)){ TYPE_ERROR(x,INTEGER_Class); }
    results->count=1;
    results->values[0]=as_boolean((integer_value(x)&1)!=0);
    close_live_pool(pool);}

void kernel_EVENP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    if(!integerp(x)){ TYPE_ERROR(x,INTEGER_Class); }
    results->count=1;
    results->values[0]=as_boolean((integer_value(x)&1)==0);
    close_live_pool(pool);}

/* -------------------------------------------------------------------- *
 *  1+ 1- ABS MIN MAX                                                    *
 * -------------------------------------------------------------------- */

void kernel_ONE_PLUS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    check_number(x);
    results->count=1;
    if(floatp(x)){
        results->values[0]=float_from_floating(float_value(x)+1.0);
    } else {
        word r;
        if(__builtin_add_overflow(integer_value(x),(word)1,&r)){
            ARITHMETIC_ERROR("Integer overflow in (1+ %"WORD_FORMAT")",integer_value(x));}
        results->values[0]=integer_from_word(r);}
    close_live_pool(pool);}

void kernel_ONE_MINUS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    check_number(x);
    results->count=1;
    if(floatp(x)){
        results->values[0]=float_from_floating(float_value(x)-1.0);
    } else {
        word r;
        if(__builtin_sub_overflow(integer_value(x),(word)1,&r)){
            ARITHMETIC_ERROR("Integer overflow in (1- %"WORD_FORMAT")",integer_value(x));}
        results->values[0]=integer_from_word(r);}
    close_live_pool(pool);}

void kernel_ABS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    check_number(x);
    results->count=1;
    if(floatp(x)){
        results->values[0]=float_from_floating(fabs(float_value(x)));
    } else {
        word v=integer_value(x);
        if(v==WORD_MIN){
            ARITHMETIC_ERROR("Integer overflow in (abs %"WORD_FORMAT")",v);}
        results->values[0]=integer_from_word(v<0?-v:v);}
    close_live_pool(pool);}

/* Variadic MIN and MAX: at least one argument; folded by the strict predicate. */
void kernel_MIN(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref tail=arguments;
    if(!consp(tail)){ ERROR("(min) needs at least one argument%s",""); }
    Ref best=cons_car(tail); check_number(best); tail=cons_cdr(tail);
    while(consp(tail)){
        Ref next=cons_car(tail); check_number(next);
        if(num_lt(next,best)){ best=next; }
        tail=cons_cdr(tail);}
    results->count=1; results->values[0]=best;
    close_live_pool(pool);}

void kernel_MAX(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref tail=arguments;
    if(!consp(tail)){ ERROR("(max) needs at least one argument%s",""); }
    Ref best=cons_car(tail); check_number(best); tail=cons_cdr(tail);
    while(consp(tail)){
        Ref next=cons_car(tail); check_number(next);
        if(num_lt(best,next)){ best=next; }
        tail=cons_cdr(tail);}
    results->count=1; results->values[0]=best;
    close_live_pool(pool);}

/* -------------------------------------------------------------------- *
 *  Integer division family: TRUNCATE FLOOR CEILING ROUND MOD REM        *
 *                                                                       *
 *  TRUNCATE/FLOOR/CEILING/ROUND return TWO values per CLHS:             *
 *  (quotient remainder). MOD and REM return one value.                  *
 *                                                                       *
 *  Single-arg form treats divisor as 1.                                 *
 * -------------------------------------------------------------------- */

/* Result of integer trunc/floor/ceil/round with two integer args. */
typedef struct { word q; word r; } intdiv_t;

static intdiv_t int_truncate(word num, word den){
    intdiv_t out;
    out.q = num/den;             /* C99 truncates toward zero */
    out.r = num - out.q*den;
    return out;}

static intdiv_t int_floor(word num, word den){
    intdiv_t out = int_truncate(num,den);
    if(out.r!=0 && ((out.r<0)!=(den<0))){
        out.q -= 1;
        out.r += den;}
    return out;}

static intdiv_t int_ceiling(word num, word den){
    intdiv_t out = int_truncate(num,den);
    if(out.r!=0 && ((out.r<0)==(den<0))){
        out.q += 1;
        out.r -= den;}
    return out;}

static intdiv_t int_round(word num, word den){
    /* Banker's rounding: ties to even. */
    intdiv_t out = int_truncate(num,den);
    word twice_r = out.r * 2;
    word abs_den = den<0?-den:den;
    word abs_twice_r = twice_r<0?-twice_r:twice_r;
    if(abs_twice_r > abs_den
       || (abs_twice_r == abs_den && (out.q & 1))){
        if((out.r<0)==(den<0)){ out.q += 1; out.r -= den; }
        else                  { out.q -= 1; out.r += den; }}
    return out;}

#define DIVOP_KERNEL(cname,intop,floatop)                                      \
    void cname(Ref function,Ref arguments,MultipleValues* results){            \
        uword pool=open_live_pool();                                           \
        add_to_live_pool(function);                                            \
        add_to_live_pool(arguments);                                           \
        Ref current=arguments;                                                 \
        Ref num=pop_mandatory_argument(function,&current);                     \
        Ref den=nullp(current)?I(1):pop_mandatory_argument(function,&current); \
        check_no_more_arguments(function,current);                             \
        check_number(num); check_number(den);                                  \
        if(either_float(num,den)){                                             \
            floating n=as_floating(num);                                       \
            floating d=as_floating(den);                                       \
            if(d==0.0){ ARITHMETIC_ERROR("Division by zero%s",""); }           \
            floating q=floatop(n/d);                                           \
            results->count=2;                                                  \
            results->values[0]=integer_from_word((word)q);                     \
            results->values[1]=float_from_floating(n - q*d);                   \
        } else {                                                               \
            word n=integer_value(num); word d=integer_value(den);              \
            if(d==0){ ARITHMETIC_ERROR("Division by zero%s",""); }             \
            if(n==WORD_MIN && d==-1){                                          \
                ARITHMETIC_ERROR("Integer overflow in division by -1%s",""); } \
            intdiv_t r=intop(n,d);                                             \
            results->count=2;                                                  \
            results->values[0]=integer_from_word(r.q);                         \
            results->values[1]=integer_from_word(r.r); }                       \
        close_live_pool(pool);}

DIVOP_KERNEL(kernel_TRUNCATE, int_truncate, trunc)
DIVOP_KERNEL(kernel_FLOOR,    int_floor,    floor)
DIVOP_KERNEL(kernel_CEILING,  int_ceiling,  ceil)
DIVOP_KERNEL(kernel_ROUND,    int_round,    rint)

void kernel_MOD(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref num=pop_mandatory_argument(function,&current);
    Ref den=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    check_number(num); check_number(den);
    results->count=1;
    if(either_float(num,den)){
        floating n=as_floating(num), d=as_floating(den);
        if(d==0.0){ ARITHMETIC_ERROR("Division by zero%s",""); }
        floating q=floor(n/d);
        results->values[0]=float_from_floating(n - q*d);
    } else {
        word n=integer_value(num), d=integer_value(den);
        if(d==0){ ARITHMETIC_ERROR("Division by zero%s",""); }
        intdiv_t r=int_floor(n,d);
        results->values[0]=integer_from_word(r.r);}
    close_live_pool(pool);}

void kernel_REM(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref num=pop_mandatory_argument(function,&current);
    Ref den=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    check_number(num); check_number(den);
    results->count=1;
    if(either_float(num,den)){
        floating n=as_floating(num), d=as_floating(den);
        if(d==0.0){ ARITHMETIC_ERROR("Division by zero%s",""); }
        floating q=trunc(n/d);
        results->values[0]=float_from_floating(n - q*d);
    } else {
        word n=integer_value(num), d=integer_value(den);
        if(d==0){ ARITHMETIC_ERROR("Division by zero%s",""); }
        intdiv_t r=int_truncate(n,d);
        results->values[0]=integer_from_word(r.r);}
    close_live_pool(pool);}

/* -------------------------------------------------------------------- *
 *  Bitwise: LOGAND LOGOR LOGXOR LOGNOT ASH INTEGER-LENGTH               *
 * -------------------------------------------------------------------- */

#define LOG_OP_2(cname,op)                                                  \
    void cname(Ref function,Ref arguments,MultipleValues* results){         \
        uword pool=open_live_pool();                                        \
        add_to_live_pool(function); add_to_live_pool(arguments);            \
        Ref current=arguments;                                              \
        Ref a=pop_mandatory_argument(function,&current);                    \
        Ref b=pop_mandatory_argument(function,&current);                    \
        check_no_more_arguments(function,current);                          \
        if(!integerp(a)){ TYPE_ERROR(a,INTEGER_Class); }                    \
        if(!integerp(b)){ TYPE_ERROR(b,INTEGER_Class); }                    \
        results->count=1;                                                   \
        results->values[0]=integer_from_word(                               \
            (word)((uword)integer_value(a) op (uword)integer_value(b)));    \
        close_live_pool(pool);}

LOG_OP_2(kernel_LOGAND, &)
LOG_OP_2(kernel_LOGOR,  |)
LOG_OP_2(kernel_LOGXOR, ^)

void kernel_LOGNOT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref a=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    if(!integerp(a)){ TYPE_ERROR(a,INTEGER_Class); }
    results->count=1;
    results->values[0]=integer_from_word(~integer_value(a));
    close_live_pool(pool);}

/* (ASH integer count) — shift left if count is positive, right if negative.
   Right-shift on signed words is implementation-defined in C; on every host
   we care about (x86/ARM, gcc and clang) it is arithmetic, which is what CL
   wants. We refuse counts ≥ word-width to avoid UB. */
void kernel_ASH(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref a=pop_mandatory_argument(function,&current);
    Ref n=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    if(!integerp(a)){ TYPE_ERROR(a,INTEGER_Class); }
    if(!integerp(n)){ TYPE_ERROR(n,INTEGER_Class); }
    word v=integer_value(a);
    word k=integer_value(n);
    results->count=1;
    if(k==0){
        results->values[0]=a;
    } else if(k>0){
        if(k >= (word)(8*sizeof(word))){
            results->values[0]=I(0);
        } else {
            /* Left shift: detect overflow by reverse op. */
            uword shifted = (uword)v << k;
            results->values[0]=integer_from_word((word)shifted);}
    } else {
        word rk=-k;
        if(rk >= (word)(8*sizeof(word))){
            results->values[0]=I(v<0?-1:0);
        } else {
            results->values[0]=integer_from_word(v >> rk);}}
    close_live_pool(pool);}

/* (INTEGER-LENGTH n) — number of bits needed to represent |n| (excl. sign). */
void kernel_INTEGER_LENGTH(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref a=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    if(!integerp(a)){ TYPE_ERROR(a,INTEGER_Class); }
    word v=integer_value(a);
    uword u = (v<0) ? (uword)(~v) : (uword)v;
    word len=0;
    while(u){ len++; u >>= 1; }
    results->count=1;
    results->values[0]=integer_from_word(len);
    close_live_pool(pool);}

/* -------------------------------------------------------------------- *
 *  Coercion: FLOAT  INTEGER (truncate-to-int)                           *
 *  Plus a narrow COERCE that accepts the two number classes.            *
 * -------------------------------------------------------------------- */

/* (FLOAT number) — return number coerced to a float. */
void kernel_FLOAT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x=pop_mandatory_argument(function,&current);
    /* CL allows (float x prototype) — ignore prototype, single float anyway. */
    if(consp(current)){ pop_mandatory_argument(function,&current); }
    check_no_more_arguments(function,current);
    check_number(x);
    results->count=1;
    results->values[0]=floatp(x)?x:float_from_floating(as_floating(x));
    close_live_pool(pool);}

/* (TRUNCATE-TO-INTEGER x) — coerce to integer by truncation. This is the
   kernel helper used by the Lisp side's (COERCE x 'INTEGER). */
void kernel_INTEGER(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    check_number(x);
    results->count=1;
    if(integerp(x)){
        results->values[0]=x;
    } else {
        floating v=float_value(x);
        if(v >= (floating)WORD_MAX || v <= (floating)WORD_MIN){
            ARITHMETIC_ERROR("Float %g does not fit in a kernel integer",v);}
        results->values[0]=integer_from_word((word)trunc(v));}
    close_live_pool(pool);}

/* A narrow COERCE: only the number classes for now. The full CL COERCE
   handles sequence/character/symbol cases and belongs on the Lisp side. */
void kernel_COERCE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref obj=pop_mandatory_argument(function,&current);
    Ref result_type=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    if(eql(result_type,S(FLOAT)) || eql(result_type,S(SINGLE-FLOAT))
       || eql(result_type,S(DOUBLE-FLOAT)) || eql(result_type,S(SHORT-FLOAT))
       || eql(result_type,S(LONG-FLOAT))){
        check_number(obj);
        results->values[0]=floatp(obj)?obj:float_from_floating(as_floating(obj));
    } else if(eql(result_type,S(INTEGER)) || eql(result_type,S(FIXNUM))){
        if(integerp(obj)){
            results->values[0]=obj;
        } else if(floatp(obj)){
            floating v=float_value(obj);
            if(v != trunc(v)){
                ARITHMETIC_ERROR("Cannot exactly coerce %g to integer",v);}
            results->values[0]=integer_from_word((word)v);
        } else {
            TYPE_ERROR(obj,NUMBER_Class);}
    } else if(eql(result_type,S(NUMBER))){
        check_number(obj);
        results->values[0]=obj;
    } else {
        ERROR("COERCE: result type %s not supported by the kernel",
              string_cstring(prin1_to_string(result_type)));}
    close_live_pool(pool);}

/* -------------------------------------------------------------------- *
 *  Slice 6 numeric extras.                                              *
 *                                                                       *
 *  EXPT, SQRT, ISQRT, GCD, LCM, SIGNUM, LOGCOUNT, and the elementary    *
 *  float transcendentals (EXP, LOG, SIN, COS, TAN, ATAN). These live    *
 *  in the kernel because they need libm or 64-bit integer math we'd    *
 *  rather not implement in Lisp.                                        *
 * -------------------------------------------------------------------- */

/* (EXPT base power) — integer^integer stays integer when possible.
   Anything involving a float, or a negative integer power, promotes to
   float and uses libm's pow(). */
void kernel_EXPT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref base = pop_mandatory_argument(function,&current);
    Ref expo = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    check_number(base); check_number(expo);
    results->count=1;
    if(integerp(base) && integerp(expo) && integer_value(expo) >= 0){
        /* Pure-integer fast path with overflow checking. */
        word b = integer_value(base);
        word e = integer_value(expo);
        word r = 1;
        while(e > 0){
            if(e & 1){
                if(__builtin_mul_overflow(r,b,&r)){
                    ARITHMETIC_ERROR("Integer overflow in (EXPT %"WORD_FORMAT" %"WORD_FORMAT")",
                                     integer_value(base), integer_value(expo));}}
            e >>= 1;
            if(e){
                if(__builtin_mul_overflow(b,b,&b)){
                    ARITHMETIC_ERROR("Integer overflow in (EXPT %"WORD_FORMAT" %"WORD_FORMAT")",
                                     integer_value(base), integer_value(expo));}}}
        results->values[0] = integer_from_word(r);
    } else {
        floating r = pow(as_floating(base), as_floating(expo));
        results->values[0] = float_from_floating(r);}
    close_live_pool(pool);}

/* (SQRT x) — always returns a float. */
void kernel_SQRT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    check_number(x);
    floating v = as_floating(x);
    if(v < 0){ ARITHMETIC_ERROR("SQRT of negative number %g (complex not supported)",v); }
    results->count=1;
    results->values[0] = float_from_floating(sqrt(v));
    close_live_pool(pool);}

/* (ISQRT n) — integer square root: largest k with k*k <= n.  n must be a
   non-negative integer. */
void kernel_ISQRT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref n = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    if(!integerp(n)){ TYPE_ERROR(n,INTEGER_Class); }
    word v = integer_value(n);
    if(v < 0){ ARITHMETIC_ERROR("ISQRT: argument %"WORD_FORMAT" is negative",v); }
    /* Newton's method; converges in <= ~32 iterations for 64-bit. */
    word x = v;
    word y = (x + 1) / 2;
    while(y < x){ x = y; y = (x + v / x) / 2; }
    results->count=1;
    results->values[0] = integer_from_word(x);
    close_live_pool(pool);}

/* Helpers for GCD / LCM. */
static word abs_word(word x){ return x < 0 ? -x : x; }
static word gcd2(word a, word b){
    a = abs_word(a); b = abs_word(b);
    while(b){ word t = a % b; a = b; b = t; }
    return a;}

/* (GCD &rest integers) — greatest common divisor. (gcd) -> 0. */
void kernel_GCD(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref tail = arguments;
    word acc = 0;
    while(consp(tail)){
        Ref x = cons_car(tail);
        if(!integerp(x)){ TYPE_ERROR(x,INTEGER_Class); }
        acc = gcd2(acc, integer_value(x));
        tail = cons_cdr(tail);}
    results->count=1;
    results->values[0] = integer_from_word(acc);
    close_live_pool(pool);}

/* (LCM &rest integers) — least common multiple. (lcm) -> 1. */
void kernel_LCM(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref tail = arguments;
    word acc = 1;
    while(consp(tail)){
        Ref x = cons_car(tail);
        if(!integerp(x)){ TYPE_ERROR(x,INTEGER_Class); }
        word v = integer_value(x);
        if(v == 0){ acc = 0; break; }
        word g = gcd2(acc, v);
        word q = v / g;
        word r = 0;
        if(__builtin_mul_overflow(acc,q,&r)){
            ARITHMETIC_ERROR("Integer overflow in LCM%s","");}
        acc = abs_word(r);
        tail = cons_cdr(tail);}
    results->count=1;
    results->values[0] = integer_from_word(acc);
    close_live_pool(pool);}

/* (SIGNUM x) — -1, 0, +1 of the same numeric type as x. */
void kernel_SIGNUM(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    check_number(x);
    results->count=1;
    if(integerp(x)){
        word v = integer_value(x);
        results->values[0] = integer_from_word(v < 0 ? -1 : v > 0 ? 1 : 0);
    } else {
        floating v = float_value(x);
        floating s = v < 0.0 ? -1.0 : v > 0.0 ? 1.0 : 0.0;
        results->values[0] = float_from_floating(s);}
    close_live_pool(pool);}

/* (LOGCOUNT integer) — population count of |x| (CL ignores sign). */
void kernel_LOGCOUNT(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    if(!integerp(x)){ TYPE_ERROR(x,INTEGER_Class); }
    word v = integer_value(x);
    /* For negative x, CL says (logcount -1) is 0, (logcount -2) is 1 etc:
       count the zero bits of the equivalent twos-complement. We treat that
       as counting bits set in ~x for negatives. */
    uword bits = v < 0 ? ~(uword)v : (uword)v;
    word n = (word)__builtin_popcountl(bits);
    results->count=1;
    results->values[0] = integer_from_word(n);
    close_live_pool(pool);}

/* Float transcendentals — each accepts any number and returns a float. */
#define FLOAT_TRANSCENDENTAL(NAME,CFUN)                                 \
    void kernel_##NAME(Ref function,Ref arguments,MultipleValues* results){ \
        uword pool=open_live_pool();                                    \
        add_to_live_pool(function); add_to_live_pool(arguments);        \
        Ref current=arguments;                                          \
        Ref x = pop_mandatory_argument(function,&current);              \
        check_no_more_arguments(function,current);                      \
        check_number(x);                                                \
        results->count=1;                                               \
        results->values[0] = float_from_floating(CFUN(as_floating(x))); \
        close_live_pool(pool);}

FLOAT_TRANSCENDENTAL(EXP, exp)
FLOAT_TRANSCENDENTAL(SIN, sin)
FLOAT_TRANSCENDENTAL(COS, cos)
FLOAT_TRANSCENDENTAL(TAN, tan)
FLOAT_TRANSCENDENTAL(ASIN, asin)
FLOAT_TRANSCENDENTAL(ACOS, acos)
#undef FLOAT_TRANSCENDENTAL

/* (LOG number &optional base) — natural log if no base; arbitrary base
   computed as log(x)/log(base). */
void kernel_LOG(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x = pop_mandatory_argument(function,&current);
    check_number(x);
    floating v = as_floating(x);
    if(v <= 0.0){ ARITHMETIC_ERROR("LOG: non-positive argument %g",v); }
    floating r = log(v);
    if(consp(current)){
        Ref b = pop_mandatory_argument(function,&current);
        check_number(b);
        floating bv = as_floating(b);
        if(bv <= 0.0 || bv == 1.0){
            ARITHMETIC_ERROR("LOG: invalid base %g",bv);}
        r = r / log(bv);}
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = float_from_floating(r);
    close_live_pool(pool);}

/* (ATAN y &optional x) — one-arg form is atan(y); two-arg is atan2(y,x). */
void kernel_ATAN(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref y = pop_mandatory_argument(function,&current);
    check_number(y);
    floating yv = as_floating(y);
    floating r;
    if(consp(current)){
        Ref x = pop_mandatory_argument(function,&current);
        check_number(x);
        r = atan2(yv, as_floating(x));
    } else {
        r = atan(yv);}
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0] = float_from_floating(r);
    close_live_pool(pool);}

/* -------------------------------------------------------------------- *
 *  Registration.                                                        *
 *                                                                       *
 *  Mirrors the auto-generated kernel_initialize_built_in_functions().   *
 *  Phase 2 hand-registers; once the generator is taught about these     *
 *  operators they can be folded into the generated file.                *
 * -------------------------------------------------------------------- */

static void register_fn(Ref fname, Ref lambda_list, KernelFunctionPtr fn){
    set_symbol_function(fname, built_in_function_new(fname,lambda_list,fn));}

void kernel_initialize_arithmetic(void){
    Ref ll_xy=list(S(X),S(Y),NULL);
    Ref ll_x=list(S(X),NULL);

    /* 2-arg arithmetic */
    register_fn(S(+), ll_xy, kernel_PLUS);
    register_fn(S(-), ll_xy, kernel_MINUS);
    register_fn(S(*), ll_xy, kernel_TIMES);
    register_fn(S(/), ll_xy, kernel_DIVIDE);

    /* Comparisons */
    register_fn(S(=),  ll_xy, kernel_NUM_EQ);
    register_fn(S(/=), ll_xy, kernel_NUM_NEQ);
    register_fn(S(<),  ll_xy, kernel_LT);
    register_fn(S(>),  ll_xy, kernel_GT);
    register_fn(S(<=), ll_xy, kernel_LE);
    register_fn(S(>=), ll_xy, kernel_GE);

    /* Predicates */
    register_fn(S(ZEROP),  ll_x, kernel_ZEROP);
    register_fn(S(PLUSP),  ll_x, kernel_PLUSP);
    register_fn(S(MINUSP), ll_x, kernel_MINUSP);
    register_fn(S(ODDP),   ll_x, kernel_ODDP);
    register_fn(S(EVENP),  ll_x, kernel_EVENP);

    /* Increments + magnitude + min/max */
    register_fn(S(1+),  ll_x,  kernel_ONE_PLUS);
    register_fn(S(1-),  ll_x,  kernel_ONE_MINUS);
    register_fn(S(ABS), ll_x,  kernel_ABS);
    register_fn(S(MIN), ll_xy, kernel_MIN);
    register_fn(S(MAX), ll_xy, kernel_MAX);

    /* Division-family: lambda list is (number &optional divisor) on the Lisp side.
       Until the &optional support is in the kernel we register them with the
       2-arg list; the kernel-side dispatch in this file handles the 1-arg case
       by defaulting current=NIL. */
    Ref ll_n_opt_d=list(S(NUMBER),S(DIVISOR),NULL);
    register_fn(S(TRUNCATE), ll_n_opt_d, kernel_TRUNCATE);
    register_fn(S(FLOOR),    ll_n_opt_d, kernel_FLOOR);
    register_fn(S(CEILING),  ll_n_opt_d, kernel_CEILING);
    register_fn(S(ROUND),    ll_n_opt_d, kernel_ROUND);
    register_fn(S(MOD),      ll_xy, kernel_MOD);
    register_fn(S(REM),      ll_xy, kernel_REM);

    /* Bitwise */
    register_fn(S(LOGAND),         ll_xy, kernel_LOGAND);
    register_fn(S(LOGOR),          ll_xy, kernel_LOGOR);  /* old MACLISP name */
    register_fn(S(LOGIOR),         ll_xy, kernel_LOGOR);  /* CL standard name */
    register_fn(S(LOGXOR),         ll_xy, kernel_LOGXOR);
    register_fn(S(LOGNOT),         ll_x,  kernel_LOGNOT);
    register_fn(S(ASH),            ll_xy, kernel_ASH);
    register_fn(S(INTEGER-LENGTH), ll_x,  kernel_INTEGER_LENGTH);

    /* Coercion */
    register_fn(S(FLOAT),   ll_x,  kernel_FLOAT);
    register_fn(S(INTEGER), ll_x,  kernel_INTEGER);
    register_fn(S(COERCE),  list(S(OBJECT),S(RESULT-TYPE),NULL), kernel_COERCE);

    /* Slice 6 numeric extras. */
    Ref ll_rest    = list(S(&REST),S(NUMBERS),NULL);
    Ref ll_n_opt_b = list(S(NUMBER),S(&OPTIONAL),S(BASE),NULL);
    Ref ll_y_opt_x = list(S(Y),S(&OPTIONAL),S(X),NULL);
    register_fn(S(EXPT),     list(S(BASE),S(POWER),NULL), kernel_EXPT);
    register_fn(S(SQRT),     ll_x,  kernel_SQRT);
    register_fn(S(ISQRT),    ll_x,  kernel_ISQRT);
    register_fn(S(GCD),      ll_rest, kernel_GCD);
    register_fn(S(LCM),      ll_rest, kernel_LCM);
    register_fn(S(SIGNUM),   ll_x,  kernel_SIGNUM);
    register_fn(S(LOGCOUNT), ll_x,  kernel_LOGCOUNT);
    register_fn(S(EXP),      ll_x,  kernel_EXP);
    register_fn(S(LOG),      ll_n_opt_b, kernel_LOG);
    register_fn(S(SIN),      ll_x,  kernel_SIN);
    register_fn(S(COS),      ll_x,  kernel_COS);
    register_fn(S(TAN),      ll_x,  kernel_TAN);
    register_fn(S(ASIN),     ll_x,  kernel_ASIN);
    register_fn(S(ACOS),     ll_x,  kernel_ACOS);
    register_fn(S(ATAN),     ll_y_opt_x, kernel_ATAN);
}
