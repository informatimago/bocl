#include <string.h>
#include "packages.h"
#include "kernel.h"
#include "cons.h"
#include "variable.h"

/* Object_new is defined in kernel.c but not declared in any header. */
extern Ref Object_new(Ref class, Slots* slots);

Ref COMMON_LISP_Package      = NULL;
Ref COMMON_LISP_USER_Package = NULL;
Ref KEYWORD_Package          = NULL;
Ref BOCL_Package             = NULL;
Ref all_packages             = NULL;

/* ------------------------------------------------------------------ */
/*  Constructor + registry                                              */
/* ------------------------------------------------------------------ */

static Package* Package_slots(Ref name, Ref nicknames){
    /* present/external are obarrays (cons sentinels + SymHash index); see
       kernel.c make_obarray.  Fill the reference slots before setting objcount
       so a GC during make_obarray never sees uninitialised slots. */
    Package* that = allocate(sizeof(*that));
    that->name      = name;
    that->nicknames = nicknames;
    that->present   = make_obarray();
    that->external  = make_obarray();
    that->shadowing = NIL_Symbol;
    that->used      = NIL_Symbol;
    that->used_by   = NIL_Symbol;
    that->objcount  = 7;
    return that;}

static Ref make_package(Ref name, Ref nicknames){
    /* Package_slots/Object_new allocate (the struct, two obarrays, the wrapper)
       while NAME and NICKNAMES are only C arguments — unrooted, so a GC during
       those allocations would free them before they are linked into the package.
       Root them across the build. */
    uword pool = open_live_pool();
    add_to_live_pool(name); add_to_live_pool(nicknames);
    Ref r = Object_new(PACKAGE_Class, (Slots*)Package_slots(name, nicknames));
    close_live_pool(pool);
    return r;}

/* Prepend a (name . pkg) entry to the package registry.  all_packages is a
   permanent sentinel cons (see kernel_initialize_packages): its identity never
   changes, so it stays reachable from the GC rootset across reassignments, and
   we add entries by mutating its CDR in place.  This is what keeps the registry
   alive through GC — without it, FIND-PACKAGE / LIST-ALL-PACKAGES lose every
   package the first time the collector runs. */
static void registry_add(Ref name, Ref pkg){
    /* cons() does not root its arguments (it is too hot), so protect NAME/PKG
       across the first cons and the fresh PAIR across the second — otherwise a GC
       fired by either allocation frees the not-yet-linked cell, leaving a dangling
       (name . pkg) entry in all_packages that FIND-PACKAGE later dereferences. */
    uword pool = open_live_pool();
    add_to_live_pool(name); add_to_live_pool(pkg);
    Ref pair = cons(name, pkg);
    add_to_live_pool(pair);
    rplacd(all_packages, cons(pair, cons_cdr(all_packages)));
    close_live_pool(pool);}

Ref package_new(const char* name, const char* const* nicknames){
    Ref nn = NIL_Symbol;
    if(nicknames){
        for(int i = 0; nicknames[i]; i++){
            push(string_new_cstring(nicknames[i]), &nn);}
        nn = nreverse(nn);}
    Ref pkg = make_package(string_new_cstring(name), nn);
    /* Register pkg in the global all-packages alist (name → pkg, and each nickname → pkg). */
    registry_add(string_new_cstring(name), pkg);
    if(nicknames){
        for(int i = 0; nicknames[i]; i++){
            registry_add(string_new_cstring(nicknames[i]), pkg);}}
    return pkg;}

/* ------------------------------------------------------------------ */
/*  Lookups                                                             */
/* ------------------------------------------------------------------ */

/* The string named by a CL string designator: a string is itself, a symbol
   gives its symbol-name, a character gives a one-character string.  Anything
   else is a type error.  (Package functions take string designators.) */
static Ref string_designator_name(Ref designator){
    if(typep(designator, STRING_Class)){ return designator; }
    if(symbolp(designator))            { return symbol_name(designator); }
    if(characterp(designator)){
        char buf[2];
        buf[0] = (char)(octet)integer_value(character_code(designator));
        buf[1] = '\0';
        return string_new_cstring(buf);}
    TYPE_ERROR(designator, STRING_Class);
    return NIL_Symbol;   /* unreachable: TYPE_ERROR does not return */
}

