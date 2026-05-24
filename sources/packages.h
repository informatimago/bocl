#ifndef packages_h
#define packages_h
#include "kernel.h"

/* Phase 3 minimal package system. All symbols share KERNEL_obarray (no
   per-package isolation yet); each Package keeps a list of the symbols
   currently registered as "interned in" it. The standard packages are
   COMMON-LISP (alias CL), COMMON-LISP-USER (alias CL-USER), and KEYWORD.
   *PACKAGE* defaults to CL-USER. */

extern Ref COMMON_LISP_Package;
extern Ref COMMON_LISP_USER_Package;
extern Ref KEYWORD_Package;

/* The single global registry of named packages (alist of name → package). */
extern Ref all_packages;

/* Initialise the package subsystem (called from kernel_initialize). */
void kernel_initialize_packages(void);

Ref package_new(const char* name, const char* const* nicknames);

/* Find a package by string-or-symbol designator. NIL if not found. */
Ref package_find(Ref designator);

/* Register sym as "interned in" pkg (idempotent). */
void package_add_symbol(Ref pkg, Ref sym);

/* Mark sym as external in pkg (idempotent; symbol must be already interned). */
void package_export(Ref pkg, Ref sym);

/* Return the current value of *PACKAGE*. */
Ref current_package(void);

#endif
