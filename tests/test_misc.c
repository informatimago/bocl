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

TEST_INITIALIZE(test_misc);
TEST_SUITE(test_misc, misc_init, misc_fini)
    TEST_CASE("cfasl primitives",    test_cfasl_primitives)
    TEST_CASE("cfasl writer",        test_cfasl_writer)
    TEST_CASE("cfasl roundtrip",     test_cfasl_roundtrip)
    TEST_CASE("cfasl load file",     test_cfasl_load_file)
    TEST_CASE("cfasl corpus",        test_cfasl_corpus)
    TEST_CASE("vm compiled-function", test_vm_compiled_function)
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
