/* tests/test_misc.c — CUnit tests for Slice 14 miscellaneous utilities:
   MAKE-ARRAY, array ops, COPY-SEQ, MAKE-LIST, RANDOM, file-system,
   timing constants, FILL, REPLACE, MISMATCH, and write-to-string fix. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "simple_test.h"
#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"
#include "memory.h"   /* open_live_pool/close_live_pool for the cfasl reader */
#include "cfasl.h"    /* cfasl-1: binary fasl byte/varint/float primitives */
#include "bytecode.h" /* vm-1: compiled-function object + ISA */

TEST_SUITE_SETUP(misc_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);   /* bocl-internal names live in CL */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(misc_fini){
    return CUE_SUCCESS;}

static Ref e(const char* s){ return bocl_read_eval_cstring(s); }

/* ---- MAKE-ARRAY -------------------------------------------------------- */
static void test_make_array(void){
    /* default fill is NIL */
    TEST_EQ(equal, e("3"),
            e("(let ((v (make-array 3))) (vector-length v))"));
    /* :initial-element */
    TEST_EQ(equal, e("'(0 0 0)"),
            e("(let* ((v (make-array 3 :initial-element 0)))"
              "  (list (svref v 0) (svref v 1) (svref v 2)))"));
    /* :initial-contents */
    TEST_EQ(equal, e("'(a b c)"),
            e("(let* ((v (make-array 3 :initial-contents '(a b c))))"
              "  (list (svref v 0) (svref v 1) (svref v 2)))"));
    /* empty array */
    TEST_EQ(equal, e("0"),
            e("(vector-length (make-array 0))"));}

/* ---- ARRAY-DIMENSIONS / ARRAY-TOTAL-SIZE / ARRAY-RANK ----------------- */
static void test_array_info(void){
    TEST_EQ(equal, e("'(4)"),
            e("(array-dimensions (make-array 4))"));
    TEST_EQ(equal, e("4"),
            e("(array-total-size (make-array 4))"));
    TEST_EQ(equal, e("1"),
            e("(array-rank (make-array 4))"));}

/* ---- ARRAY-IN-BOUNDS-P ------------------------------------------------ */
static void test_array_in_bounds(void){
    TEST_EQ(equal, e("t"),
            e("(array-in-bounds-p (make-array 5) 0)"));
    TEST_EQ(equal, e("t"),
            e("(array-in-bounds-p (make-array 5) 4)"));
    TEST_EQ(equal, e("nil"),
            e("(array-in-bounds-p (make-array 5) 5)"));
    TEST_EQ(equal, e("nil"),
            e("(array-in-bounds-p (make-array 5) -1)"));}

/* ---- ADJUSTABLE-ARRAY-P / ARRAY-HAS-FILL-POINTER-P ------------------- */
static void test_array_predicates(void){
    TEST_EQ(equal, e("nil"),
            e("(adjustable-array-p (make-array 3))"));
    TEST_EQ(equal, e("nil"),
            e("(array-has-fill-pointer-p (make-array 3))"));
    TEST_EQ(equal, e("t"),
            e("(simple-vector-p (vector 1 2 3))"));
    TEST_EQ(equal, e("nil"),
            e("(simple-vector-p '(1 2 3))"));}

/* ---- MAKE-LIST -------------------------------------------------------- */
static void test_make_list(void){
    TEST_EQ(equal, e("'(nil nil nil)"),
            e("(make-list 3)"));
    TEST_EQ(equal, e("'(0 0 0 0)"),
            e("(make-list 4 :initial-element 0)"));
    TEST_EQ(equal, e("nil"),
            e("(make-list 0)"));}