Ref package_find(Ref designator){
    /* Accept STRING / SYMBOL / CHARACTER name designators, or a PACKAGE. */
    if(typep(designator, PACKAGE_Class)){ return designator; }
    Ref name = string_designator_name(designator);
    Ref entry = cons_cdr(all_packages);
    while(consp(entry)){
        Ref pair = cons_car(entry);
        if(string_equal(cons_car(pair), name)){
            return cons_cdr(pair);}
        entry = cons_cdr(entry);}
    return NIL_Symbol;}

void package_add_symbol(Ref pkg, Ref sym){
    check_type(pkg, PACKAGE_Class);
    obarray_add_symbol(pkg->slots->package.present, sym);}

void package_export(Ref pkg, Ref sym){
    check_type(pkg, PACKAGE_Class);
    obarray_add_symbol(pkg->slots->package.present, sym);   /* must be present */
    obarray_add_symbol(pkg->slots->package.external, sym);}

/* Core lookup: find NAME (a STRING) accessible in PKG, following the use-list.
   Sets *status to the keyword :EXTERNAL/:INTERNAL/:INHERITED or NIL, and returns
   the symbol (or NIL when not found). */
static Ref pkg_find_symbol(Ref name, Ref pkg, Ref* status){
    Ref s = find_symbol(name, pkg->slots->package.external);
    if(s){ *status = K(EXTERNAL); return s; }
    s = find_symbol(name, pkg->slots->package.present);
    if(s){ *status = K(INTERNAL); return s; }
    Ref used = pkg->slots->package.used;
    while(consp(used)){
        Ref u = cons_car(used);
        s = find_symbol(name, u->slots->package.external);
        if(s){ *status = K(INHERITED); return s; }
        used = cons_cdr(used);}
    *status = NIL_Symbol;
    return NIL_Symbol;}

/* INTERN: find-or-create NAME in PKG.  *status is the accessibility of an
   EXISTING symbol, or NIL when a fresh symbol was created (CL semantics). */
static Ref pkg_intern(Ref name, Ref pkg, Ref* status){
    Ref s = pkg_find_symbol(name, pkg, status);
    if(*status != NIL_Symbol){ return s; }
    if(pkg == KEYWORD_Package){
        /* keyword_intern installs the keyword (KEYWORD_obarray, self-evaluating);
           KEYWORD's present/external both ARE KEYWORD_obarray, so it's external. */
        s = keyword_intern(name);
        *status = NIL_Symbol;
        return s;}
    s = symbol_new(name);                       /* fresh uninterned symbol */
    s->slots->symbol.package = pkg;             /* home package */
    obarray_add_symbol(pkg->slots->package.present, s);
    *status = NIL_Symbol;
    return s;}

Ref current_package(void){
    Ref p = symbol_value(S(*PACKAGE*));
    /* If somehow not bound or not a package, fall back to CL-USER. */
    if(!typep(p, PACKAGE_Class)){ return COMMON_LISP_USER_Package; }
    return p;}

/* ---- Reader support: bare and package-qualified symbol resolution -------- */

Ref package_find_cstring(const char* name){
    return package_find(string_new_cstring(name));}

/* In each of these, the fresh name STRING must be rooted across the call: it is
   reachable only through this C local, yet current_package() / pkg_intern() /
   find_symbol() all allocate (interning a symbol, building the obarray index) and
   a GC there would free the name before it is hashed/stored. */
Ref package_intern_current_cstring(const char* name){
    Ref status;
    uword pool=open_live_pool();
    Ref s=string_new_cstring(name);
    add_to_live_pool(s);
    Ref pkg=current_package();
    add_to_live_pool(pkg);
    Ref result=pkg_intern(s, pkg, &status);
    close_live_pool(pool);
    return result;}

Ref package_intern_cstring_in(const char* name, Ref pkg){
    Ref status;
    uword pool=open_live_pool();
    Ref s=string_new_cstring(name);
    add_to_live_pool(s);
    add_to_live_pool(pkg);
    Ref result=pkg_intern(s, pkg, &status);
    close_live_pool(pool);
    return result;}

/* Returns the external symbol named NAME in PKG, or C NULL if there is none.
   NULL (not NIL_Symbol) is the not-found sentinel precisely so that finding the
   symbol NIL — e.g. reading CL:NIL — is distinguishable from finding nothing. */
Ref package_find_external_cstring(const char* name, Ref pkg){
    uword pool=open_live_pool();
    Ref s=string_new_cstring(name);
    add_to_live_pool(s);
    add_to_live_pool(pkg);
    Ref found = find_symbol(s, pkg->slots->package.external);
    close_live_pool(pool);
    return found;}

