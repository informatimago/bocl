#ifndef hash_table_h
#define hash_table_h
#include "kernel.h"

/* Phase 4 minimal hash-table implementation. Alist-backed (per design)
   with the four standard test functions (EQ / EQL / EQUAL / EQUALP).
   No buckets, no rehashing — O(n) lookup, fine until ansi-test cares.

   Plus EQ and EQUALP as kernel built-ins (the existing kernel has EQL
   and EQUAL but not EQ — pointer equality — or EQUALP). */

void kernel_initialize_hash_tables(void);

bool eq(Ref a, Ref b);
bool equalp(Ref a, Ref b);

Ref hash_table_new(Ref test);

#endif