/* ---- COPY-SEQ --------------------------------------------------------- */
static void test_copy_seq(void){
    /* list — check that modifying the copy doesn't affect the original */
    TEST_EQ(equal, e("'(1 99)"),
            e("(let* ((x (list 1 2 3)) (y (copy-seq x)))"
              "  (rplaca y 99) (list (car x) (car y)))"));
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(copy-seq '(1 2 3))"));
    /* vector */
    TEST_EQ(equal, e("3"),
            e("(vector-length (copy-seq (vector 10 20 30)))"));
    /* string */
    TEST_EQ(equal, e("\"hello\""),
            e("(copy-seq \"hello\")"));}

/* ---- RANDOM ----------------------------------------------------------- */
static void test_random(void){
    /* Result must be in [0, limit) */
    TEST_EQ(equal, e("t"),
            e("(let ((r (random 10))) (and (>= r 0) (< r 10)))"));
    /* Float limit */
    TEST_EQ(equal, e("t"),
            e("(let ((r (random 1.0))) (and (>= r 0.0) (< r 1.0))))"));
    /* Make-random-state returns T (our sentinel) */
    TEST_EQ(equal, e("t"),
            e("(make-random-state)"));}

/* ---- PROBE-FILE ------------------------------------------------------- */
static void test_probe_file(void){
    /* /dev/null always exists */
    TEST_EQ(equal, e("t"),
            e("(if (probe-file \"/dev/null\") t nil)"));
    /* Non-existent path */
    TEST_EQ(equal, e("nil"),
            e("(probe-file \"/bocl_no_such_file_should_exist_12345\")"));}

/* ---- DELETE-FILE / RENAME-FILE --------------------------------------- */
static void test_file_ops(void){
    /* Create a temp file, probe, rename, probe, delete, probe. */
    TEST_EQ(equal, e("t"),
            e("(progn"
              "  (with-open-file (f \"/tmp/bocl_test_misc_1.tmp\" :direction :output"
              "                      :if-exists :supersede)"
              "    (write-string \"hello\" f))"
              "  (if (probe-file \"/tmp/bocl_test_misc_1.tmp\") t nil))"));
    TEST_EQ(equal, e("t"),
            e("(progn"
              "  (rename-file \"/tmp/bocl_test_misc_1.tmp\""
              "               \"/tmp/bocl_test_misc_2.tmp\")"
              "  (if (probe-file \"/tmp/bocl_test_misc_2.tmp\") t nil))"));
    TEST_EQ(equal, e("nil"),
            e("(probe-file \"/tmp/bocl_test_misc_1.tmp\")"));
    /* delete-file returns T; probe-file after deletion returns NIL */
    TEST_EQ(equal, e("t"), e("(delete-file \"/tmp/bocl_test_misc_2.tmp\")"));
    TEST_EQ(equal, e("nil"),
            e("(probe-file \"/tmp/bocl_test_misc_2.tmp\")"));}

/* ---- TIMING ----------------------------------------------------------- */
static void test_timing(void){
    /* internal-time-units-per-second should be 1000000 */
    TEST_EQ(equal, e("1000000"),
            e("internal-time-units-per-second"));
    /* get-universal-time returns an integer > 3900000000 (past year 2023) */
    TEST_EQ(equal, e("t"),
            e("(> (get-universal-time) 3900000000)"));
    /* get-internal-real-time returns a positive integer */
    TEST_EQ(equal, e("t"),
            e("(> (get-internal-real-time) 0)"));
    /* get-internal-run-time returns a positive integer */
    TEST_EQ(equal, e("t"),
            e("(> (get-internal-run-time) 0)"));}

/* ---- FILL ------------------------------------------------------------- */
static void test_fill(void){
    TEST_EQ(equal, e("'(0 0 0)"),
            e("(let ((x (list 1 2 3))) (fill x 0))"));
    TEST_EQ(equal, e("'(1 0 0)"),
            e("(let ((x (list 1 2 3))) (fill x 0 :start 1) x)"));
    TEST_EQ(equal, e("'(1 0 3)"),
            e("(let ((x (list 1 2 3))) (fill x 0 :start 1 :end 2) x)"));}

/* ---- REPLACE ---------------------------------------------------------- */
static void test_replace(void){
    TEST_EQ(equal, e("'(4 5 6)"),
            e("(let ((x (list 1 2 3))) (replace x '(4 5 6)))"));
    TEST_EQ(equal, e("'(1 4 5)"),
            e("(let ((x (list 1 2 3))) (replace x '(4 5 6) :start1 1) x)"));
    TEST_EQ(equal, e("'(1 4 3)"),
            e("(let ((x (list 1 2 3))) (replace x '(4 5 6) :start1 1 :end1 2) x)"));}

/* ---- MISMATCH --------------------------------------------------------- */
static void test_mismatch(void){
    TEST_EQ(equal, e("nil"),
            e("(mismatch '(1 2 3) '(1 2 3))"));
    TEST_EQ(equal, e("2"),
            e("(mismatch '(1 2 3) '(1 2 4))"));
    TEST_EQ(equal, e("0"),
            e("(mismatch '(1 2 3) '(9 2 3))"));
    /* length mismatch */
    TEST_EQ(equal, e("3"),
            e("(mismatch '(1 2 3) '(1 2 3 4))"));}

/* ---- WRITE-TO-STRING (should honour :escape) -------------------------- */
static void test_write_to_string(void){
    /* Default: :escape t  → printed with prin1 quoting */
    TEST_EQ(equal, e("\"\\\"hello\\\"\""),
            e("(write-to-string \"hello\")"));
    /* :escape nil → princ style */
    TEST_EQ(equal, e("\"hello\""),
            e("(write-to-string \"hello\" :escape nil)"));}

/* Profiling interface (issue hot-1-profile).  The default build has
   -DBOCL_PROFILE OFF, so ENABLED-P is NIL and the entry points are no-ops. */
static void test_profile_interface(void){
    TEST_EQ(eql, NIL_Symbol, e("(bocl:profile-enabled-p)"));   /* off in default build */
    /* WITH-PROFILE-PHASE runs BODY and returns its value even when off */
    TEST_EQ(eql, I(42), e("(bocl:with-profile-phase :macroexpand 42)"));
    TEST_EQ(eql, I(3),  e("(bocl:with-profile-phase :eval (+ 1 2))"));
    /* RESET / REPORT don't error (report to a string stream) */
    TEST_EQ(eql, NIL_Symbol, e("(bocl:profile-reset)"));
    TEST_EQ(eql, T_Symbol,
            e("(progn (with-output-to-string (s) (bocl:profile-report s)) t)"));}

/* cfasl-1: binary fasl low-level I/O primitives round-trip (issue plan-1-compact-fasl). */
static void test_cfasl_primitives(void){
    cfasl_out b; cfasl_out_init(&b);
    /* write a mix */
    cfasl_put_u8(&b, 0xAB);
    uint64_t uv[] = { 0, 1, 127, 128, 300, 16384, 0xFFFFFFFFull, 0x123456789ABCull };
    for(size_t i=0;i<sizeof uv/sizeof uv[0]; i++){ cfasl_put_uvarint(&b, uv[i]); }
    int64_t  sv[] = { 0, 1, -1, 63, -64, 1000, -1000, 0x7FFFFFFFLL, -0x80000000LL };
    for(size_t i=0;i<sizeof sv/sizeof sv[0]; i++){ cfasl_put_svarint(&b, sv[i]); }
    cfasl_put_f32(&b, 3.5f);
    cfasl_put_f64(&b, -2.718281828459045);
    const char* s = "hi"; cfasl_put_bytes(&b, s, 2);
    TEST_EQ(eql, NIL_Symbol, b.error ? T_Symbol : NIL_Symbol);   /* no OOM */

    /* read it back */
    cfasl_in c; cfasl_in_init(&c, b.data, b.len);
    TEST_EQ(eql, I(0xAB), I(cfasl_get_u8(&c)));
    for(size_t i=0;i<sizeof uv/sizeof uv[0]; i++){
        TEST_EQ(eql, T_Symbol, cfasl_get_uvarint(&c)==uv[i] ? T_Symbol : NIL_Symbol); }
    for(size_t i=0;i<sizeof sv/sizeof sv[0]; i++){
        TEST_EQ(eql, T_Symbol, cfasl_get_svarint(&c)==sv[i] ? T_Symbol : NIL_Symbol); }
    TEST_EQ(eql, T_Symbol, cfasl_get_f32(&c)==3.5f ? T_Symbol : NIL_Symbol);
    TEST_EQ(eql, T_Symbol, cfasl_get_f64(&c)==-2.718281828459045 ? T_Symbol : NIL_Symbol);
    char two[3]={0}; cfasl_get_bytes(&c, two, 2);
    TEST_EQ(eql, T_Symbol, (two[0]=='h'&&two[1]=='i') ? T_Symbol : NIL_Symbol);
    TEST_EQ(eql, NIL_Symbol, c.error ? T_Symbol : NIL_Symbol);   /* consumed cleanly */
    TEST_EQ(eql, T_Symbol, cfasl_at_end(&c) ? T_Symbol : NIL_Symbol);
    cfasl_out_free(&b);
}

/* A minimal structural decoder mirroring the cfasl.h tag grammar: it walks the
   serialized graph without rebuilding it, so the writer (cfasl-2) can be checked
   before the real loader (cfasl-3) exists.  Counts BACKREFs seen. */
static int cfasl_backrefs_seen;
static void cfasl_skip_object(cfasl_in* c){
    if(c->error){ return; }
    uint8_t tag = cfasl_get_u8(c);
    switch(tag){
    case CFASL_TAG_NIL: case CFASL_TAG_T: case CFASL_TAG_EOF:            break;
    case CFASL_TAG_BACKREF:      cfasl_get_uvarint(c); cfasl_backrefs_seen++; break;
    case CFASL_TAG_FIXNUM:       cfasl_get_svarint(c);                   break;
    case CFASL_TAG_CHARACTER:    cfasl_get_uvarint(c);                   break;
    case CFASL_TAG_DOUBLE_FLOAT: cfasl_get_f64(c);                       break;
    case CFASL_TAG_SINGLE_FLOAT: cfasl_get_f32(c);                       break;
    case CFASL_TAG_STRING:{ uint64_t n=cfasl_get_uvarint(c);
                            for(uint64_t i=0;i<n && !c->error;i++){ cfasl_get_u8(c);} } break;
    case CFASL_TAG_KEYWORD: case CFASL_TAG_UNINTERNED:
                                 cfasl_skip_object(c);                   break;
    case CFASL_TAG_SYMBOL: case CFASL_TAG_CONS: case CFASL_TAG_RATIO:
                                 cfasl_skip_object(c); cfasl_skip_object(c); break;
    case CFASL_TAG_SIMPLE_VECTOR:{ uint64_t n=cfasl_get_uvarint(c);
                            for(uint64_t i=0;i<n && !c->error;i++){ cfasl_skip_object(c);} } break;
    default: c->error = 1;                                              break;
    }
}

/* cfasl-2: the object-graph writer emits a decodable, EQ-sharing-preserving
   stream (issue cfasl-2-object-serializer). */
static void test_cfasl_writer(void){
    /* the two X are the same interned symbol; a fixnum, string, char and ratio
       exercise the scalar tags too. */
    Ref form = e("'(setq x x 1 \"hi\" #\\A 3/4 #(0 1 2))");
    cfasl_out b; cfasl_out_init(&b);
    cfasl_wctx* w = cfasl_wctx_new(&b);
    cfasl_write_object(w, form);
    TEST_EQ(eql, NIL_Symbol, cfasl_wctx_error(w) ? T_Symbol : NIL_Symbol); /* all types supported */
    TEST_EQ(eql, I(CFASL_TAG_CONS), I(b.data[0]));                         /* a list at the root */

    cfasl_in c; cfasl_in_init(&c, b.data, b.len);
    cfasl_backrefs_seen = 0;
    cfasl_skip_object(&c);
    TEST_EQ(eql, NIL_Symbol, c.error ? T_Symbol : NIL_Symbol);            /* decodes cleanly */
    TEST_EQ(eql, T_Symbol, cfasl_at_end(&c) ? T_Symbol : NIL_Symbol);     /* consumes exactly */
    TEST_EQ(eql, T_Symbol, cfasl_backrefs_seen>=1 ? T_Symbol : NIL_Symbol); /* repeated X shared */

    /* an unsupported object (bignum) flags the context so the caller can fall
       back to the text fasl rather than emit a truncated graph. */
    cfasl_out b2; cfasl_out_init(&b2);
    cfasl_wctx* w2 = cfasl_wctx_new(&b2);
    cfasl_write_object(w2, e("(* 1000000000000 1000000000000)"));         /* a bignum */
    TEST_EQ(eql, T_Symbol, cfasl_wctx_error(w2) ? T_Symbol : NIL_Symbol);

    cfasl_wctx_free(w);  cfasl_out_free(&b);
    cfasl_wctx_free(w2); cfasl_out_free(&b2);
}

/* Serialize FORM with the writer and read it back with the reader (into an open
   live pool).  Sets *back.  Asserts no writer/reader error and clean EOF. */
static Ref cfasl_roundtrip(const char* src, uword pool){
    (void)pool;
    Ref original = e(src);
    cfasl_out b; cfasl_out_init(&b);
    cfasl_wctx* w = cfasl_wctx_new(&b);
    cfasl_write_object(w, original);
    TEST_EQ(eql, NIL_Symbol, cfasl_wctx_error(w) ? T_Symbol : NIL_Symbol);
    cfasl_in in; cfasl_in_init(&in, b.data, b.len);
    cfasl_rctx* r = cfasl_rctx_new(&in);
    Ref back = cfasl_read_object(r);
    TEST_EQ(eql, NIL_Symbol, cfasl_rctx_error(r) ? T_Symbol : NIL_Symbol);
    TEST_EQ(eql, T_Symbol, cfasl_at_end(&in) ? T_Symbol : NIL_Symbol);
    cfasl_rctx_free(r);
    cfasl_wctx_free(w);
    cfasl_out_free(&b);
    return back;
}

/* cfasl-3: the loader/deserializer rebuilds the graph EQUAL to the original and
   preserves EQ identity for shared sub-objects (issue cfasl-3-loader). */
static void test_cfasl_roundtrip(void){
    uword pool = open_live_pool();

    /* (1) conses / symbols / string / fixnums: EQUAL descends fully. */
    Ref original = e("'(defun f (x) (setq x x) \"doc\" (a b 42) nil)");
    Ref back = cfasl_roundtrip("'(defun f (x) (setq x x) \"doc\" (a b 42) nil)", pool);
    TEST_EQ(eql, T_Symbol, equal(original, back) ? T_Symbol : NIL_Symbol);

    /* EQ sharing: the two X of (setq x x) are one symbol, and it is THE interned
       X (not a fresh copy) — the deserializer interns, it does not build symbols. */
    Ref setqform = car(cdr(cdr(cdr(back))));      /* (setq x x) */
    Ref x1 = car(cdr(setqform));
    Ref x2 = car(cdr(cdr(setqform)));
    TEST_EQ(eql, T_Symbol, (x1==x2) ? T_Symbol : NIL_Symbol);
    TEST_EQ(eql, T_Symbol, (x1==e("'x")) ? T_Symbol : NIL_Symbol);

    /* (2) scalars + simple-vector + keyword: EQUAL skips vectors, so check the
       shape element-wise. */
    Ref v = cfasl_roundtrip("(vector #\\Z 3/4 :kw)", pool);   /* a live simple-vector */
    TEST_EQ(eql, T_Symbol, eql(svref(v, I(0)), e("#\\Z")) ? T_Symbol : NIL_Symbol);
    TEST_EQ(eql, T_Symbol, eql(svref(v, I(1)), e("3/4"))  ? T_Symbol : NIL_Symbol);
    TEST_EQ(eql, T_Symbol, (svref(v, I(2)) == e(":kw"))   ? T_Symbol : NIL_Symbol);   /* keyword interned once */

    close_live_pool(pool);
}

/* cfasl-3: bocl_fasl_load reads a complete cfasl image from disk (magic +
   version + form stream + EOF) and evaluates its forms with real side effects,
   and rejects a non-cfasl file so LOAD can fall back. */
static void test_cfasl_load_file(void){
    /* assemble a full image: header, one DEFUN form, EOF tag */
    Ref form = e("'(defun %cfasl-sq (x) (* x x))");
    cfasl_out b; cfasl_out_init(&b);
    cfasl_put_bytes(&b, CFASL_MAGIC, CFASL_MAGIC_LEN);
    cfasl_put_u8(&b, (uint8_t)CFASL_VERSION);
    cfasl_put_u8(&b, 0);                                  /* flags */
    cfasl_wctx* w = cfasl_wctx_new(&b);
    cfasl_write_object(w, form);
    cfasl_put_u8(&b, CFASL_TAG_EOF);
    TEST_EQ(eql, NIL_Symbol, (cfasl_wctx_error(w)||b.error) ? T_Symbol : NIL_Symbol);

    char path[] = "/tmp/bocl_cfaslXXXXXX";
    int fd = mkstemp(path);
    TEST_EQ(eql, T_Symbol, (fd>=0) ? T_Symbol : NIL_Symbol);
    ssize_t wr = write(fd, b.data, b.len); (void)wr; close(fd);

    Ref res = bocl_fasl_load(path);
    unlink(path);
    TEST_EQ(eql, T_Symbol, res);                         /* loaded OK */
    TEST_EQ(eql, I(49), e("(%cfasl-sq 7)"));             /* the DEFUN took effect */

    /* a file that is not a cfasl loads as NIL so LOAD falls back to source */
    char p2[] = "/tmp/bocl_notfaslXXXXXX";
    int fd2 = mkstemp(p2);
    ssize_t wr2 = write(fd2, "not a bocl fasl!!!!", 19); (void)wr2; close(fd2);
    TEST_EQ(eql, NIL_Symbol, bocl_fasl_load(p2));
    unlink(p2);

    cfasl_wctx_free(w);
    cfasl_out_free(&b);
}

/* cfasl-5: compile-file -> binary fasl -> load must equal a source load across a
   feature corpus, AND a load large enough to trigger GC mid-deserialization must
   survive (regression guard for the reader live-pool rooting — Object_new does
   not leave its result rooted, so recorded objects would otherwise be collected
   out from under a pending BACKREF). */
static void test_cfasl_corpus(void){
    /* (1) GC-rooting guard: a file with enough forms + repeated CL symbols
       (backrefs) that binary load allocates through several GCs. */
    FILE* f = fopen("/tmp/bocl_cfasl_corpus.lisp", "w");
    TEST_EQ(eql, T_Symbol, f ? T_Symbol : NIL_Symbol);
    fputs("(defpackage :cfc (:use :cl))(in-package :cfc)\n", f);
    for(int i=0;i<200;i++){
        fprintf(f, "(defmacro m%d (x) (list '+ %d x))\n", i, i);
        fprintf(f, "(defun f%d (l) (let ((a 0)) (dolist (e l a) (setq a (+ a (m%d e))))))\n", i, i);
    }
    fclose(f);
    e("(compile-file \"/tmp/bocl_cfasl_corpus.lisp\")");
    TEST_EQ(eql, T_Symbol, e("(%cfasl-file-p \"/tmp/bocl_cfasl_corpus.fasl\")")); /* binary */
    e("(load \"/tmp/bocl_cfasl_corpus.fasl\")");
    TEST_EQ(eql, I(552), e("(cfc::f182 '(1 2 3))"));   /* 183+184+185 — survived GC */
    TEST_EQ(eql, I(6),   e("(cfc::f0 '(1 2 3))"));      /* 1+2+3 */

    /* (2) feature coverage: DEFPACKAGE/IN-PACKAGE, EVAL-WHEN, DEFCLASS/DEFMETHOD,
       ratio/float/string/simple-vector literals — binary load == expected. */
    FILE* g = fopen("/tmp/bocl_cfasl_feat.lisp", "w");
    TEST_EQ(eql, T_Symbol, g ? T_Symbol : NIL_Symbol);
    fputs("(defpackage :cfg (:use :cl) (:export #:area))\n(in-package :cfg)\n", g);
    fputs("(eval-when (:load-toplevel :execute) (defparameter *lv* :yes))\n", g);
    fputs("(defclass shp () ((r :initarg :r :accessor r)))\n", g);
    fputs("(defmethod area ((s shp)) (* 3 (r s) (r s)))\n", g);
    fputs("(defvar *rat* 3/4)(defvar *flo* 2.5d0)(defvar *str* \"hi\")(defvar *vec* #(1 2 3))\n", g);
    fputs("(in-package :cl-user)\n", g);
    fclose(g);
    e("(compile-file \"/tmp/bocl_cfasl_feat.lisp\")");
    e("(load \"/tmp/bocl_cfasl_feat.fasl\")");
    TEST_EQ(eql, T_Symbol, e("(if (eq cfg::*lv* :yes) t nil)"));           /* eval-when :load */
    TEST_EQ(eql, I(48),    e("(cfg:area (make-instance 'cfg::shp :r 4))")); /* class + method */
    TEST_EQ(eql, T_Symbol, e("(if (= cfg::*rat* 3/4) t nil)"));            /* ratio */
    TEST_EQ(eql, T_Symbol, e("(if (= cfg::*flo* 2.5d0) t nil)"));          /* double-float */
    TEST_EQ(equal, e("\"hi\""), e("cfg::*str*"));                          /* string */
    TEST_EQ(eql, I(2),     e("(aref cfg::*vec* 1)"));                      /* simple-vector */
}

/* vm-1: the compiled-function object exists, is FUNCTIONP, its accessors round-
   trip, and the arity descriptor packs/unpacks.  No execution yet (vm-2). */
static void test_vm_compiled_function(void){
    uword pool = open_live_pool();
    Ref name   = e("'vm1-cf");
    Ref ll     = e("'(x y &optional z)");
    Ref code   = e("\"abc\"");                 /* placeholder octet vector (bocl string) */
    Ref consts = e("(vector 'foo 42 \"s\")");
    word apacked = BC_ARITY_PACK(2,1,0,0,0);   /* 2 required, 1 optional, no rest/key */
    Ref cf = compiled_function_new(name, ll, code, consts,
                                   I(apacked), I(5), I(3), e("'(lambda (x y &optional z) x)"));
    add_to_live_pool(cf);

    /* predicates */
    TEST_EQ(eql, T_Symbol,   compiled_function_p(cf) ? T_Symbol : NIL_Symbol);
    TEST_EQ(eql, NIL_Symbol, compiled_closure_p(cf)  ? T_Symbol : NIL_Symbol);

    /* accessors round-trip what was stored */
    TEST_EQ(eql,   e("'vm1-cf"),           compiled_function_name(cf));
    TEST_EQ(equal, e("'(x y &optional z)"),compiled_function_lambda_list(cf));
    TEST_EQ(equal, e("\"abc\""),           compiled_function_code(cf));
    TEST_EQ(eql,   I(5),                   compiled_function_max_stack(cf));
    TEST_EQ(eql,   I(3),                   compiled_function_n_locals(cf));
    TEST_EQ(eql,   e("'foo"),              svref(compiled_function_constants(cf), I(0)));

    /* arity descriptor packs/unpacks */
    word a = integer_value(compiled_function_arity(cf));
    TEST_EQ(eql, I(2), I(BC_ARITY_REQUIRED(a)));
    TEST_EQ(eql, I(1), I(BC_ARITY_OPTIONAL(a)));
    TEST_EQ(eql, NIL_Symbol, BC_ARITY_REST_P(a) ? T_Symbol : NIL_Symbol);
    TEST_EQ(eql, NIL_Symbol, BC_ARITY_KEY_P(a)  ? T_Symbol : NIL_Symbol);

    /* a compiled closure wraps the template */
    Ref cc = compiled_closure_new(cf, e("(vector)"));
    add_to_live_pool(cc);
    TEST_EQ(eql, T_Symbol, compiled_closure_p(cc) ? T_Symbol : NIL_Symbol);
    TEST_EQ(eql, cf, compiled_closure_function(cc));

    /* CL FUNCTIONP accepts both, alongside interpreted / built-in functions */
    set_symbol_value(e("'*vm1-cf*"), cf);
    set_symbol_value(e("'*vm1-cc*"), cc);
    TEST_EQ(eql, T_Symbol, e("(functionp *vm1-cf*)"));
    TEST_EQ(eql, T_Symbol, e("(functionp *vm1-cc*)"));

    close_live_pool(pool);
}

/* vm-2: hand-assemble tiny byte-code functions and run them through the VM,
   matching the tree-walker.  Opcode operands here are all < 128 (single-byte
   uvarints / zig-zag svarints), so the byte arrays are literal. */
static Ref vm_code_str(const uint8_t* bytes, size_t n){
    Ref s = string_new((halfword)n);
    memcpy(string_octets(s), bytes, n);
    return s;
}
/* apply a compiled function to a Lisp arg-list, return the primary value */
static Ref vm_call(Ref cf, const char* arglist_src){
    MultipleValues r = {0};
    kernel_apply(cf, e(arglist_src), &r);
    return r.count>0 ? r.values[0] : NIL_Symbol;
}
static void test_vm_execute(void){
    uword pool = open_live_pool();

    /* (1) (lambda (x) (+ x 1)) : LOAD-LOCAL 0 0; PUSH-CONST 1; CALL-NAMED + 2; RETURN */
    {   uint8_t bc[] = { BC_LOAD_LOCAL,0,0, BC_PUSH_CONST,1, BC_CALL_NAMED,0,2, BC_RETURN };
        Ref cf = compiled_function_new(e("'vmadd1"), e("'(x)"),
                    vm_code_str(bc,sizeof bc), e("(vector '+ 1)"),
                    I(BC_ARITY_PACK(1,0,0,0,0)), I(2), I(1), NIL_Symbol);
        add_to_live_pool(cf);
        TEST_EQ(eql, I(42), vm_call(cf, "'(41)"));
        TEST_EQ(eql, I(0),  vm_call(cf, "'(-1)"));
    }

    /* (2) (lambda (x) (if x 1 2)) : forward JUMP-IF-NIL + JUMP */
    {   uint8_t bc[] = { BC_LOAD_LOCAL,0,0, BC_JUMP_IF_NIL,8, BC_PUSH_CONST,0,
                         BC_JUMP,4, BC_PUSH_CONST,1, BC_RETURN };
        Ref cf = compiled_function_new(e("'vmif"), e("'(x)"),
                    vm_code_str(bc,sizeof bc), e("(vector 1 2)"),
                    I(BC_ARITY_PACK(1,0,0,0,0)), I(1), I(1), NIL_Symbol);
        add_to_live_pool(cf);
        TEST_EQ(eql, I(1), vm_call(cf, "'(t)"));
        TEST_EQ(eql, I(2), vm_call(cf, "'(nil)"));
    }

    /* (3) (lambda (l) (car l)) : CALL to a built-in */
    {   uint8_t bc[] = { BC_LOAD_LOCAL,0,0, BC_CALL_NAMED,0,1, BC_RETURN };
        Ref cf = compiled_function_new(e("'vmcar"), e("'(l)"),
                    vm_code_str(bc,sizeof bc), e("(vector 'car)"),
                    I(BC_ARITY_PACK(1,0,0,0,0)), I(1), I(1), NIL_Symbol);
        add_to_live_pool(cf);
        TEST_EQ(eql, I(99), vm_call(cf, "'((99 . 5))"));
    }

    /* (4) reverse via a backward-JUMP loop (car/cdr/cons + nil test + STORE-LOCAL) */
    {   uint8_t bc[] = {
            /*0 loop*/ BC_LOAD_LOCAL,0,0, BC_JUMP_IF_NIL,56,   /* if (null lst) goto done */
            /*5*/      BC_LOAD_LOCAL,0,0, BC_CALL_NAMED,1,1,    /* (car lst)   [car=idx1] */
            /*11*/     BC_LOAD_LOCAL,0,1, BC_CALL_NAMED,0,2,    /* (cons .. r) [cons=idx0] */
            /*17*/     BC_STORE_LOCAL,0,1, BC_POP,              /* r = ...          */
            /*21*/     BC_LOAD_LOCAL,0,0, BC_CALL_NAMED,2,1,    /* (cdr lst)   [cdr=idx2] */
            /*27*/     BC_STORE_LOCAL,0,0, BC_POP,              /* lst = ...        */
            /*31*/     BC_JUMP,65,                              /* goto loop (-33)  */
            /*33 done*/BC_LOAD_LOCAL,0,1, BC_RETURN };          /* return r         */
        Ref cf = compiled_function_new(e("'vmrev"), e("'(lst)"),
                    vm_code_str(bc,sizeof bc), e("(vector 'cons 'car 'cdr)"),
                    I(BC_ARITY_PACK(1,0,0,0,0)), I(3), I(2), NIL_Symbol);
        add_to_live_pool(cf);
        TEST_EQ(equal, e("'(3 2 1)"), vm_call(cf, "'((1 2 3))"));
        TEST_EQ(eql,   NIL_Symbol,    vm_call(cf, "'(())"));
    }

    close_live_pool(pool);
}

/* ---- vm-3: the compiler front-end / lexical analyzer ------------------- */
/* %VM-ANALYZE (bootstrap/vm-compiler.lisp) walks a (LAMBDA ...) into IR with
   every lexical reference resolved to a (depth index) address in the flat-locals
   model.  The IR is a keyword-tagged tree; here we check the :BODY / :ARITY the
   analyzer produces for the acceptance lambdas (nested LET, closures, LABELS
   recursion, SPECIAL binding, macro expansion).  BODY(x) := (getf (cdr x) :body). */
static void test_vm_analyze(void){
    uword pool = open_live_pool();
    e("(defvar *vm3-dyn* 0)");

    /* the analyzer must be present (prelude loaded vm-compiler.lisp) */
    TEST_EQ(eql, T_Symbol, e("(if (fboundp '%vm-analyze) t nil)"));

    /* identity: parameter X -> depth 0, slot 0 */
    TEST_EQ(equal, e("'(:lref 0 0)"),
            e("(getf (cdr (%vm-analyze '(lambda (x) x))) :body)"));

    /* flat-locals: nested LET consumes fresh slots (does NOT reset per LET) */
    TEST_EQ(equal, e("'(:let ((:lex 1 (:const 1)))"
                     "   (:let ((:lex 2 (:const 2)))"
                     "     (:call-named list (:lref 0 0) (:lref 0 1) (:lref 0 2))))"),
            e("(getf (cdr (%vm-analyze"
              "  '(lambda (x) (let ((y 1)) (let ((z 2)) (list x y z)))))) :body)"));

    /* LET* : second init sees the first binding (slot 0) */
    TEST_EQ(equal, e("'(:let* ((:lex 0 (:const 1)) (:lex 1 (:lref 0 0))) (:lref 0 1))"),
            e("(getf (cdr (%vm-analyze '(lambda () (let* ((a 1) (b a)) b)))) :body)"));

    /* closure: reference to an outer var crosses one lambda -> depth 1.
       BODY is (:make-closure (:lambda ...)); the inner lambda's :body is (:lref 1 0). */
    TEST_EQ(equal, e("'(:lref 1 0)"),
            e("(getf (cdr (cadr (getf (cdr"
              "   (%vm-analyze '(lambda (x) (lambda () x)))) :body))) :body)"));

    /* LABELS recursion: the self call resolves the function slot at depth 1 */
    TEST_EQ(equal, e("t"),
            e("(let* ((body (getf (cdr (%vm-analyze"
              "        '(lambda () (labels ((f (n) (if (= n 0) 1 (f (- n 1))))) (f 3)))))"
              "        :body))"
              "       (fbody (getf (cdr (cdar (cadr body))) :body)))"
              "  (if (equal (cadddr fbody) '(:call (:lref 1 0)"
              "                              (:call-named - (:lref 0 0) (:const 1)))) t nil))"));

    /* SPECIAL binding: a DEFVAR'd var -> dynamic (:special) bind and (:gref) ref */
    TEST_EQ(equal, e("'(:let ((:special *vm3-dyn* (:const 1))) (:gref *vm3-dyn*))"),
            e("(getf (cdr (%vm-analyze '(lambda () (let ((*vm3-dyn* 1)) *vm3-dyn*)))) :body)"));

    /* SETQ: lexical -> :lset, dynamic/global -> :gset */
    TEST_EQ(equal, e("'(:lset 0 0 (:const 9))"),
            e("(getf (cdr (%vm-analyze '(lambda (x) (setq x 9)))) :body)"));
    TEST_EQ(equal, e("'(:gset *vm3-dyn* (:const 9))"),
            e("(getf (cdr (%vm-analyze '(lambda () (setq *vm3-dyn* 9)))) :body)"));

    /* MACROLET / SYMBOL-MACROLET are expanded away by the analyzer */
    TEST_EQ(equal, e("'(:call-named * (:lref 0 0) (:lref 0 0))"),
            e("(getf (cdr (%vm-analyze"
              "  '(lambda (x) (macrolet ((sq (a) (list '* a a))) (sq x))))) :body)"));
    TEST_EQ(equal, e("'(:call-named car (:lref 0 0))"),
            e("(getf (cdr (%vm-analyze"
              "  '(lambda (x) (symbol-macrolet ((m (car x))) m)))) :body)"));

    /* arity pack + n-locals (supplied-p vars take slots) */
    TEST_EQ(equal, e("'(:arity 2 1 t t nil)"),
            e("(getf (cdr (%vm-analyze"
              "  '(lambda (a b &optional (c 3) &rest r &key k) (list a b c r k)))) :arity)"));
    TEST_EQ(equal, e("3"),
            e("(getf (cdr (%vm-analyze '(lambda (a &optional (c 3 cp)) (list a c cp)))) :n-locals)"));

    /* FLET: a local function shadows the global of the same name at its slot.
       BODY is (:flet ((idx . lambda-ir)) call); the call is its third element. */
    TEST_EQ(equal, e("'(:call (:lref 0 1) (:lref 0 0))"),
            e("(caddr (getf (cdr (%vm-analyze"
              "  '(lambda (x) (flet ((list (y) y)) (list x))))) :body))"));

    close_live_pool(pool);
}

/* ---- vm-4: byte-code codegen core, differential vs the tree-walker ------ */
/* %VM-COMPILE (bootstrap/vm-compiler.lisp) analyzes + byte-compiles a lambda into
   a runnable CompiledFunction.  VM4-CMP compiles a lambda AND interprets the same
   lambda (via (eval (function lambda))), applies both to the same args, and returns
   T iff the results are EQUAL — so each check proves the compiled path matches the
   tree-walker.  */
static void test_vm_codegen(void){
    uword pool = open_live_pool();
    e("(defvar *vm4-g* 100)");
    e("(setf (symbol-function 'vm4-dbl) (%vm-compile '(lambda (n) (* n 2))))");
    e("(defun vm4-cmp (lam args)"
      "  (equal (apply (%vm-compile lam) args)"
      "         (apply (eval (list 'function lam)) args)))");

    /* the compiler produces a genuine compiled-function */
    TEST_EQ(eql, T_Symbol,
            e("(let ((f (%vm-compile '(lambda (x) x)))) (if (functionp f) t nil))"));

    /* arithmetic / calls */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x) (+ x 1)) '(41))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x y) (* (+ x 1) (- y 2))) '(4 5))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (l) (car (cdr l))) '((1 2 3)))"));
    /* IF, IF-chains */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x) (if x 'yes 'no)) '(7))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x) (if x 'yes 'no)) '(nil))"));
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda (n) (if (= n 0) 'z (if (= n 1) 'o 'm))) '(1))"));
    /* PROGN effect context */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x) (progn (+ x 1) (* x 2))) '(10))"));
    /* LET parallel (inner init sees the OUTER binding), nested LET fresh slots */
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda (x) (let ((y 5)) (let ((y 9) (z y)) (list y z x)))) '(0))"));
    /* LET* sequential */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda () (let* ((a 2) (b (* a 3))) (+ a b))) '())"));
    /* SETQ lexical, global var ref + set */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x) (setq x (+ x 100)) x) '(1))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda () *vm4-g*) '())"));
    /* QUOTE literal, FUNCALL of a passed function */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda () '(a b c)) '())"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (f x) (funcall f x)) (list #'1+ 5))"));
    /* &optional (NIL default) and &rest */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x &optional y) (list x y)) '(1))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (a &rest r) (cons a r)) '(1 2 3))"));

    /* SETQ of a global takes effect in the running image */
    TEST_EQ(eql, I(7), e("(progn (funcall (%vm-compile '(lambda () (setq *vm4-g* 7) *vm4-g*))))"));
    /* a compiled function calling another compiled function */
    TEST_EQ(eql, I(20), e("(funcall (%vm-compile '(lambda (x) (vm4-dbl (vm4-dbl x)))) 5)"));

    close_live_pool(pool);
}