/* Printer support: decide how SYMBOL must be qualified when written (prin1) with
   *PACKAGE* in force.  Sets *package_name_out to the home package's name STRING
   when a prefix is needed (else NIL) and *sep to:
     0  no prefix — the symbol is accessible as itself in *PACKAGE* (or keyword)
     1  pkg:name  — external in its home package
     2  pkg::name — internal in its home package
     3  #:name    — uninterned (no home package)
   No allocation, so it is safe to call from the printer. */
void symbol_print_prefix(Ref symbol, Ref* package_name_out, int* sep){
    *package_name_out = NIL_Symbol;
    *sep = 0;
    Ref home = symbol->slots->symbol.package;
    if(!typep(home, PACKAGE_Class)){ *sep = 3; return; }   /* uninterned */
    if(home == KEYWORD_Package){ *sep = 0; return; }       /* caller prints ':' */
    Ref name = symbol_name(symbol);
    Ref status;
    Ref accessible = pkg_find_symbol(name, current_package(), &status);
    if(accessible == symbol){ *sep = 0; return; }          /* accessible unqualified */
    *package_name_out = home->slots->package.name;
    Ref ext = find_symbol(name, home->slots->package.external);
    *sep = (ext == symbol) ? 1 : 2;}

/* ------------------------------------------------------------------ */
/*  Kernel built-ins                                                    */
/* ------------------------------------------------------------------ */

static void kernel_FIND_PACKAGE(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref designator = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = package_find(designator);
    close_live_pool(pool);}

static void kernel_PACKAGE_NAME(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref pkg = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    Ref p = package_find(pkg);
    if(nullp(p)){ ERROR("PACKAGE-NAME: no such package %s",
                        string_cstring(prin1_to_string(pkg))); }
    results->count = 1;
    results->values[0] = p->slots->package.name;
    close_live_pool(pool);}

static void kernel_PACKAGE_NICKNAMES(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref pkg = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    Ref p = package_find(pkg);
    if(nullp(p)){ ERROR("PACKAGE-NICKNAMES: no such package %s",
                        string_cstring(prin1_to_string(pkg))); }
    results->count = 1;
    results->values[0] = p->slots->package.nicknames;
    close_live_pool(pool);}

static void kernel_MAKE_PACKAGE(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref name_designator = pop_mandatory_argument(function, &current);
    Ref nicknames = pop_key_argument(function, &current, K(NICKNAMES), NIL_Symbol);
    Ref use = pop_key_argument(function, &current, K(USE), NIL_Symbol);
    Ref name = string_designator_name(name_designator);
    /* Coerce nicknames entries (string designators) to strings. */
    Ref clean_nn = NIL_Symbol;
    Ref it = nicknames;
    while(consp(it)){
        push(string_designator_name(cons_car(it)), &clean_nn);
        it = cons_cdr(it);}
    clean_nn = nreverse(clean_nn);
    /* NAME and the freshly-built CLEAN_NN list must survive make_package and the
       registry_add loop below, all of which allocate; neither is reachable from a
       root otherwise (CLEAN_NN is fresh; NAME may be a fresh coerced string). */
    add_to_live_pool(name); add_to_live_pool(clean_nn);
    Ref pkg = make_package(name, clean_nn);
    add_to_live_pool(pkg);
    registry_add(name, pkg);
    it = clean_nn;
    while(consp(it)){
        registry_add(cons_car(it), pkg);
        it = cons_cdr(it);}
    /* :use list — each entry is a package designator. */
    it = use;
    while(consp(it)){
        Ref u = package_find(cons_car(it));
        if(nullp(u)){ ERROR("MAKE-PACKAGE: unknown package in :use %s",
                            string_cstring(prin1_to_string(cons_car(it)))); }
        pkg->slots->package.used = cons(u, pkg->slots->package.used);
        gc_write_barrier(pkg->slots);
        u->slots->package.used_by = cons(pkg, u->slots->package.used_by);
        gc_write_barrier(u->slots);
        it = cons_cdr(it);}
    results->count = 1;
    results->values[0] = pkg;
    close_live_pool(pool);}

