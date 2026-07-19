#ifndef arithmetic_h
#define arithmetic_h
#include "kernel.h"

/* Phase 2 numeric primitives.  All registered at startup by
   kernel_initialize_arithmetic().  Each is a KernelFunctionPtr. */

void kernel_initialize_arithmetic(void);

/* ---- binary entry points for the byte-code VM inline arithmetic ops --------------- *
 * (vm-inline-primitives) The SAME numeric-tower core the variadic +,-,*,<,= builtins  *
 * fold over, exposed so BC_ADD2/SUB2/MUL2/LT2/GT2/LE2/GE2/NUM_EQ2 can compute a 2-arg  *
 * op without building an arg list.  Full semantics preserved: bignum promotion,        *
 * ratios, floats, complex, TYPE-ERROR on non-numbers, division-by-zero.  OP is one of  *
 * '+' '-' '*' '/'.  num_lt/num_eq are the pairwise < / = predicates (num_le = !num_lt  *
 * (b,a), num_gt = num_lt(b,a), num_ge = !num_lt(a,b)).                                  */
Ref  num_combine(char op, Ref a, Ref b);
bool num_lt(Ref a, Ref b);
bool num_eq(Ref a, Ref b);

/* ---- arithmetic --------------------------------------------------------- */
void kernel_PLUS           (Ref function, Ref arguments, MultipleValues* results);
void kernel_TIMES          (Ref function, Ref arguments, MultipleValues* results);
void kernel_MINUS          (Ref function, Ref arguments, MultipleValues* results);
void kernel_DIVIDE         (Ref function, Ref arguments, MultipleValues* results);

/* ---- comparisons -------------------------------------------------------- */
void kernel_NUM_EQ         (Ref function, Ref arguments, MultipleValues* results);
void kernel_LT             (Ref function, Ref arguments, MultipleValues* results);
void kernel_GT             (Ref function, Ref arguments, MultipleValues* results);
void kernel_LE             (Ref function, Ref arguments, MultipleValues* results);
void kernel_GE             (Ref function, Ref arguments, MultipleValues* results);
void kernel_NUM_NEQ        (Ref function, Ref arguments, MultipleValues* results);

/* ---- predicates --------------------------------------------------------- */
void kernel_ZEROP          (Ref function, Ref arguments, MultipleValues* results);
void kernel_PLUSP          (Ref function, Ref arguments, MultipleValues* results);
void kernel_MINUSP         (Ref function, Ref arguments, MultipleValues* results);
void kernel_ODDP           (Ref function, Ref arguments, MultipleValues* results);
void kernel_EVENP          (Ref function, Ref arguments, MultipleValues* results);

/* ---- single-arg arithmetic --------------------------------------------- */
void kernel_ONE_PLUS       (Ref function, Ref arguments, MultipleValues* results);
void kernel_ONE_MINUS      (Ref function, Ref arguments, MultipleValues* results);
void kernel_ABS            (Ref function, Ref arguments, MultipleValues* results);
void kernel_MIN            (Ref function, Ref arguments, MultipleValues* results);
void kernel_MAX            (Ref function, Ref arguments, MultipleValues* results);
void kernel_SIGNUM         (Ref function, Ref arguments, MultipleValues* results);

/* ---- integer division --------------------------------------------------- */
void kernel_TRUNCATE       (Ref function, Ref arguments, MultipleValues* results);
void kernel_FLOOR          (Ref function, Ref arguments, MultipleValues* results);
void kernel_CEILING        (Ref function, Ref arguments, MultipleValues* results);
void kernel_ROUND          (Ref function, Ref arguments, MultipleValues* results);
void kernel_MOD            (Ref function, Ref arguments, MultipleValues* results);
void kernel_REM            (Ref function, Ref arguments, MultipleValues* results);

/* ---- bitwise / shift ---------------------------------------------------- */
void kernel_LOGAND         (Ref function, Ref arguments, MultipleValues* results);
void kernel_LOGOR          (Ref function, Ref arguments, MultipleValues* results);
void kernel_LOGXOR         (Ref function, Ref arguments, MultipleValues* results);
void kernel_LOGNOT         (Ref function, Ref arguments, MultipleValues* results);
void kernel_ASH            (Ref function, Ref arguments, MultipleValues* results);
void kernel_INTEGER_LENGTH (Ref function, Ref arguments, MultipleValues* results);
void kernel_LOGCOUNT       (Ref function, Ref arguments, MultipleValues* results);

/* ---- coercion ----------------------------------------------------------- */
void kernel_FLOAT          (Ref function, Ref arguments, MultipleValues* results);
void kernel_INTEGER        (Ref function, Ref arguments, MultipleValues* results);
void kernel_COERCE         (Ref function, Ref arguments, MultipleValues* results);

/* ---- powers / roots ----------------------------------------------------- */
void kernel_EXPT           (Ref function, Ref arguments, MultipleValues* results);
void kernel_SQRT           (Ref function, Ref arguments, MultipleValues* results);
void kernel_ISQRT          (Ref function, Ref arguments, MultipleValues* results);
void kernel_GCD            (Ref function, Ref arguments, MultipleValues* results);
void kernel_LCM            (Ref function, Ref arguments, MultipleValues* results);

/* ---- transcendentals ---------------------------------------------------- */
void kernel_EXP            (Ref function, Ref arguments, MultipleValues* results);
void kernel_LOG            (Ref function, Ref arguments, MultipleValues* results);
void kernel_SIN            (Ref function, Ref arguments, MultipleValues* results);
void kernel_COS            (Ref function, Ref arguments, MultipleValues* results);
void kernel_TAN            (Ref function, Ref arguments, MultipleValues* results);
void kernel_ASIN           (Ref function, Ref arguments, MultipleValues* results);
void kernel_ACOS           (Ref function, Ref arguments, MultipleValues* results);
void kernel_ATAN           (Ref function, Ref arguments, MultipleValues* results);

/* ---- Slice 20: float introspection + LOGBITP ----------------------------- */
void kernel_DECODE_FLOAT        (Ref function, Ref arguments, MultipleValues* results);
void kernel_INTEGER_DECODE_FLOAT(Ref function, Ref arguments, MultipleValues* results);
void kernel_FLOAT_RADIX         (Ref function, Ref arguments, MultipleValues* results);
void kernel_FLOAT_DIGITS        (Ref function, Ref arguments, MultipleValues* results);
void kernel_FLOAT_PRECISION     (Ref function, Ref arguments, MultipleValues* results);
void kernel_FLOAT_SIGN          (Ref function, Ref arguments, MultipleValues* results);
void kernel_SCALE_FLOAT         (Ref function, Ref arguments, MultipleValues* results);
void kernel_LOGBITP             (Ref function, Ref arguments, MultipleValues* results);

#endif