/* ---- vm-5 (part 1): lexical control — BLOCK/RETURN-FROM/TAGBODY/GO as jumps -- */
/* Non-escaping within a single compiled function (closures are vm-6), so these
   compile to byte-code jumps with an operand-stack cut + a scratch result slot.
   VM4-CMP proves each compiled result equals the tree-walker's.  Loop MACROS
   (DOTIMES / DO / DOLIST) expand to TAGBODY/GO/BLOCK, so they exercise this path. */
static void test_vm_control(void){
    uword pool = open_live_pool();
    e("(unless (fboundp 'vm4-cmp)"
      "  (defun vm4-cmp (lam args)"
      "    (equal (apply (%vm-compile lam) args) (apply (eval (list 'function lam)) args))))");

    /* BLOCK value + RETURN-FROM (top / mid-expression / conditional / nested) */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x) (block b (+ x 1))) '(5))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x) (block b (return-from b 99) x)) '(5))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x) (block b (+ 1 (return-from b 42)) 7)) '(5))"));
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda (x) (block b (if (> x 0) (return-from b 'pos) 'neg))) '(3))"));
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda () (block o (block i (return-from o 'out)) 'never)) '())"));

    /* TAGBODY / GO hand-written loop */
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda (n)"
              "   (let ((i 0) (s 0))"
              "     (block nil (tagbody top (if (> i n) (return nil))"
              "                         (setq s (+ s i)) (setq i (+ i 1)) (go top)))"
              "     s)) '(6))"));
    /* loop macros expand to TAGBODY/GO/BLOCK */
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda (n) (let ((s 0)) (dotimes (i n s) (setq s (+ s i))))) '(6))"));
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda (n) (do ((i 0 (+ i 1)) (a 1 (* a 2))) ((= i n) a))) '(5))"));
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda (l) (let ((s 0)) (dolist (x l s) (setq s (+ s x))))) '((3 4 5)))"));
    /* early RETURN-FROM out of a loop */
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda (l) (block f (dolist (x l) (if (> x 3) (return-from f x))) nil))"
              "         '((1 2 5 9)))"));
    /* a compiled countdown loop actually iterates (no C-stack recursion) */
    TEST_EQ(eql, I(500500),
            e("(funcall (%vm-compile '(lambda (n) (let ((s 0)) (dotimes (i n s) (setq s (+ s (+ i 1))))))) 1000)"));

    close_live_pool(pool);
}