/* Add a single package (designator) to IN_PKG's use list. */
static void use_one_package(Ref to_use_des, Ref in_pkg){
    Ref to_use = package_find(to_use_des);
    if(nullp(to_use)){ ERROR("USE-PACKAGE: no such package %s",
                             string_cstring(prin1_to_string(to_use_des))); }
    if(nullp(memq(to_use, in_pkg->slots->package.used))){
        in_pkg->slots->package.used = cons(to_use, in_pkg->slots->package.used);
        gc_write_barrier(in_pkg->slots);
        to_use->slots->package.used_by = cons(in_pkg, to_use->slots->package.used_by);
        gc_write_barrier(to_use->slots);}}

static void kernel_USE_PACKAGE(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref to_use_des = pop_mandatory_argument(function, &current);
    Ref in_pkg_des = nullp(current) ? current_package() : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    Ref in_pkg = package_find(in_pkg_des);
    if(nullp(in_pkg)){ ERROR("USE-PACKAGE: no such package %s",
                             string_cstring(prin1_to_string(in_pkg_des))); }
    /* packages-to-use is a list of designators OR a single designator. */
    if(consp(to_use_des)){
        Ref it = to_use_des;
        while(consp(it)){ use_one_package(cons_car(it), in_pkg); it = cons_cdr(it); }
    } else if(!nullp(to_use_des)){
        use_one_package(to_use_des, in_pkg);}
    results->count = 1;
    results->values[0] = T_Symbol;
    close_live_pool(pool);}

static void kernel_EXPORT(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref syms_arg = pop_mandatory_argument(function, &current);
    Ref pkg_des = nullp(current) ? current_package() : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    Ref pkg = package_find(pkg_des);
    if(nullp(pkg)){ ERROR("EXPORT: no such package %s",
                          string_cstring(prin1_to_string(pkg_des))); }
    /* syms_arg may be a single symbol or a list of symbols. */
    if(symbolp(syms_arg)){
        package_export(pkg, syms_arg);
    } else {
        Ref it = syms_arg;
        while(consp(it)){
            Ref s = cons_car(it);
            if(!symbolp(s)){ TYPE_ERROR(s, SYMBOL_Class); }
            package_export(pkg, s);
            it = cons_cdr(it);}}
    results->count = 1;
    results->values[0] = T_Symbol;
    close_live_pool(pool);}

/* (%EXPORT-STRINGS names-list package) — intern each STRING in NAMES into the
   package and make it external.  Uses only kernel features (quote + strings) so
   the generated cl-exports.lisp can run BEFORE the prelude, ensuring the
   standard CL symbols are already external when the prelude reads cl:FOO. */
static void kernel_EXPORT_STRINGS(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref names   = pop_mandatory_argument(function, &current);
    Ref pkg_des = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    Ref pkg = package_find(pkg_des);
    if(nullp(pkg)){ ERROR("%%EXPORT-STRINGS: no such package %s",
                          string_cstring(prin1_to_string(pkg_des))); }
    Ref it = names;
    while(consp(it)){
        Ref nm = cons_car(it);
        if(typep(nm, STRING_Class)){
            Ref status;
            Ref sym = pkg_intern(nm, pkg, &status);
            package_export(pkg, sym);}
        it = cons_cdr(it);}
    results->count = 1;
    results->values[0] = T_Symbol;
    close_live_pool(pool);}

static void kp_INTERN(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref name = pop_mandatory_argument(function, &current);
    Ref pkg_des = nullp(current) ? current_package() : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    name = string_designator_name(name);   /* accept string / symbol / character */
    Ref pkg = package_find(pkg_des);
    if(nullp(pkg)){ ERROR("INTERN: no such package %s",
                          string_cstring(prin1_to_string(pkg_des))); }
    Ref status;
    Ref sym = pkg_intern(name, pkg, &status);
    results->count = 2;
    results->values[0] = sym;
    results->values[1] = status;
    close_live_pool(pool);}

static void kp_FIND_SYMBOL(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref name = pop_mandatory_argument(function, &current);
    Ref pkg_des = nullp(current) ? current_package() : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    name = string_designator_name(name);   /* accept string / symbol / character */
    Ref pkg = package_find(pkg_des);
    if(nullp(pkg)){ ERROR("FIND-SYMBOL: no such package %s",
                          string_cstring(prin1_to_string(pkg_des))); }
    Ref status;
    Ref sym = pkg_find_symbol(name, pkg, &status);
    results->count = 2;
    results->values[0] = sym;
    results->values[1] = status;
    close_live_pool(pool);}

static void kernel_PACKAGE(Ref function, Ref arguments, MultipleValues* results){
    /* (PACKAGEP obj) — a useful one-off. */
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref obj = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = as_boolean(typep(obj, PACKAGE_Class));
    close_live_pool(pool);}

static void kernel_PACKAGE_SYMBOLS(Ref function, Ref arguments, MultipleValues* results){
    /* (PACKAGE-SYMBOLS package-designator) → list of all interned symbols */
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref pkg_des = nullp(current) ? current_package() : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    Ref pkg = package_find(pkg_des);
    if(nullp(pkg)){ ERROR("PACKAGE-SYMBOLS: no such package %s",
                          string_cstring(prin1_to_string(pkg_des))); }
    results->count = 1;
    results->values[0] = cons_cdr(pkg->slots->package.present);
    close_live_pool(pool);}

static void kernel_PACKAGE_EXPORTED_SYMBOLS(Ref function, Ref arguments, MultipleValues* results){
    /* (PACKAGE-EXPORTED-SYMBOLS package-designator) → list of exported symbols */
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref pkg_des = nullp(current) ? current_package() : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    Ref pkg = package_find(pkg_des);
    if(nullp(pkg)){ ERROR("PACKAGE-EXPORTED-SYMBOLS: no such package %s",
                          string_cstring(prin1_to_string(pkg_des))); }
    results->count = 1;
    results->values[0] = cons_cdr(pkg->slots->package.external);
    close_live_pool(pool);}

static void kernel_LIST_ALL_PACKAGES(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    check_no_more_arguments(function, arguments);
    /* all_packages is name→pkg; dedupe to just the pkg list. */
    Ref pkgs = NIL_Symbol;
    Ref it = cons_cdr(all_packages);
    while(consp(it)){
        Ref pkg = cons_cdr(cons_car(it));
        if(nullp(memq(pkg, pkgs))){
            pkgs = cons(pkg, pkgs);}
        it = cons_cdr(it);}
    results->count = 1;
    results->values[0] = pkgs;
    close_live_pool(pool);}

/* ------------------------------------------------------------------ */
/*  Low-level table primitives for the high-level operators.  The Lisp  */
/*  layer (import/shadow/unintern/... in the prelude) builds on these.  */
/* ------------------------------------------------------------------ */

/* Pop (symbol package) — package defaults to *PACKAGE* — and resolve package. */
static void pop_sym_pkg(Ref function, Ref* current, Ref* sym, Ref* pkg){
    *sym = pop_mandatory_argument(function, current);
    Ref pkg_des = nullp(*current) ? current_package()
                                  : pop_mandatory_argument(function, current);
    check_no_more_arguments(function, *current);
    *pkg = package_find(pkg_des);
    if(nullp(*pkg)){ ERROR("package operator: no such package %s",
                           string_cstring(prin1_to_string(pkg_des))); }}

/* Pop a single optional package designator (defaults to *PACKAGE*). */
static Ref pop_one_pkg(Ref function, Ref arguments){
    Ref current=arguments;
    Ref pkg_des = nullp(current) ? current_package() : pop_mandatory_argument(function,&current);
    check_no_more_arguments(function, current);
    Ref pkg = package_find(pkg_des);
    if(nullp(pkg)){ ERROR("package operator: no such package %s",
                          string_cstring(prin1_to_string(pkg_des))); }
    return pkg;}

/* (%PKG-IMPORT symbol package) — make symbol present (accessible) in package. */
static void kp_PKG_IMPORT(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments, sym, pkg; pop_sym_pkg(function,&current,&sym,&pkg);
    obarray_add_symbol(pkg->slots->package.present, sym);
    /* IMPORT: if the symbol has no home package, PACKAGE becomes its home (CLHS). */
    if(nullp(sym->slots->symbol.package)){
        sym->slots->symbol.package = pkg;
        gc_write_barrier(sym->slots);}
    results->count=1; results->values[0]=T_Symbol; close_live_pool(pool);}

/* (%PKG-MAKE-PRESENT name-string package) — ensure a symbol of NAME is PRESENT
   (home) in package, creating it if no present symbol of that name exists,
   ignoring any inherited symbol of the same name.  This is what SHADOW needs:
   plain INTERN would return an inherited symbol instead of making a local one. */