/* ---- vm-5 (part 2a): multiple values + non-capturing lambdas -------------- */
/* MULTIPLE-VALUE-CALL/-PROG1 (and the -BIND/-LIST/NTH-VALUE macros built on them)
   use BC-MV-LIST + the APPEND/APPLY/VALUES-LIST builtins.  A non-capturing LAMBDA in
   value position compiles to a standalone template pushed as a constant (real
   closures are vm-6).  All checked differentially against the tree-walker. */
static void test_vm_values(void){
    uword pool = open_live_pool();
    e("(unless (fboundp 'vm4-cmp)"
      "  (defun vm4-cmp (lam args)"
      "    (equal (apply (%vm-compile lam) args) (apply (eval (list 'function lam)) args))))");

    /* the FUNCTION values returns MV through RETURN; primary in single-value context */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda () (values 1 2 3)) '())"));
    TEST_EQ(eql, I(3),     e("(funcall (%vm-compile '(lambda (a b) (floor a b))) 17 5)"));
    /* MULTIPLE-VALUE-LIST / -BIND / -CALL / NTH-VALUE / -PROG1 */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda () (multiple-value-list (values 1 2 3))) '())"));
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda () (multiple-value-bind (a b c) (values 10 20 30) (list a b c))) '())"));
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda () (multiple-value-call #'list (values 1 2) (values 3 4))) '())"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda () (nth-value 1 (values 7 8 9))) '())"));
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda () (multiple-value-list"
              "   (multiple-value-prog1 (values 1 2) (values 9 9)))) '())"));
    /* a compiled function returning multiple values, consumed by MULTIPLE-VALUE-BIND */
    TEST_EQ(equal, e("'(3 2)"),
            e("(multiple-value-list (funcall (%vm-compile '(lambda (a b) (floor a b))) 17 5)))"));

    /* non-capturing LAMBDA in value position: funcall + mapcar */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x) (funcall (lambda (y) (* y y)) x)) '(6))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (l) (mapcar (lambda (z) (+ z 1)) l)) '((1 2 3)))"));

    close_live_pool(pool);
}