static void kp_PKG_MAKE_PRESENT(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref name = pop_mandatory_argument(function,&current);
    Ref pkg_des = nullp(current) ? current_package() : pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    if(!typep(name,STRING_Class)){ TYPE_ERROR(name,STRING_Class); }
    Ref pkg=package_find(pkg_des);
    if(nullp(pkg)){ ERROR("SHADOW: no such package%s",""); }
    Ref sym = find_symbol(name, pkg->slots->package.present);
    if(!sym){
        sym = symbol_new(name);
        sym->slots->symbol.package = pkg;
        obarray_add_symbol(pkg->slots->package.present, sym);}
    results->count=1; results->values[0]=sym; close_live_pool(pool);}

/* (%PKG-UNINTERN symbol package) — remove symbol from present/external/shadowing. */
static void kp_PKG_UNINTERN(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments, sym, pkg; pop_sym_pkg(function,&current,&sym,&pkg);
    bool removed = obarray_remove_symbol(pkg->slots->package.present, sym);
    obarray_remove_symbol(pkg->slots->package.external, sym);
    pkg->slots->package.shadowing = delete(sym, pkg->slots->package.shadowing);
    gc_write_barrier(pkg->slots);
    /* If PKG was the symbol's home package, the home becomes NIL (CLHS UNINTERN). */
    if(sym->slots->symbol.package == pkg){
        sym->slots->symbol.package = NIL_Symbol;
        gc_write_barrier(sym->slots);}
    results->count=1; results->values[0]=as_boolean(removed); close_live_pool(pool);}

/* (%PKG-UNEXPORT symbol package) — remove symbol from the external table. */
static void kp_PKG_UNEXPORT(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments, sym, pkg; pop_sym_pkg(function,&current,&sym,&pkg);
    obarray_remove_symbol(pkg->slots->package.external, sym);
    results->count=1; results->values[0]=T_Symbol; close_live_pool(pool);}

/* (%PKG-SHADOW-ADD symbol package) — record symbol as shadowing in package. */
static void kp_PKG_SHADOW_ADD(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments, sym, pkg; pop_sym_pkg(function,&current,&sym,&pkg);
    if(nullp(memq(sym, pkg->slots->package.shadowing))){
        pkg->slots->package.shadowing = cons(sym, pkg->slots->package.shadowing);
        gc_write_barrier(pkg->slots);}
    results->count=1; results->values[0]=T_Symbol; close_live_pool(pool);}

static void kp_PKG_SHADOWING_SYMBOLS(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref pkg=pop_one_pkg(function,arguments);
    results->count=1; results->values[0]=pkg->slots->package.shadowing; close_live_pool(pool);}

static void kp_PKG_USE_LIST(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref pkg=pop_one_pkg(function,arguments);
    results->count=1; results->values[0]=pkg->slots->package.used; close_live_pool(pool);}

static void kp_PKG_USED_BY_LIST(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref pkg=pop_one_pkg(function,arguments);
    results->count=1; results->values[0]=pkg->slots->package.used_by; close_live_pool(pool);}

/* (%PKG-UNUSE used-package package) — drop used from package's use-list. */
static void kp_PKG_UNUSE(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref used_des = pop_mandatory_argument(function,&current);
    Ref pkg_des  = nullp(current) ? current_package() : pop_mandatory_argument(function,&current);
    check_no_more_arguments(function, current);
    Ref used=package_find(used_des), pkg=package_find(pkg_des);
    if(nullp(used)||nullp(pkg)){ ERROR("UNUSE-PACKAGE: no such package%s",""); }
    pkg->slots->package.used     = delete(used, pkg->slots->package.used);
    gc_write_barrier(pkg->slots);
    used->slots->package.used_by = delete(pkg,  used->slots->package.used_by);
    gc_write_barrier(used->slots);
    results->count=1; results->values[0]=T_Symbol; close_live_pool(pool);}

/* (%PKG-RENAME package new-name nicknames) — update name + nicknames + registry. */
static void kp_PKG_RENAME(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref pkg_des   = pop_mandatory_argument(function,&current);
    Ref new_name  = pop_mandatory_argument(function,&current);
    Ref nicknames = nullp(current) ? NIL_Symbol : pop_mandatory_argument(function,&current);
    check_no_more_arguments(function, current);
    Ref pkg=package_find(pkg_des);
    if(nullp(pkg)){ ERROR("RENAME-PACKAGE: no such package%s",""); }
    if(symbolp(new_name)){ new_name=symbol_name(new_name); }
    /* Drop old name+nickname registry entries pointing at pkg, then re-add. */
    Ref kept=NIL_Symbol, it=cons_cdr(all_packages);
    while(consp(it)){ Ref pair=cons_car(it);
        if(cons_cdr(pair)!=pkg){ push(pair,&kept); } it=cons_cdr(it); }
    rplacd(all_packages, nreverse(kept));
    pkg->slots->package.name=new_name;
    pkg->slots->package.nicknames=nicknames;
    gc_write_barrier(pkg->slots);
    registry_add(new_name, pkg);
    it=nicknames; while(consp(it)){ Ref n=cons_car(it);
        if(symbolp(n)){ n=symbol_name(n); } registry_add(n,pkg); it=cons_cdr(it); }
    results->count=1; results->values[0]=pkg; close_live_pool(pool);}