/* ---- vm-5 (part 2b): dynamic NLX — CATCH/THROW, dynamic binding, UNWIND-PROTECT -- */
/* These use the setjmp/longjmp frame machinery (a frame on g_frame_stack whose setjmp
   is the landing point), interoperating with tree-walked frames.  A lexical exit
   (RETURN-FROM/GO, compiled as a jump) that crosses a dynamic frame emits its unwind
   (cleanup + pop), so specials are restored and cleanups run.  VM4-CMP compares the
   compiled result to the tree-walker; side-effect cases compare directly. */
static void test_vm_nlx(void){
    uword pool = open_live_pool();
    e("(unless (fboundp 'vm4-cmp)"
      "  (defun vm4-cmp (lam args)"
      "    (equal (apply (%vm-compile lam) args) (apply (eval (list 'function lam)) args))))");
    e("(defvar *vm-nlx-sv* 'global)");
    e("(defvar *vm-nlx-log* '())");
    e("(defun vm-nlx-log (x) (setq *vm-nlx-log* (cons x *vm-nlx-log*)) x)");
    e("(defmacro vm-nlx-run (lam &rest args)"          /* reset log, run compiled lam, return (result . log) */
      "  `(progn (setq *vm-nlx-log* '() *vm-nlx-sv* 'global)"
      "          (let ((r (funcall (%vm-compile ,lam) ,@args)))"
      "            (cons r (reverse *vm-nlx-log*)))))");

    /* CATCH / THROW */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda () (catch 'g 42)) '())"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda () (catch 'g (throw 'g 99) 7)) '())"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda () (catch 'a (catch 'b (throw 'a 'out)) 'x)) '())"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda () (catch 'a (+ 1 (catch 'b (throw 'b 10)))) )'())"));
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(multiple-value-list (funcall (%vm-compile '(lambda () (catch 'z (throw 'z (values 1 2 3)))))))"));
    /* THROW across a call boundary (thrower is a separate compiled fn) */
    e("(setf (symbol-function 'vm-nlx-thrower) (%vm-compile '(lambda (v) (throw 'ball v))))");
    TEST_EQ(eql, I(123),
            e("(funcall (%vm-compile '(lambda () (catch 'ball (vm-nlx-thrower 123) 'unreached))))"));
    /* THROW out of a loop */
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda (n) (catch 's (dotimes (i n) (if (= i 3) (throw 's i))) 'done)) '(10))"));

    /* dynamic (special) binding: bound value visible (even in a called fn), restored after */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda () (let ((*vm-nlx-sv* 'b)) *vm-nlx-sv*)) '())"));
    TEST_EQ(eql, e("'global"),
            e("(progn (funcall (%vm-compile '(lambda () (let ((*vm-nlx-sv* 'b)) *vm-nlx-sv*)))) *vm-nlx-sv*)"));
    /* parallel LET: the second init sees the OUTER special value */
    TEST_EQ(equal, e("'(global new global)"),
            e("(funcall (%vm-compile '(lambda () (let ((o *vm-nlx-sv*))"
              "   (let ((*vm-nlx-sv* 'new) (y *vm-nlx-sv*)) (list y *vm-nlx-sv* o))))))"));
    /* special restored on a THROW through the binding */
    TEST_EQ(equal, e("'(temp global)"),
            e("(funcall (%vm-compile '(lambda () (list (catch 'k (let ((*vm-nlx-sv* 'temp))"
              "   (throw 'k *vm-nlx-sv*))) *vm-nlx-sv*))))"));

    /* UNWIND-PROTECT: value + cleanup on normal, on throw, on lexical return-from */
    TEST_EQ(equal, e("'(42 cl)"),
            e("(vm-nlx-run '(lambda () (unwind-protect 42 (vm-nlx-log 'cl))))"));
    TEST_EQ(equal, e("'(thrown cl)"),
            e("(vm-nlx-run '(lambda () (catch 'k (unwind-protect (throw 'k 'thrown) (vm-nlx-log 'cl)))))"));
    TEST_EQ(equal, e("'(ret cl)"),
            e("(vm-nlx-run '(lambda () (block b (unwind-protect (return-from b 'ret) (vm-nlx-log 'cl)))))"));
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(multiple-value-list (funcall (%vm-compile '(lambda () (unwind-protect (values 1 2 3) 0)))))"));
    /* nested UNWIND-PROTECT unwound by a throw: inner cleanup then outer */
    TEST_EQ(equal, e("'(done inner outer)"),
            e("(vm-nlx-run '(lambda () (catch 'k (unwind-protect (unwind-protect (throw 'k 'done)"
              "   (vm-nlx-log 'inner)) (vm-nlx-log 'outer)))))"));

    /* lexical exit crossing BOTH an unwind-protect and a special binding:
       (result . log) shows the cleanup ran seeing the still-bound special... */
    TEST_EQ(equal, e("'(done x)"),
            e("(vm-nlx-run '(lambda () (block b (let ((*vm-nlx-sv* 'x))"
              "   (unwind-protect (return-from b 'done) (vm-nlx-log *vm-nlx-sv*))))))"));
    /* ...and afterwards the special is restored */
    TEST_EQ(eql, e("'global"),
            e("(progn (funcall (%vm-compile '(lambda () (block b (let ((*vm-nlx-sv* 'x))"
              "   (unwind-protect (return-from b 'done) (vm-nlx-log *vm-nlx-sv*)))))))"
              "  *vm-nlx-sv*)"));

    /* PROGV: bind a runtime list of specials; restore (incl. makunbound when short);
       and restore on a throw through the PROGV. */
    e("(defvar *vm-nlx-b* 'gb)");
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda () (progv '(*vm-nlx-sv* *vm-nlx-b*) '(1 2) (list *vm-nlx-sv* *vm-nlx-b*))) '())"));
    TEST_EQ(equal, e("'(1 nil)"),
            e("(funcall (%vm-compile '(lambda () (progv '(*vm-nlx-sv* *vm-nlx-b*) '(1)"
              "   (list *vm-nlx-sv* (boundp '*vm-nlx-b*))))))"));
    TEST_EQ(equal, e("'(temp global)"),
            e("(funcall (%vm-compile '(lambda () (list (catch 'k (progv '(*vm-nlx-sv*) '(temp)"
              "   (throw 'k *vm-nlx-sv*))) *vm-nlx-sv*))))"));

    /* LOAD-TIME-VALUE: evaluated once at compile(=load) time, embedded as a constant */
    TEST_EQ(eql, I(5),  e("(funcall (%vm-compile '(lambda () (load-time-value (+ 2 3)))))"));
    TEST_EQ(equal, e("'(z a b)"),
            e("(funcall (%vm-compile '(lambda (x) (cons x (load-time-value (list 'a 'b))))) 'z)"));

    close_live_pool(pool);
}

/* ---- vm-6 (part 1): closures — capturing lambdas, FLET/LABELS, upvalues -------- */
/* A capturing LAMBDA compiles to MAKE-CLOSURE, which pairs the template with a
   DISPLAY over the current activation's frame ([this frame, ...current upvalues]);
   LOAD-LOCAL depth d>0 reads display[d-1].  Captured frames outlive the enclosing
   call (shared for mutation).  A fn that only relays a deeper frame to an inner
   closure is itself made a closure (max-depth propagation).  Interop (compiled <->
   tree-walked <-> built-in) already flows through kernel_apply. */
static void test_vm_closures(void){
    uword pool = open_live_pool();
    e("(unless (fboundp 'vm4-cmp)"
      "  (defun vm4-cmp (lam args)"
      "    (equal (apply (%vm-compile lam) args) (apply (eval (list 'function lam)) args))))");

    /* immediately-applied capturing lambda; mapcar with a capturing lambda */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (n) (funcall (lambda (x) (+ x n)) 10)) '(5))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (b l) (mapcar (lambda (x) (+ x b)) l)) '(100 (1 2 3)))"));
    /* nested closures, depth 2 (inner reaches the outermost frame via the display chain) */
    TEST_EQ(eql, I(123),
            e("(funcall (%vm-compile '(lambda (a) (funcall (lambda (b) (funcall (lambda (c)"
              "   (+ a b c)) 100)) 20))) 3)"));

    /* returned closures capture distinct bindings */
    e("(defvar *mk* (%vm-compile '(lambda (n) (lambda (x) (+ x n)))))");
    TEST_EQ(eql, I(6),  e("(funcall (funcall *mk* 5) 1)"));
    TEST_EQ(eql, I(11), e("(funcall (funcall *mk* 10) 1)"));
    /* mutable captured counter */
    e("(defvar *ct* (funcall (%vm-compile '(lambda () (let ((c 0)) (lambda () (setq c (+ c 1))))))))");
    TEST_EQ(equal, e("'(1 2 3)"), e("(list (funcall *ct*) (funcall *ct*) (funcall *ct*))"));
    /* two closures sharing one captured variable */
    e("(defvar *pair* (funcall (%vm-compile"
      "  '(lambda () (let ((c 0)) (list (lambda () (setq c (+ c 1))) (lambda () c)))))))");
    TEST_EQ(eql, I(2), e("(progn (funcall (first *pair*)) (funcall (first *pair*)) (funcall (second *pair*)))"));

    /* mutually-recursive LABELS (even?/odd?) and a simple FLET */
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda (n) (labels ((ev (k) (if (= k 0) t (od (- k 1))))"
              "                               (od (k) (if (= k 0) nil (ev (- k 1)))))"
              "                        (list (ev n) (od n)))) '(4))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x) (flet ((dbl (y) (* y 2))) (dbl (dbl x)))) '(3))"));
    /* a compiled closure passed to a tree-walked higher-order fn (interop) */
    TEST_EQ(equal, e("'(2 4 6)"),
            e("(mapcar (funcall (%vm-compile '(lambda (k) (lambda (x) (* x k)))) 2) '(1 2 3))"));

    close_live_pool(pool);
}

TEST_INITIALIZE(test_misc);
TEST_SUITE(test_misc, misc_init, misc_fini)
    TEST_CASE("cfasl primitives",    test_cfasl_primitives)
    TEST_CASE("cfasl writer",        test_cfasl_writer)
    TEST_CASE("cfasl roundtrip",     test_cfasl_roundtrip)
    TEST_CASE("cfasl load file",     test_cfasl_load_file)
    TEST_CASE("cfasl corpus",        test_cfasl_corpus)
    TEST_CASE("vm compiled-function", test_vm_compiled_function)
    TEST_CASE("vm execute",          test_vm_execute)
    TEST_CASE("vm analyze",          test_vm_analyze)
    TEST_CASE("vm codegen",          test_vm_codegen)
    TEST_CASE("vm control",          test_vm_control)
    TEST_CASE("vm values",           test_vm_values)
    TEST_CASE("vm nlx",              test_vm_nlx)
    TEST_CASE("vm closures",         test_vm_closures)
    TEST_CASE("profile interface",   test_profile_interface)
    TEST_CASE("make-array",          test_make_array)
    TEST_CASE("array-info",          test_array_info)
    TEST_CASE("array-in-bounds-p",   test_array_in_bounds)
    TEST_CASE("array-predicates",    test_array_predicates)
    TEST_CASE("make-list",           test_make_list)
    TEST_CASE("copy-seq",            test_copy_seq)
    TEST_CASE("random",              test_random)
    TEST_CASE("probe-file",          test_probe_file)
    TEST_CASE("file-ops",            test_file_ops)
    TEST_CASE("timing",              test_timing)
    TEST_CASE("fill",                test_fill)
    TEST_CASE("replace",             test_replace)
    TEST_CASE("mismatch",            test_mismatch)
    TEST_CASE("write-to-string",     test_write_to_string)
    TEST_FINALIZE()