/* (%PKG-DELETE package) — unregister package and empty its tables. */
static void kp_PKG_DELETE(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref pkg=pop_one_pkg(function,arguments);
    Ref kept=NIL_Symbol, it=cons_cdr(all_packages);
    while(consp(it)){ Ref pair=cons_car(it);
        if(cons_cdr(pair)!=pkg){ push(pair,&kept); } it=cons_cdr(it); }
    rplacd(all_packages, nreverse(kept));
    /* Drop this package from every package it uses (their used-by lists). */
    Ref u=pkg->slots->package.used;
    while(consp(u)){ Ref used=cons_car(u);
        used->slots->package.used_by=delete(pkg,used->slots->package.used_by);
        gc_write_barrier(used->slots); u=cons_cdr(u); }
    pkg->slots->package.present=make_obarray();
    pkg->slots->package.external=make_obarray();
    pkg->slots->package.shadowing=NIL_Symbol;
    pkg->slots->package.used=NIL_Symbol;
    pkg->slots->package.used_by=NIL_Symbol;
    pkg->slots->package.name=NIL_Symbol;     /* deleted package: name is NIL */
    gc_write_barrier(pkg->slots);
    results->count=1; results->values[0]=T_Symbol; close_live_pool(pool);}

static void register_fn(Ref fname, Ref lambda_list, KernelFunctionPtr fn){
    set_symbol_function(fname, built_in_function_new(fname, lambda_list, fn));}

void kernel_initialize_packages(void){
    /* Permanent sentinel header: entries live in its CDR.  Its constant
       identity lets the GC rootset (kernel_initialize_rootset) keep the whole
       registry alive even as packages are added at run time. */
    all_packages = cons(NIL_Symbol, NIL_Symbol);

    static const char* cl_nicknames[]      = { "CL",      NULL };
    static const char* cl_user_nicknames[] = { "CL-USER", NULL };
    COMMON_LISP_Package      = package_new("COMMON-LISP",      cl_nicknames);
    COMMON_LISP_USER_Package = package_new("COMMON-LISP-USER", cl_user_nicknames);
    KEYWORD_Package          = package_new("KEYWORD",          NULL);
    BOCL_Package             = package_new("BOCL",             NULL);

    /* The kernel interns every CL symbol into KERNEL_obarray, so that obarray IS
       COMMON-LISP's present-table.  (Its external subset is populated later, in
       the prelude, by EXPORTing the standard CL symbols.)  Every keyword lives
       in KEYWORD_obarray and is external, so KEYWORD's present and external
       tables both ARE that obarray. */
    {   extern Ref KERNEL_obarray; extern Ref KEYWORD_obarray;
        COMMON_LISP_Package->slots->package.present = KERNEL_obarray;
        gc_write_barrier(COMMON_LISP_Package->slots);
        KEYWORD_Package->slots->package.present  = KEYWORD_obarray;
        KEYWORD_Package->slots->package.external = KEYWORD_obarray;
        gc_write_barrier(KEYWORD_Package->slots);}

    /* CL-USER uses CL (and is used-by recorded on CL). */
    COMMON_LISP_USER_Package->slots->package.used =
        cons(COMMON_LISP_Package, COMMON_LISP_USER_Package->slots->package.used);
    gc_write_barrier(COMMON_LISP_USER_Package->slots);
    COMMON_LISP_Package->slots->package.used_by =
        cons(COMMON_LISP_USER_Package, COMMON_LISP_Package->slots->package.used_by);
    /* BOCL (the implementation-extensions package) uses CL too. */
    BOCL_Package->slots->package.used =
        cons(COMMON_LISP_Package, BOCL_Package->slots->package.used);
    gc_write_barrier(BOCL_Package->slots);
    COMMON_LISP_Package->slots->package.used_by =
        cons(BOCL_Package, COMMON_LISP_Package->slots->package.used_by);
    gc_write_barrier(COMMON_LISP_Package->slots);

    /* *PACKAGE* defaults to CL-USER. */
    define_parameter(S(*PACKAGE*), COMMON_LISP_USER_Package);

    /* Register kernel-visible built-ins. Lambda lists use simple positional
       descriptors; the &optional package designator is handled inline. */
    register_fn(S(FIND-PACKAGE),       list(S(NAME),NULL),                          kernel_FIND_PACKAGE);
    register_fn(S(PACKAGE-NAME),       list(S(PACKAGE),NULL),                       kernel_PACKAGE_NAME);
    register_fn(S(PACKAGE-NICKNAMES),  list(S(PACKAGE),NULL),                       kernel_PACKAGE_NICKNAMES);
    register_fn(S(MAKE-PACKAGE),       list(S(NAME),NULL),                          kernel_MAKE_PACKAGE);
    register_fn(S(USE-PACKAGE),        list(S(PACKAGES-TO-USE),NULL),               kernel_USE_PACKAGE);
    register_fn(S(EXPORT),             list(S(SYMBOLS),NULL),                       kernel_EXPORT);
    register_fn(S(%EXPORT-STRINGS),    list(S(NAMES),S(PACKAGE),NULL),             kernel_EXPORT_STRINGS);
    register_fn(S(INTERN),             list(S(NAME),NULL),                          kp_INTERN);
    register_fn(S(FIND-SYMBOL),        list(S(NAME),NULL),                          kp_FIND_SYMBOL);
    register_fn(S(PACKAGEP),                    list(S(OBJECT),NULL),  kernel_PACKAGE);
    register_fn(S(LIST-ALL-PACKAGES),           NIL_Symbol,            kernel_LIST_ALL_PACKAGES);
    register_fn(S(PACKAGE-SYMBOLS),             list(S(PACKAGE),NULL), kernel_PACKAGE_SYMBOLS);
    register_fn(S(PACKAGE-EXPORTED-SYMBOLS),    list(S(PACKAGE),NULL), kernel_PACKAGE_EXPORTED_SYMBOLS);
    /* Low-level table primitives — the Lisp layer wraps these. */
    register_fn(S(%PKG-IMPORT),            list(S(SYMBOL),NULL),          kp_PKG_IMPORT);
    register_fn(S(%PKG-MAKE-PRESENT),      list(S(NAME),NULL),            kp_PKG_MAKE_PRESENT);
    register_fn(S(%PKG-UNINTERN),          list(S(SYMBOL),NULL),          kp_PKG_UNINTERN);
    register_fn(S(%PKG-UNEXPORT),          list(S(SYMBOL),NULL),          kp_PKG_UNEXPORT);
    register_fn(S(%PKG-SHADOW-ADD),        list(S(SYMBOL),NULL),          kp_PKG_SHADOW_ADD);
    register_fn(S(PACKAGE-SHADOWING-SYMBOLS), list(S(PACKAGE),NULL),      kp_PKG_SHADOWING_SYMBOLS);
    register_fn(S(PACKAGE-USE-LIST),       list(S(PACKAGE),NULL),         kp_PKG_USE_LIST);
    register_fn(S(PACKAGE-USED-BY-LIST),   list(S(PACKAGE),NULL),         kp_PKG_USED_BY_LIST);
    register_fn(S(%PKG-UNUSE),             list(S(USED),NULL),            kp_PKG_UNUSE);
    register_fn(S(%PKG-RENAME),            list(S(PACKAGE),NULL),         kp_PKG_RENAME);
    register_fn(S(%PKG-DELETE),            list(S(PACKAGE),NULL),         kp_PKG_DELETE);

    /* Fix up the home package of every symbol interned before the package
       system existed: those created during early bootstrap got KERNEL_obarray /
       KEYWORD_obarray as a placeholder home.  (Symbols interned after this point
       get the right home package from symbol_intern_obarray directly.) */
    {   extern Ref KERNEL_obarray, KEYWORD_obarray;
        Ref it=cons_cdr(KERNEL_obarray);
        while(consp(it)){ Ref s=cons_car(it); s->slots->symbol.package=COMMON_LISP_Package;
            gc_write_barrier(s->slots); it=cons_cdr(it); }
        it=cons_cdr(KEYWORD_obarray);
        while(consp(it)){ Ref s=cons_car(it); s->slots->symbol.package=KEYWORD_Package;
            gc_write_barrier(s->slots); it=cons_cdr(it); }}
}
