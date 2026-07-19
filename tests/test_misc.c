/* tests/test_misc.c — CUnit tests for Slice 14 miscellaneous utilities:
   MAKE-ARRAY, array ops, COPY-SEQ, MAKE-LIST, RANDOM, file-system,
   timing constants, FILL, REPLACE, MISMATCH, and write-to-string fix. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>   /* open O_WRONLY|O_TRUNC — test_cfasl_validate rewrites its temp image */

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

static Ref e(const char* s){ return bocl_test_eval(s); }

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
              "  (with-open-file (f \"{TMP}/test_misc_1.tmp\" :direction :output"
              "                      :if-exists :supersede)"
              "    (write-string \"hello\" f))"
              "  (if (probe-file \"{TMP}/test_misc_1.tmp\") t nil))"));
    TEST_EQ(equal, e("t"),
            e("(progn"
              "  (rename-file \"{TMP}/test_misc_1.tmp\""
              "               \"{TMP}/test_misc_2.tmp\")"
              "  (if (probe-file \"{TMP}/test_misc_2.tmp\") t nil))"));
    TEST_EQ(equal, e("nil"),
            e("(probe-file \"{TMP}/test_misc_1.tmp\")"));
    /* delete-file returns T; probe-file after deletion returns NIL */
    TEST_EQ(equal, e("t"), e("(delete-file \"{TMP}/test_misc_2.tmp\")"));
    TEST_EQ(equal, e("nil"),
            e("(probe-file \"{TMP}/test_misc_2.tmp\")"));}

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

/* BOCL:VALIDATE-CFASL / %CFASL-VALIDATE — structural validation of a compiled
   image WITHOUT evaluating it (the inspection companion to the v1.8.32
   truncated-image boot defenses). */
static void test_cfasl_validate(void){
    /* Assemble a valid two-form image (one writer context per form, exactly
       like kernel_WRITE_CFASL / the per-form reader). */
    cfasl_out b; cfasl_out_init(&b);
    cfasl_put_bytes(&b, CFASL_MAGIC, CFASL_MAGIC_LEN);
    cfasl_put_u8(&b, (uint8_t)CFASL_VERSION);
    cfasl_put_u8(&b, 0);                                  /* flags */
    {   cfasl_wctx* w = cfasl_wctx_new(&b);
        cfasl_write_object(w, e("'(defun %cfv-never-defined (x) x)"));
        TEST_EQ(eql, NIL_Symbol, cfasl_wctx_error(w) ? T_Symbol : NIL_Symbol);
        cfasl_wctx_free(w); }
    {   cfasl_wctx* w = cfasl_wctx_new(&b);
        cfasl_write_object(w, e("'(list 1 2 3)"));
        TEST_EQ(eql, NIL_Symbol, cfasl_wctx_error(w) ? T_Symbol : NIL_Symbol);
        cfasl_wctx_free(w); }
    cfasl_put_u8(&b, CFASL_TAG_EOF);
    TEST_EQ(eql, NIL_Symbol, b.error ? T_Symbol : NIL_Symbol);

    char path[] = "/tmp/bocl_cfvalXXXXXX";
    int fd = mkstemp(path);
    TEST_EQ(eql, T_Symbol, (fd>=0) ? T_Symbol : NIL_Symbol);
    ssize_t wr = write(fd, b.data, b.len); (void)wr; close(fd);

    char form[512];
    /* Intact image: (values T 2 NIL CFASL_VERSION) — and validation must NOT
       have evaluated the forms (the DEFUN must not take effect). */
    snprintf(form,sizeof form,
             "(equal (multiple-value-list (bocl:validate-cfasl \"%s\" :report nil))"
             "       (list t 2 nil %d))",path,(int)CFASL_VERSION);
    TEST_EQ(eql, T_Symbol, e(form));
    TEST_EQ(eql, NIL_Symbol, e("(fboundp '%cfv-never-defined)"));

    /* Truncated by one octet (the end-of-image tag): invalid, both forms still
       read cleanly, and the message names the missing tag. */
    {   int fd2 = open(path, O_WRONLY|O_TRUNC);
        TEST_EQ(eql, T_Symbol, (fd2>=0) ? T_Symbol : NIL_Symbol);
        ssize_t w2 = write(fd2, b.data, b.len-1); (void)w2; close(fd2);
        snprintf(form,sizeof form,
                 "(multiple-value-bind (ok n msg) (bocl:validate-cfasl \"%s\" :report nil)"
                 "  (list ok n (and (stringp msg) (search \"end-of-image\" msg) t)))",path);
        TEST_EQ(equal, e("'(nil 2 t)"), e(form)); }

    /* Truncated mid-form: invalid, and the message reports a malformed form
       or the missing tag (where the cut lands decides which). */
    {   int fd3 = open(path, O_WRONLY|O_TRUNC);
        TEST_EQ(eql, T_Symbol, (fd3>=0) ? T_Symbol : NIL_Symbol);
        ssize_t w3 = write(fd3, b.data, b.len/2); (void)w3; close(fd3);
        snprintf(form,sizeof form,
                 "(multiple-value-bind (ok n msg) (bocl:validate-cfasl \"%s\" :report nil)"
                 "  (declare (ignore n)) (list ok (stringp msg)))",path);
        TEST_EQ(equal, e("'(nil t)"), e(form)); }

    /* Not a cfasl at all: invalid with a bad-magic message. */
    {   int fd4 = open(path, O_WRONLY|O_TRUNC);
        TEST_EQ(eql, T_Symbol, (fd4>=0) ? T_Symbol : NIL_Symbol);
        ssize_t w4 = write(fd4, "definitely not a fasl", 21); (void)w4; close(fd4);
        snprintf(form,sizeof form,
                 "(multiple-value-bind (ok n msg ver) (bocl:validate-cfasl \"%s\" :report nil)"
                 "  (declare (ignore n)) (list ok ver (and (stringp msg) (search \"magic\" msg) t)))",
                 path);
        TEST_EQ(equal, e("'(nil nil t)"), e(form)); }

    unlink(path);
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
    FILE* f = fopen(bocl_test_path("{TMP}/cfasl_corpus.lisp"), "w");
    TEST_EQ(eql, T_Symbol, f ? T_Symbol : NIL_Symbol);
    fputs("(defpackage :cfc (:use :cl))(in-package :cfc)\n", f);
    for(int i=0;i<200;i++){
        fprintf(f, "(defmacro m%d (x) (list '+ %d x))\n", i, i);
        fprintf(f, "(defun f%d (l) (let ((a 0)) (dolist (e l a) (setq a (+ a (m%d e))))))\n", i, i);
    }
    fclose(f);
    e("(compile-file \"{TMP}/cfasl_corpus.lisp\")");
    TEST_EQ(eql, T_Symbol, e("(%cfasl-file-p \"{TMP}/cfasl_corpus.cfasl\")")); /* binary */
    e("(load \"{TMP}/cfasl_corpus.cfasl\")");
    TEST_EQ(eql, I(552), e("(cfc::f182 '(1 2 3))"));   /* 183+184+185 — survived GC */
    TEST_EQ(eql, I(6),   e("(cfc::f0 '(1 2 3))"));      /* 1+2+3 */

    /* (2) feature coverage: DEFPACKAGE/IN-PACKAGE, EVAL-WHEN, DEFCLASS/DEFMETHOD,
       ratio/float/string/simple-vector literals — binary load == expected. */
    FILE* g = fopen(bocl_test_path("{TMP}/cfasl_feat.lisp"), "w");
    TEST_EQ(eql, T_Symbol, g ? T_Symbol : NIL_Symbol);
    fputs("(defpackage :cfg (:use :cl) (:export #:area))\n(in-package :cfg)\n", g);
    fputs("(eval-when (:load-toplevel :execute) (defparameter *lv* :yes))\n", g);
    fputs("(defclass shp () ((r :initarg :r :accessor r)))\n", g);
    fputs("(defmethod area ((s shp)) (* 3 (r s) (r s)))\n", g);
    fputs("(defvar *rat* 3/4)(defvar *flo* 2.5d0)(defvar *str* \"hi\")(defvar *vec* #(1 2 3))\n", g);
    fputs("(in-package :cl-user)\n", g);
    fclose(g);
    e("(compile-file \"{TMP}/cfasl_feat.lisp\")");
    e("(load \"{TMP}/cfasl_feat.cfasl\")");
    TEST_EQ(eql, T_Symbol, e("(if (eq cfg::*lv* :yes) t nil)"));           /* eval-when :load */
    TEST_EQ(eql, I(48),    e("(cfg:area (make-instance 'cfg::shp :r 4))")); /* class + method */
    TEST_EQ(eql, T_Symbol, e("(if (= cfg::*rat* 3/4) t nil)"));            /* ratio */
    TEST_EQ(eql, T_Symbol, e("(if (= cfg::*flo* 2.5d0) t nil)"));          /* double-float */
    TEST_EQ(equal, e("\"hi\""), e("cfg::*str*"));                          /* string */
    TEST_EQ(eql, I(2),     e("(aref cfg::*vec* 1)"));                      /* simple-vector */
    /* leave no fixtures behind (tests-fixed-tmp-paths) */
    e("(ignore-errors (delete-file \"{TMP}/cfasl_corpus.lisp\"))");
    e("(ignore-errors (delete-file \"{TMP}/cfasl_corpus.cfasl\"))");
    e("(ignore-errors (delete-file \"{TMP}/cfasl_feat.lisp\"))");
    e("(ignore-errors (delete-file \"{TMP}/cfasl_feat.cfasl\"))");
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

/* FLET vs LABELS scoping conformance (issue interp-flet-sibling-visibility,
   fixed v1.8.36): a FLET function's body must NOT see its siblings (they
   resolve to the GLOBAL function), and a local function's body is closed over
   its DEFINING environment, not the call site's.  LABELS keeps sibling
   visibility.  Both the tree-walker and the VM must agree. */

/* EQL specializers (CLHS 7.6.2, added for UIOP's slurp-input-stream) + the
   READ-SEQUENCE fill-position return (CLHS 21.2; it used to return END
   unconditionally, sending UIOP's COPY-STREAM-TO-STREAM into an infinite
   loop) — the two conformance fixes behind uiop run-program (v1.8.40). */
static void test_eql_specializers_and_read_sequence(void){
    e("(defgeneric %eqs (x))");
    e("(defmethod %eqs ((x integer)) :int)");
    e("(defmethod %eqs ((x (eql 42))) :answer)");
    e("(defmethod %eqs ((x (eql :k))) :kw)");
    e("(defmethod %eqs ((x t)) :any)");
    TEST_EQ(equal, e("'(:answer :int :kw :any)"),
            e("(list (%eqs 42) (%eqs 7) (%eqs :k) (%eqs \"s\"))"));
    /* eql method sees the class method through call-next-method */
    e("(defmethod %eqs ((x (eql 0))) (list :zero (call-next-method)))");
    TEST_EQ(equal, e("'(:zero :int)"), e("(%eqs 0)"));
    /* redefinition replaces (EQUAL specializer dedup) */
    e("(defmethod %eqs ((x (eql 42))) :answer2)");
    TEST_EQ(eql, e(":answer2"), e("(%eqs 42)"));
    /* read-sequence returns the FILL POSITION, 0 at EOF */
    TEST_EQ(equal, e("'(3 0)"),
            e("(progn (with-open-file (o \"{TMP}/rs_test.tmp\" :direction :output"
              "                          :if-exists :supersede) (princ \"abc\" o))"
              "  (with-open-file (i \"{TMP}/rs_test.tmp\")"
              "    (let ((buf (make-string 8)))"
              "      (list (read-sequence buf i) (read-sequence buf i)))))"));
    e("(delete-file \"{TMP}/rs_test.tmp\")");
    /* delete-file on a missing file signals FILE-ERROR (CLHS) */
    TEST_EQ(eql, e(":fe"),
            e("(handler-case (delete-file \"{TMP}/no_such_file.tmp\")"
              "  (file-error () :fe))"));
}

static void test_flet_scoping(void){
    /* interpreted */
    TEST_EQ(eql, e(":UNDEF"),
            e("(handler-case (flet ((%fs-g () 1) (%fs-h () (%fs-g))) (%fs-h))"
              "  (error () :undef))"));
    e("(defun %fs-g2 () :global)");
    TEST_EQ(eql, e(":GLOBAL"),
            e("(flet ((%fs-g2 () :local) (%fs-h2 () (%fs-g2))) (%fs-h2))"));
    TEST_EQ(eql, e(":OUTER"),
            e("(flet ((%fs-f () :outer))"
              "  (flet ((%fs-f () :inner) (%fs-h3 () (%fs-f))) (%fs-h3)))"));
    TEST_EQ(eql, I(1),
            e("(labels ((%fs-g4 () 1) (%fs-h4 () (%fs-g4))) (%fs-h4))"));
    /* defining-env closure: the flet body sees x=10, never the call site's 20 */
    TEST_EQ(eql, I(10),
            e("(let ((x 10)) (flet ((%fs-g5 () x)) (let ((x 20)) x (%fs-g5))))"));
    TEST_EQ(eql, I(10),
            e("(let ((x 10)) (flet ((%fs-g6 () x))"
              "  (let ((x 20)) x (funcall (function %fs-g6)))))"));
    /* compiled — same five answers */
    TEST_EQ(eql, e(":UNDEF"),
            e("(handler-case (funcall (compile nil '(lambda ()"
              "    (flet ((%fs-cg () 1) (%fs-ch () (%fs-cg))) (%fs-ch)))))"
              "  (error () :undef))"));
    TEST_EQ(eql, e(":GLOBAL"),
            e("(funcall (compile nil '(lambda ()"
              "  (flet ((%fs-g2 () :local) (%fs-h2 () (%fs-g2))) (%fs-h2)))))"));
    TEST_EQ(eql, I(1),
            e("(funcall (compile nil '(lambda ()"
              "  (labels ((%fs-cg4 () 1) (%fs-ch4 () (%fs-cg4))) (%fs-ch4)))))"));
    TEST_EQ(eql, I(10),
            e("(funcall (compile nil '(lambda ()"
              "  (let ((x 10)) (flet ((%fs-cg5 () x)) (let ((x 20)) x (%fs-cg5)))))))"));
}

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

    /* --- vm-6 part 2: the extended lambda list via the RAW-entry + prologue path.
       &optional with a non-NIL default / supplied-p, &key, &aux — all compared to the
       tree-walker on the same (well-formed) argument lists. --- */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x &optional (y 10)) (list x y)) '(1))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x &optional (y 10)) (list x y)) '(1 2))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (&optional (y 0 yp)) (list y yp)) '(5))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (&optional (y 0 yp)) (list y yp)) '())"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x &optional (y (* x 2))) (list x y)) '(3))"));  /* default sees x */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (&key a b) (list a b)) '(:a 1 :b 2))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (&key a b) (list a b)) '(:b 2 :a 1))"));          /* order */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (&key (a 9)) a) '())"));                          /* key default */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (&key (a 0 ap)) (list a ap)) '(:a 7))"));         /* key supplied-p */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (&key (a 0 ap)) (list a ap)) '())"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x &optional (y 2) &key (z 3)) (list x y z)) '(1 7 :z 9))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x &optional (y 2) &key (z 3)) (list x y z)) '(1))"));
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (x &aux (y (+ x 1))) (list x y)) '(5))"));         /* &aux */
    TEST_EQ(eql, T_Symbol, e("(vm4-cmp '(lambda (&rest r &key a) (list r a)) '(:a 1))"));          /* &rest + &key */
    /* a closure capturing an &optional parameter bound by the prologue */
    TEST_EQ(eql, I(10), e("(funcall (%vm-compile '(lambda (&optional (n 10)) (funcall (lambda () n)))))"));

    close_live_pool(pool);
}

/* ---- vm-8 (part 1): byte-code fasl — serialize compiled-function templates ---- */
/* cfasl gained CFASL_TAG_COMPILED_FUNCTION (a template: name, lambda-list, code octet
   string, constant pool — which may nest templates for FLET/closures — arity,
   max-stack, n-locals, source).  A round-tripped template must run identically. */
static void test_vm_fasl(void){
    uword pool = open_live_pool();

    /* a plain compiled fn round-trips and still runs */
    Ref f = cfasl_roundtrip("(%vm-compile '(lambda (x) (+ (* x x) 1)))", pool);
    add_to_live_pool(f);
    TEST_EQ(eql, T_Symbol, compiled_function_p(f) ? T_Symbol : NIL_Symbol);
    e("(defvar *fasl-f* nil)");
    set_symbol_value(e("'*fasl-f*"), f);
    TEST_EQ(eql, I(50), e("(funcall *fasl-f* 7)"));

    /* nested templates (a closure-making fn) survive: its constant pool holds the
       inner template; round-trip, then build + call the closure */
    e("(defvar *fasl-mk* (%vm-compile '(lambda (n) (lambda (x) (+ x n)))))");
    Ref mk = cfasl_roundtrip("*fasl-mk*", pool); add_to_live_pool(mk);
    TEST_EQ(eql, T_Symbol, compiled_function_p(mk) ? T_Symbol : NIL_Symbol);
    e("(defvar *fasl-mk2* nil)");
    set_symbol_value(e("'*fasl-mk2*"), mk);
    TEST_EQ(eql, I(15), e("(funcall (funcall *fasl-mk2* 10) 5)"));

    /* a fn with the extended lambda list (RAW-entry + prologue) round-trips */
    e("(defvar *fasl-opt* (%vm-compile '(lambda (x &key (y 10)) (list x y))))");
    Ref g = cfasl_roundtrip("*fasl-opt*", pool); add_to_live_pool(g);
    e("(defvar *fasl-opt2* nil)");
    set_symbol_value(e("'*fasl-opt2*"), g);
    TEST_EQ(equal, e("'(1 10)"), e("(funcall *fasl-opt2* 1)"));
    TEST_EQ(equal, e("'(1 7)"),  e("(funcall *fasl-opt2* 1 :y 7)"));

    close_live_pool(pool);
}

/* ---- vm-conditions: HANDLER-BIND / HANDLER-CASE / IGNORE-ERRORS ---------------- */
/* HANDLER-BIND compiles to a frame_handler_bind (a no-cleanup dynamic frame the SIGNAL
   machinery walks and CALLS into).  HANDLER-CASE is expanded by the analyzer into
   BLOCK/RETURN-FROM + CATCH/THROW + HANDLER-BIND with capturing handler closures — all
   already supported.  Verified vs the tree-walker where results are values. */
static void test_vm_conditions(void){
    uword pool = open_live_pool();
    e("(unless (fboundp 'vm4-cmp)"
      "  (defun vm4-cmp (lam args)"
      "    (equal (apply (%vm-compile lam) args) (apply (eval (list 'function lam)) args))))");

    /* IGNORE-ERRORS: (values nil condition) on error, the value on success */
    TEST_EQ(equal, e("'(nil t)"),
            e("(funcall (%vm-compile '(lambda () (multiple-value-bind (v c)"
              "   (ignore-errors (error \"boom\") 99) (list v (if c t nil))))))"));
    TEST_EQ(equal, e("'(3 nil)"),
            e("(funcall (%vm-compile '(lambda () (multiple-value-bind (v c)"
              "   (ignore-errors (+ 1 2)) (list v c)))))"));

    /* HANDLER-CASE: catch + bind the condition; normal value; type dispatch */
    TEST_EQ(equal, e("'(:caught \"x\")"),
            e("(funcall (%vm-compile '(lambda () (handler-case (error \"x\")"
              "   (error (c) (list :caught (princ-to-string c)))))))"));
    TEST_EQ(eql, T_Symbol,
            e("(vm4-cmp '(lambda (x) (handler-case (* x x) (error () -1))) '(6))"));
    TEST_EQ(eql, e("':te"),
            e("(funcall (%vm-compile '(lambda () (handler-case"
              "   (error 'type-error :datum 1 :expected-type 'string)"
              "   (division-by-zero () :dbz) (type-error () :te) (error () :err)))))"));

    /* HANDLER-BIND: a handler that transfers control (THROW out); and one that
       DECLINES (returns) so an enclosing HANDLER-CASE runs */
    TEST_EQ(eql, e("':handled"),
            e("(funcall (%vm-compile '(lambda () (catch 'out (handler-bind"
              "   ((error (lambda (c) (declare (ignore c)) (throw 'out :handled))))"
              "   (error \"e\") :unreached)))))"));
    TEST_EQ(eql, e("':outer"),
            e("(funcall (%vm-compile '(lambda () (handler-case"
              "   (handler-bind ((error (lambda (c) (declare (ignore c)) nil))) (error \"e\"))"
              "   (error () :outer)))))"));
    /* nested HANDLER-CASE: the inner one catches */
    TEST_EQ(eql, e("':inner"),
            e("(funcall (%vm-compile '(lambda () (handler-case"
              "   (handler-case (error \"e\") (error () :inner)) (error () :outer)))))"));

    /* the block-multiple-values fix (BLOCK/RETURN-FROM now preserve all values) */
    TEST_EQ(equal, e("'(7 8 9)"),
            e("(funcall (%vm-compile '(lambda () (multiple-value-list"
              "   (block b (return-from b (values 7 8 9)))))))"));
    TEST_EQ(equal, e("'(1 2 3)"),
            e("(funcall (%vm-compile '(lambda () (multiple-value-list"
              "   (block b (values 1 2 3))))))"));

    close_live_pool(pool);
}

/* ---- vm-conditions RESTART: RESTART-BIND / RESTART-CASE / INVOKE-RESTART ------- */
/* RESTART-BIND is a frame_restart the shared FIND-RESTART/INVOKE-RESTART walk; RESTART-CASE
   is analyzer-expanded to RESTART-BIND + CATCH/THROW.  A compiled RESTART-CASE + INVOKE-
   RESTART must round-trip, bind the clause lambda-list to the invocation args, be invocable
   from a HANDLER-BIND handler, and match the tree-walker. */
static void test_vm_restarts(void){
    uword pool = open_live_pool();

    /* RESTART-CASE normal completion vs restart invoked */
    e("(setf (symbol-function 'r-basic) (%vm-compile"
      "  '(lambda (x) (restart-case (if (< x 0) (invoke-restart 'use-zero) x) (use-zero () 0)))))");
    TEST_EQ(eql, T_Symbol, e("(if (compiled-function-p (symbol-function 'r-basic)) t nil)"));
    TEST_EQ(eql, I(5), e("(r-basic 5)"));      /* form returns normally */
    TEST_EQ(eql, I(0), e("(r-basic -1)"));     /* restart invoked -> clause runs */

    /* invocation args bound to the clause's (full) lambda-list */
    e("(setf (symbol-function 'r-args) (%vm-compile"
      "  '(lambda () (restart-case (invoke-restart 'uv 40 2) (uv (a b) (+ a b))))))");
    TEST_EQ(eql, I(42), e("(r-args)"));
    /* &rest in a clause lambda-list */
    e("(setf (symbol-function 'r-rest) (%vm-compile"
      "  '(lambda () (restart-case (invoke-restart 'uv 1 2 3) (uv (&rest r) r)))))");
    TEST_EQ(equal, e("'(1 2 3)"), e("(r-rest)"));

    /* clause selection: the right one of several runs */
    e("(setf (symbol-function 'r-multi) (%vm-compile"
      "  '(lambda (n) (restart-case (invoke-restart (case n (1 'a) (2 'b) (t 'c)))"
      "                 (a () :a) (b () :b) (c () :c)))))");
    TEST_EQ(eql, e("':a"), e("(r-multi 1)"));
    TEST_EQ(eql, e("':b"), e("(r-multi 2)"));
    TEST_EQ(eql, e("':c"), e("(r-multi 9)"));

    /* a HANDLER-BIND handler invokes a restart established (in compiled code) around SIGNAL */
    e("(setf (symbol-function 'r-hand) (%vm-compile"
      "  '(lambda (x) (handler-bind ((error (lambda (c) (declare (ignore c)) (invoke-restart 'cont 99))))"
      "                 (restart-case (if (< x 0) (error \"neg\") x) (cont (v) v))))))");
    TEST_EQ(eql, I(7),  e("(r-hand 7)"));
    TEST_EQ(eql, I(99), e("(r-hand -1)"));

    /* RESTART-BIND: the restart function is applied IN PLACE (it does its own transfer —
       here a THROW; an escaping RETURN-FROM out of the fn closure is a separate vm-6 gap). */
    e("(setf (symbol-function 'r-bind) (%vm-compile"
      "  '(lambda (x) (catch 'done (restart-bind ((jump (lambda (v) (throw 'done (* v 10)))))"
      "                              (if (< x 0) (invoke-restart 'jump 5) x))))))");
    TEST_EQ(eql, I(3),  e("(r-bind 3)"));
    TEST_EQ(eql, I(50), e("(r-bind -1)"));

    /* FIND-RESTART sees the compiled restart; nested RESTART-CASE picks the inner one */
    TEST_EQ(eql, T_Symbol, e("(if (funcall (%vm-compile"
                            "  '(lambda () (restart-case (and (find-restart 'r1) t) (r1 () nil))))) t nil)"));
    e("(setf (symbol-function 'r-nest) (%vm-compile"
      "  '(lambda () (restart-case (restart-case (invoke-restart 'r) (r () :inner)) (r () :outer)))))");
    TEST_EQ(eql, e("':inner"), e("(r-nest)"));

    /* parity with the tree-walker */
    TEST_EQ(eql, e("(let ((x -1)) (restart-case (if (< x 0) (invoke-restart 'z) x) (z () 0)))"),
            e("(r-basic -1)"));

    close_live_pool(pool);
}

/* ---- escaping RETURN-FROM: a nested closure returns from an outer BLOCK ------- */
/* A BLOCK whose RETURN-FROM crosses a lambda boundary can't be a local JUMP; it compiles to
   a UNIQUE per-activation tag (a fresh cons) stored in a local and captured by the closure
   through the display, and RETURN-FROM THROWs to it (unwinding the intervening frames).  The
   identity is LEXICAL: two blocks with the SAME name resolve by scope, not dynamically. */
static void test_vm_escaping_blocks(void){
    uword pool = open_live_pool();

    /* classic early-exit via an escaping closure */
    TEST_EQ(eql, I(-3), e("(funcall (compile nil"
        "  '(lambda (l) (block done (mapc (lambda (x) (when (minusp x) (return-from done x))) l) nil)))"
        "  '(1 2 -3 4))"));
    /* it is compiled, not a fallback */
    TEST_EQ(eql, T_Symbol, e("(if (compiled-function-p (compile nil"
        "  '(lambda (l) (block done (mapc (lambda (x) (return-from done x)) l) nil)))) t nil)"));
    /* escaping RETURN-FROM carries MULTIPLE VALUES */
    TEST_EQ(equal, e("'(7 14)"), e("(funcall (compile nil"
        "  '(lambda () (multiple-value-list (block b (mapc (lambda (x) (return-from b (values x (* x 2)))) '(7)) nil)))))"));
    /* an escaping RETURN-FROM runs a crossed UNWIND-PROTECT cleanup */
    TEST_EQ(equal, e("'(:cleanup)"), e("(funcall (compile nil"
        "  '(lambda () (let ((log '())) (block b (unwind-protect (mapc (lambda (x) (return-from b x)) '(9))"
        "                                          (push :cleanup log))) log))))"));
    /* recursion: each activation's BLOCK is a distinct target (fresh tag per call) */
    TEST_EQ(equal, e("'(2 2)"), e("(funcall (compile nil"
        "  '(lambda () (labels ((f (n) (block out"
        "                                (mapc (lambda (x) (when (> x 0) (return-from out (list n x)))) (list n))"
        "                                (if (> n 0) (f (1- n)) :base)))) (f 2)))))"));

    /* THE LEXICAL CASE — two DIFFERENTLY-named blocks, mutually-recursive escaping lambdas */
    e("(setf (symbol-function 'ev-ex1) (compile nil"
      "  '(lambda () (loop for x from 42 to 43 collect"
      "     (list 'block-odd (block odd (list 'block-even (block even"
      "       (let (o e) (setf o (lambda (x e) (if (oddp x) (return-from odd x) (funcall e x o)))"
      "                        e (lambda (x o) (if (evenp x) (return-from even x) (funcall o x e))))"
      "                  (funcall o x e))))))))))");
    TEST_EQ(equal, e("'((block-odd (block-even 42)) (block-odd 43))"), e("(ev-ex1)"));

    /* two SAME-named blocks (`operation`) referenced by two closures — must resolve by LEXICAL
       scope (o -> the outer block, e -> the inner), NOT by dynamic nearest-name. */
    e("(setf (symbol-function 'ev-ex2) (compile nil"
      "  '(lambda () (loop for x from 42 to 43 collect"
      "     (let (o e) (list 'block-odd (block operation"
      "       (setf o (lambda (x e) (if (oddp x) (return-from operation x) (funcall e x o))))"
      "       (list 'block-even (block operation"
      "         (setf e (lambda (x o) (if (evenp x) (return-from operation x) (funcall o x e))))"
      "         (funcall o x e))))))))))");
    TEST_EQ(equal, e("'((block-odd (block-even 42)) (block-odd 43))"), e("(ev-ex2)"));

    /* the TREE-WALKER now agrees (each BLOCK gets its own child env, so same-named nested
       blocks are lexically distinct) — evaluate ex2's body directly (interpreted). */
    TEST_EQ(equal, e("'((block-odd (block-even 42)) (block-odd 43))"),
            e("(loop for x from 42 to 43 collect"
              "  (let (o e) (list 'block-odd (block operation"
              "    (setf o (lambda (x e) (if (oddp x) (return-from operation x) (funcall e x o))))"
              "    (list 'block-even (block operation"
              "      (setf e (lambda (x o) (if (evenp x) (return-from operation x) (funcall o x e))))"
              "      (funcall o x e)))))))"));

    /* ---- escaping GO (same mechanism as RETURN-FROM: a nested closure GOes to an outer
       TAGBODY's tag; the tag's ordinal is THROWn, a re-entrant catch dispatches to it) ---- */
    /* early exit from inside a closure */
    TEST_EQ(eql, e("':neg"), e("(funcall (compile nil"
        "  '(lambda (l) (let ((r :none)) (tagbody"
        "     (mapc (lambda (x) (when (minusp x) (go found))) l) (go done)"
        "     found (setq r :neg) done) r))) '(1 -2 3))"));
    TEST_EQ(eql, e("':none"), e("(funcall (compile nil"
        "  '(lambda (l) (let ((r :none)) (tagbody"
        "     (mapc (lambda (x) (when (minusp x) (go found))) l) (go done)"
        "     found (setq r :neg) done) r))) '(1 2 3))"));
    TEST_EQ(eql, T_Symbol, e("(if (compiled-function-p (compile nil"
        "  '(lambda () (let (r) (tagbody (funcall (lambda () (go d))) d) r)))) t nil)"));  /* not a fallback */
    /* a BACKWARD escaping GO (loop) across a closure terminates and collects 0 1 2 */
    TEST_EQ(equal, e("'(0 1 2)"), e("(funcall (compile nil"
        "  '(lambda () (let ((acc '()) (i 0)) (tagbody"
        "     top (funcall (lambda () (push i acc) (incf i) (when (< i 3) (go top)))) done)"
        "     (nreverse acc)))))"));
    /* mutually-recursive escaping GO between two tags (the GO analog of ev-ex1) */
    TEST_EQ(equal, e("'((:odd 3) (:even 2) (:odd 1) (:even 0))"), e("(funcall (compile nil"
        "  '(lambda () (let ((out '()) o e) (tagbody"
        "     (setf o (lambda (n) (push (list :odd n) out) (if (> n 0) (funcall e (1- n)) (go done)))"
        "           e (lambda (n) (push (list :even n) out) (if (> n 0) (funcall o (1- n)) (go done))))"
        "     (funcall o 3) done) (nreverse out)))))"));

    close_live_pool(pool);
}

/* ---- non-local exit to an EXITED extent (per pjb) ------------------------------ */
/* RETURN-FROM a BLOCK / GO to a TAGBODY whose extent has already exited signals a
   descriptive TYPED condition (naming the block/tags) IN the RETURN-FROM/GO's context —
   BEFORE unwinding — so a HANDLER-BIND may divert.  Works in the tree-walker AND compiled;
   the fix also repaired HANDLER-BIND on ANY user CLOS condition class (it crashed before). */
static void test_vm_exited_extent(void){
    uword pool = open_live_pool();

    /* pre-existing bug this feature required fixing: HANDLER-BIND on a user CLOS condition */
    e("(define-condition tvee-c (error) ())");
    TEST_EQ(eql, e("':diverted"),
            e("(block a (handler-bind ((tvee-c (lambda (c) (declare (ignore c)) (return-from a :diverted))))"
              "           (error 'tvee-c)))"));

    /* INTERPRETED: return-from an exited block -> typed condition naming the block.  The
       conditions are bocl EXTENSIONS, exported from the BOCL package (BOCL:...). */
    e("(defvar *tvee-c* (let (c) (block g (setf c (lambda () (return-from g 1)))) c))");
    TEST_EQ(equal, e("(list 'bocl:return-from-exited-block-error 'g)"),
            e("(handler-case (funcall *tvee-c*)"
              "  (bocl:return-from-exited-block-error (e)"
              "    (list (type-of e) (bocl:return-from-exited-block-error-block-name e))))"));
    /* and a HANDLER-BIND diverts (the bolded requirement: alternative non-local exit) */
    TEST_EQ(eql, I(42),
            e("(block alt (handler-bind ((bocl:return-from-exited-block-error (lambda (e) (declare (ignore e)) (return-from alt 42))))"
              "             (funcall *tvee-c*)))"));

    /* INTERPRETED: go to an exited tagbody -> typed condition */
    e("(defvar *tvee-g* (let (c) (tagbody (setf c (lambda () (go done))) done) c))");
    TEST_EQ(eql, e("'bocl:go-exited-tagbody-error"),
            e("(handler-case (funcall *tvee-g*) (bocl:go-exited-tagbody-error (e) (type-of e)))"));

    /* COMPILED: same, through the byte-code VM (BC_THROW's not-found path) */
    e("(defvar *tvee-cc* (funcall (compile nil '(lambda () (block g (let (c) (setf c (lambda () (return-from g 1))) c)))))) ");
    TEST_EQ(equal, e("(list 'bocl:return-from-exited-block-error 'g)"),
            e("(handler-case (funcall *tvee-cc*)"
              "  (bocl:return-from-exited-block-error (e)"
              "    (list (type-of e) (bocl:return-from-exited-block-error-block-name e))))"));
    TEST_EQ(eql, e("':caught"),
            e("(handler-case (funcall (funcall (compile nil '(lambda () (let (c) (tagbody (setf c (lambda () (go d))) d) c)))))"
              "  (bocl:go-exited-tagbody-error () :caught))"));

    close_live_pool(pool);
}

/* ---- vm-8 (part 2): COMPILE / COMPILE-FILE emit byte-code -------------------- */
static void test_vm_compile_wiring(void){
    uword pool = open_live_pool();

    /* COMPILE now byte-compiles (compiled-function-p is true) ... */
    TEST_EQ(eql, T_Symbol, e("(if (compiled-function-p (compile nil '(lambda (x) (* x x)))) t nil)"));
    /* ... a compiled closure ... */
    TEST_EQ(eql, T_Symbol, e("(if (compiled-function-p (compile nil '(lambda (n) (lambda (x) (+ x n))))) t nil)"));
    /* ... and FALLS BACK (interpreted) for a form the VM can't handle — a top-level def-form
       (DEFVAR) nested in a lambda, which the analyzer errors on — but which still runs. */
    TEST_EQ(eql, NIL_Symbol,
            e("(if (compiled-function-p (compile nil"
              "   '(lambda () (progn (defvar *vm-fb-probe* 99) *vm-fb-probe*)))) t nil)"));
    TEST_EQ(eql, I(99), e("(funcall (compile nil"
              "   '(lambda () (progn (defvar *vm-fb-probe* 99) *vm-fb-probe*))))"));
    /* results match the interpreter */
    TEST_EQ(eql, I(36), e("(funcall (compile nil '(lambda (x) (+ (* x x) (* 2 x) 1))) 5)"));

    /* COMPILE-FILE emits a byte-code fasl; LOAD installs the templates (no recompile) */
    e("(with-open-file (f \"{TMP}/vm8_cf.lisp\" :direction :output :if-exists :supersede)"
      "  (write-line \"(in-package :common-lisp)\" f)"
      "  (write-line \"(defun vm8cf-sq (x) (* x x))\" f)"
      "  (write-line \"(defun vm8cf-sum (n) (let ((s 0)) (dotimes (i n s) (setq s (+ s i)))))\" f)"
      "  (write-line \"(defun vm8cf-fact (n) (if (< n 2) 1 (* n (vm8cf-fact (- n 1)))))\" f))");
    Ref fasl = e("(compile-file \"{TMP}/vm8_cf.lisp\")");
    add_to_live_pool(fasl);
    TEST_EQ(eql, T_Symbol, e("(if (%cfasl-file-p (namestring (compile-file-pathname \"{TMP}/vm8_cf.lisp\"))) t nil)"));
    e("(load (compile-file-pathname \"{TMP}/vm8_cf.lisp\"))");
    /* the loaded functions are byte-code (installed from the fasl) and run */
    TEST_EQ(eql, T_Symbol, e("(if (compiled-function-p (symbol-function 'vm8cf-sq)) t nil)"));
    TEST_EQ(eql, I(49),   e("(vm8cf-sq 7)"));
    TEST_EQ(eql, I(4950), e("(vm8cf-sum 100)"));
    TEST_EQ(eql, I(720),  e("(vm8cf-fact 6)"));
    e("(ignore-errors (delete-file \"{TMP}/vm8_cf.lisp\"))");
    e("(ignore-errors (delete-file (compile-file-pathname \"{TMP}/vm8_cf.lisp\")))");

    close_live_pool(pool);
}

/* ---- vm-10: compiled-prelude boot image regressions --------------------------- */
/* Booting from a byte-compiled prelude.cfasl uncovered two defects, guarded here:
   (1) SET-SYMBOL-FUNCTION must accept a byte-compiled COMPILED-CLOSURE (DEFGENERIC
       installs a byte-compiled discriminating function) — it used to reject anything
       that was not an interpreted FUNCTION / closure / built-in.
   (2) COMPILE-FILE must keep a DEFPARAMETER (and its docstring) RAW rather than
       macroexpanding it at compile time — a compile-time expansion bakes in the
       (SETF (DOCUMENTATION ...)) setf-expander, which on a fresh boot is not yet
       registered.  The DEFUN in the same file is still byte-compiled. */
static void test_vm_compiled_prelude(void){
    uword pool = open_live_pool();

    /* (1) install a genuine COMPILED-CLOSURE (captures a free variable) as a
       symbol-function and call it — this is what DEFGENERIC does at boot. */
    e("(defvar *vmcp-mk* (compile nil '(lambda (n) (lambda (x) (+ x n)))))");
    e("(defvar *vmcp-clo* (funcall *vmcp-mk* 10))");
    TEST_EQ(eql, T_Symbol, e("(if (compiled-function-p *vmcp-clo*) t nil)"));
    e("(setf (symbol-function 'vmcp-add10) *vmcp-clo*)");     /* must not TYPE-ERROR */
    TEST_EQ(eql, I(15), e("(vmcp-add10 5)"));

    /* (2) a DEFPARAMETER with a docstring + a DEFUN round-trip through a cfasl:
       COMPILE-FILE keeps the DEFPARAMETER raw, so LOAD never errors on the
       docstring's (SETF (DOCUMENTATION ...)) — it expands in boot order — and the
       parameter's value is correct; the DEFUN loads byte-compiled. */
    e("(with-open-file (f \"{TMP}/vm10_cf.lisp\" :direction :output :if-exists :supersede)"
      "  (write-line \"(in-package :common-lisp)\" f)"
      "  (write-line \"(defparameter *vm10-p* (list 'sentinel) \\\"a doc string\\\")\" f)"
      "  (write-line \"(defun vm10-cube (x) (* x x x))\" f))");
    e("(load (compile-file \"{TMP}/vm10_cf.lisp\"))");   /* must not error on the docstring */
    TEST_EQ(eql, T_Symbol, e("(if (compiled-function-p (symbol-function 'vm10-cube)) t nil)"));
    TEST_EQ(eql, I(27),    e("(vm10-cube 3)"));
    TEST_EQ(equal, e("'(sentinel)"), e("*vm10-p*"));
    e("(ignore-errors (delete-file \"{TMP}/vm10_cf.lisp\"))");
    e("(ignore-errors (delete-file (compile-file-pathname \"{TMP}/vm10_cf.lisp\")))");

    close_live_pool(pool);
}

/* ---- vm-6b: tail calls (trampoline) + stack-overflow signal ------------------- */
/* A call in tail position compiles to TAIL_CALL / TAIL_CALL_NAMED; the trampoline in
   bytecode_apply reuses the C frame for a compiled callee, so tail recursion runs in
   constant C-stack + heap space.  Deep NON-tail recursion signals a catchable
   STORAGE-CONDITION instead of crashing. */
static void test_vm_tail_calls(void){
    uword pool = open_live_pool();

    /* deep self tail recursion — constant space, no C-stack crash (1e7 iterations) */
    e("(setf (symbol-function 'vmt-count) (%vm-compile"
      "  '(lambda (n acc) (if (= n 0) acc (vmt-count (- n 1) (+ acc 1))))))");
    TEST_EQ(eql, I(1000000), e("(vmt-count 1000000 0)"));
    /* mutual tail recursion across two compiled functions (even?/odd?), 1e6 deep */
    e("(setf (symbol-function 'vmt-even) (%vm-compile '(lambda (n) (if (= n 0) t (vmt-odd (- n 1))))))");
    e("(setf (symbol-function 'vmt-odd)  (%vm-compile '(lambda (n) (if (= n 0) nil (vmt-even (- n 1))))))");
    TEST_EQ(eql, T_Symbol,   e("(vmt-even 200000)"));
    TEST_EQ(eql, NIL_Symbol, e("(vmt-even 199999)"));

    /* a closure created inside a tail loop captures a DISTINCT frame per iteration
       (the trampoline does NOT reuse an escaped frame) */
    e("(setf (symbol-function 'vmt-adders) (%vm-compile"
      "  '(lambda (n acc) (if (= n 0) acc"
      "     (vmt-adders (- n 1) (cons (let ((k n)) (lambda () k)) acc))))))");
    TEST_EQ(equal, e("'(1 2 3 4 5)"), e("(mapcar #'funcall (vmt-adders 5 nil))"));

    /* a tail call to a BUILT-IN falls back correctly */
    TEST_EQ(eql, I(7), e("(funcall (%vm-compile '(lambda (l) (car l))) '(7 8 9))"));

    /* (The deep NON-tail overflow -> catchable STORAGE-CONDITION is verified in the
       REPL, where the control-stack guard is armed; the test harness does not call
       eval_init_stack_guard, so an intentional overflow here would truly crash.) */

    close_live_pool(pool);
}

/* ---- fast-apply: CALL/CALL_NAMED bind operand-stack args directly ------------- */
/* When the callee is a compiled function/closure, CALL binds the operand-stack args
   straight into the callee (bytecode_apply_stack) with no arg-list cons.  It must be
   semantically identical to the list path across every arity shape, and multiple values
   must still propagate. */
static void test_vm_fast_apply(void){
    uword pool = open_live_pool();

    /* required args, compiled caller -> compiled callee (the fast path) */
    e("(setf (symbol-function 'fa-add) (%vm-compile '(lambda (a b c) (+ a (* b c)))))");
    e("(setf (symbol-function 'fa-use) (%vm-compile '(lambda (x) (fa-add x x x))))");
    TEST_EQ(eql, I(20), e("(fa-use 4)"));                 /* 4 + 4*4 */

    /* &optional: supplied and defaulted, through the fast path */
    e("(setf (symbol-function 'fa-opt) (%vm-compile '(lambda (a &optional (b 100)) (list a b))))");
    e("(setf (symbol-function 'fa-opt2) (%vm-compile '(lambda () (fa-opt 1 2))))");
    e("(setf (symbol-function 'fa-opt1) (%vm-compile '(lambda () (fa-opt 7))))");
    TEST_EQ(equal, e("'(1 2)"),   e("(fa-opt2)"));
    TEST_EQ(equal, e("'(7 100)"), e("(fa-opt1)"));

    /* &rest: the rest list is built from the stack args */
    e("(setf (symbol-function 'fa-rest) (%vm-compile '(lambda (a &rest r) (cons a r))))");
    e("(setf (symbol-function 'fa-rest0) (%vm-compile '(lambda () (fa-rest 1))))");
    e("(setf (symbol-function 'fa-rest3) (%vm-compile '(lambda () (fa-rest 1 2 3 4))))");
    TEST_EQ(equal, e("'(1)"),       e("(fa-rest0)"));
    TEST_EQ(equal, e("'(1 2 3 4)"), e("(fa-rest3)"));

    /* multiple values propagate through the fast path */
    e("(setf (symbol-function 'fa-mv) (%vm-compile '(lambda (a b) (floor a b))))");
    e("(setf (symbol-function 'fa-mvc) (%vm-compile '(lambda () (multiple-value-list (fa-mv 17 5)))))");
    TEST_EQ(equal, e("'(3 2)"), e("(fa-mvc)"));

    /* arg-count errors match the list path (too few / too many required) */
    e("(setf (symbol-function 'fa-two) (%vm-compile '(lambda (a b) (+ a b))))");
    TEST_EQ(eql, T_Symbol, e("(if (nth-value 1 (ignore-errors (funcall (%vm-compile '(lambda () (fa-two 1))) ))) t nil)"));
    TEST_EQ(eql, T_Symbol, e("(if (nth-value 1 (ignore-errors (funcall (%vm-compile '(lambda () (fa-two 1 2 3)))))) t nil)"));

    /* a compiled closure callee also takes the fast path */
    e("(setf (symbol-function 'fa-mkadder) (%vm-compile '(lambda (k) (lambda (x) (+ x k)))))");
    e("(setf (symbol-function 'fa-call-clo) (%vm-compile '(lambda (k v) (funcall (fa-mkadder k) v))))");
    TEST_EQ(eql, I(15), e("(fa-call-clo 10 5)"));

    /* An ERROR signalled by a compiled callee must unwind to a HANDLER-CASE in a compiled
       caller — crossing the caller's frame_vm_call backtrace marker.  This is the exact
       gap fast-apply first exposed: the SIGNAL->handler unwind must skip no-setjmp markers
       (frame_top_live) or it longjmps into garbage.  Value must arrive intact. */
    e("(setf (symbol-function 'fa-boom) (%vm-compile '(lambda (x) (if (< x 0) (error \"neg\") x))))");
    e("(setf (symbol-function 'fa-guard) (%vm-compile"
      "  '(lambda (x) (handler-case (+ 1 (fa-boom x)) (error () :caught)))))");
    TEST_EQ(eql, I(6),        e("(fa-guard 5)"));      /* no error: 1 + 5 */
    TEST_EQ(eql, e("':caught"), e("(fa-guard -3)"));   /* error crosses the marker, caught */

    close_live_pool(pool);
}

/* ---- inline primitives: +,-,*,<,cons,car,eq,null,... as byte-code ops --------- */
/* A CALL to an unshadowed standard primitive at its inline arity compiles to a dedicated
   stack op (no arg-list cons / no kernel_apply).  It must be SEMANTICALLY identical to the
   builtin (numeric tower, bignum promotion, type errors, car nil-safety, eq/eql), fall
   back to CALL_NAMED at other arities, and NOT fire for a lexically shadowed name. */
static void test_vm_inline_primitives(void){
    uword pool = open_live_pool();

    /* arithmetic + 1+/1- (compiled) */
    TEST_EQ(eql, I(12), e("(funcall (compile nil '(lambda (a b) (+ a b))) 8 4)"));
    TEST_EQ(eql, I(4),  e("(funcall (compile nil '(lambda (a b) (- a b))) 8 4)"));
    TEST_EQ(eql, I(32), e("(funcall (compile nil '(lambda (a b) (* a b))) 8 4)"));
    TEST_EQ(eql, I(9),  e("(funcall (compile nil '(lambda (a) (1+ a))) 8)"));
    TEST_EQ(eql, I(7),  e("(funcall (compile nil '(lambda (a) (1- a))) 8)"));
    /* the compiled body actually uses the inline op, not a call */
    TEST_EQ(eql, T_Symbol,   e("(let ((s (with-output-to-string (o)"
                              "  (%vm-disassemble (compile nil '(lambda (a b) (+ a b))) o))))"
                              "  (if (and (search \"add2\" s) (not (search \"call\" s))) t nil))"));

    /* full numeric tower preserved: fixnum overflow -> bignum, integer/integer -> ratio */
    TEST_EQ(eql, T_Symbol, e("(if (= (funcall (compile nil '(lambda (n) (* n n))) 1000000000000)"
                             "       (* 1000000000000 1000000000000)) t nil)"));
    TEST_EQ(equal, e("5/2"), e("(funcall (compile nil '(lambda (a b) (/ a b))) 5 2)"));  /* / not inlined */
    /* type error just like the builtin */
    TEST_EQ(eql, T_Symbol, e("(if (nth-value 1 (ignore-errors"
                             "  (funcall (compile nil '(lambda (a b) (+ a b))) 'x 1))) t nil)"));

    /* comparisons */
    TEST_EQ(equal, e("'(nil t nil t nil)"),
            e("(funcall (compile nil '(lambda (a b) (list (< a b) (> a b) (<= a b) (>= a b) (= a b)))) 8 4)"));
    TEST_EQ(equal, e("'(nil nil t t t)"),
            e("(funcall (compile nil '(lambda (a b) (list (< a b) (> a b) (<= a b) (>= a b) (= a b)))) 4 4)"));

    /* cons / car / cdr — incl. car nil-safety and a LIST type error */
    TEST_EQ(equal, e("'(1 . 2)"), e("(funcall (compile nil '(lambda (a b) (cons a b))) 1 2)"));
    TEST_EQ(eql, I(1), e("(funcall (compile nil '(lambda (l) (car l))) '(1 2))"));
    TEST_EQ(eql, I(2), e("(funcall (compile nil '(lambda (l) (cadr l))) '(1 2))"));
    TEST_EQ(eql, NIL_Symbol, e("(funcall (compile nil '(lambda (l) (car l))) nil)"));   /* (car nil)=nil */
    TEST_EQ(eql, T_Symbol, e("(if (nth-value 1 (ignore-errors"
                             "  (funcall (compile nil '(lambda (x) (car x))) 5))) t nil)"));  /* (car 5) errors */

    /* eq / eql / null / not */
    TEST_EQ(equal, e("'(t t nil nil)"),
            e("(funcall (compile nil '(lambda (a b) (list (eq a b) (eql a b) (null a) (not b)))) 'x 'x)"));
    TEST_EQ(equal, e("'(nil t t t)"),   /* eq on equal bignums may differ; eql t; (null nil)=t; (not nil)=t */
            e("(funcall (compile nil '(lambda () (list (eq (* 1000000000000 1000000000000)"
              "                                           (* 1000000000000 1000000000000))"
              "                                       (eql (* 1000000000000 1000000000000)"
              "                                            (* 1000000000000 1000000000000))"
              "                                       (null nil) (not nil)))))"));

    /* inline op in TAIL position returns correctly (emitted then the trailing RETURN) */
    TEST_EQ(eql, I(30), e("(funcall (compile nil '(lambda (a b) (+ a b))) 10 20)"));

    /* FALLBACK: wrong arity keeps variadic semantics via CALL_NAMED */
    TEST_EQ(eql, I(6),  e("(funcall (compile nil '(lambda (a b c) (+ a b c))) 1 2 3)"));
    TEST_EQ(eql, I(0),  e("(funcall (compile nil '(lambda () (+))))"));
    TEST_EQ(eql, T_Symbol, e("(funcall (compile nil '(lambda (a b c) (< a b c))) 1 2 3)"));

    /* FALLBACK: a lexically shadowed name is NOT inlined (calls the local FLET function) */
    TEST_EQ(eql, I(100), e("(funcall (compile nil '(lambda (a b) (flet ((+ (x y) (* x y))) (+ a b)))) 10 10)"));

    close_live_pool(pool);
}

/* ---- vm-7: CLOS dispatch effective-method cache ------------------------------- */
/* Generic dispatch now caches the effective method per generic function keyed on the
   argument-class tuple (a repeat call with the same classes skips applicable-methods +
   specificity sort + method-combination).  Must be transparent: same results, and the
   cache invalidates on add-method and class redefinition. */
static void test_vm_clos_cache(void){
    uword pool = open_live_pool();
    e("(defclass v7-animal () ())");
    e("(defclass v7-dog (v7-animal) ())");
    e("(defclass v7-cat (v7-animal) ())");
    e("(defgeneric v7-speak (a))");
    e("(defmethod v7-speak ((a v7-animal)) (list :animal))");
    e("(defmethod v7-speak ((a v7-dog)) (cons :woof (call-next-method)))");
    e("(defmethod v7-speak :around ((a v7-dog)) (cons :around (call-next-method)))");
    e("(defvar *v7-d* (make-instance 'v7-dog))");
    e("(defvar *v7-c* (make-instance 'v7-cat))");

    /* method combination: around + primary + call-next-method */
    TEST_EQ(equal, e("'(:around :woof :animal)"), e("(v7-speak *v7-d*)"));
    TEST_EQ(equal, e("'(:animal)"),               e("(v7-speak *v7-c*)"));
    /* repeat calls (cache hits) give the same results */
    TEST_EQ(equal, e("'(:around :woof :animal)"), e("(v7-speak *v7-d*)"));
    TEST_EQ(equal, e("'(:animal)"),               e("(v7-speak *v7-c*)"));

    /* add a method -> cache invalidated -> new behavior */
    e("(defmethod v7-speak ((a v7-cat)) (list :meow))");
    TEST_EQ(equal, e("'(:meow)"), e("(v7-speak *v7-c*)"));

    /* class redefinition -> cache invalidated -> dispatch still correct */
    e("(defclass v7-dog (v7-animal) ((tricks :initform nil)))");
    TEST_EQ(equal, e("'(:around :woof :animal)"), e("(v7-speak *v7-d*)"));

    /* built-in / T specializers dispatch (and cache) correctly */
    e("(defgeneric v7-kind (x))");
    e("(defmethod v7-kind ((x integer)) :int)");
    e("(defmethod v7-kind ((x string)) :str)");
    e("(defmethod v7-kind ((x t)) :other)");
    TEST_EQ(eql, e("':int"),   e("(v7-kind 5)"));
    TEST_EQ(eql, e("':str"),   e("(v7-kind \"hi\")"));
    TEST_EQ(eql, e("':other"), e("(v7-kind 'sym)"));
    TEST_EQ(eql, e("':int"),   e("(v7-kind 42)"));   /* cache hit, same class */

    /* the cache reduces work: full dispatch runs %applicable-methods per call; the
       cache runs it only on a miss (once per distinct class tuple) */
    e("(defvar *v7-ac* 0)");
    e("(let ((orig (symbol-function '%applicable-methods)))"
      "  (setf (symbol-function '%applicable-methods)"
      "        (lambda (gf args) (setq *v7-ac* (+ *v7-ac* 1)) (funcall orig gf args))))");
    e("(setq *v7-ac* 0) (dotimes (i 20) (v7-speak *v7-d*) (v7-speak *v7-c*))");
    TEST_EQ(eql, T_Symbol,
            e("(if (< *v7-ac* 5) t nil)"));   /* ~2 misses (dog, cat); NOT 40 (one per call) */

    close_live_pool(pool);
}

/* ---- vm-9: byte-code frames in the backtrace --------------------------------- */
/* A byte-code activation pushes a frame_vm_call marker on g_frame_stack, so a compiled
   call appears in (BACKTRACE) as a :CALL frame with its name + argument bindings, just
   like a tree-walked one.  The marker carries no setjmp; the unwind cascade skips it,
   so THROW / errors through compiled frames still work (also covered by test_vm_nlx). */
static void test_vm_backtrace(void){
    uword pool = open_live_pool();

    /* two compiled functions; the inner one captures the backtrace.  OUTER binds the
       result in a LET so the call to INNER is NOT in tail position — otherwise the
       trampoline (correctly) reuses OUTER's frame for INNER and OUTER would not appear
       (that TCO frame-merge is a feature, exercised by test_vm_tail_calls). */
    e("(bocl:define-function 'v9bt-inner nil (%vm-compile '(lambda (a b) (bocl:backtrace))))");
    e("(bocl:define-function 'v9bt-outer nil (%vm-compile '(lambda (x) (let ((r (v9bt-inner x (* x 2)))) r))))");
    TEST_EQ(eql, T_Symbol, e("(if (compiled-function-p (symbol-function 'v9bt-inner)) t nil)"));

    /* both compiled frames show up as named :CALL frames */
    e("(defvar *v9bt* (v9bt-outer 5))");
    e("(defvar *v9bt-calls* (remove-if-not (lambda (f) (eq (bocl:frame-kind f) :call)) *v9bt*))");
    TEST_EQ(eql, T_Symbol,
            e("(if (find 'v9bt-inner *v9bt-calls* :key #'bocl:frame-name) t nil)"));
    TEST_EQ(eql, T_Symbol,
            e("(if (find 'v9bt-outer *v9bt-calls* :key #'bocl:frame-name) t nil)"));
    /* and the argument bindings are recovered: inner A=5, outer X=5 */
    TEST_EQ(eql, I(5),
            e("(cdr (assoc 'a (bocl:frame-bindings"
              "  (find 'v9bt-inner *v9bt-calls* :key #'bocl:frame-name))))"));
    TEST_EQ(eql, I(5),
            e("(cdr (assoc 'x (bocl:frame-bindings"
              "  (find 'v9bt-outer *v9bt-calls* :key #'bocl:frame-name))))"));

    /* unwind-safety: a THROW crossing a compiled frame still delivers its value (the
       marker skip must not corrupt the cascade) */
    e("(bocl:define-function 'v9bt-thrower nil (%vm-compile '(lambda () (throw 'v9tag 42))))");
    e("(bocl:define-function 'v9bt-mid nil (%vm-compile '(lambda () (v9bt-thrower))))");
    TEST_EQ(eql, I(42), e("(catch 'v9tag (v9bt-mid))"));

    close_live_pool(pool);
}

/* ---- vm-9: byte-code DISASSEMBLE --------------------------------------------- */
/* %VM-DISASSEMBLE decodes a CompiledFunction's octet stream into a mnemonic listing
   with constants + jump targets resolved.  The decoder consumes the code octet-for-octet
   (a self-check against operand-table drift): a clean listing has no "unknown opcode"
   and no "WARNING" (over-read) line, and includes the terminal RETURN. */
static void test_vm_disassemble(void){
    uword pool = open_live_pool();

    /* branch + inline arithmetic (gt2/mul2) + a genuine named call (list) all decode */
    e("(defvar *v9d1* (with-output-to-string (o)"
      "  (%vm-disassemble (compile nil '(lambda (x) (if (> x 0) (list (* x x)) 0))) o)))");
    TEST_EQ(eql, T_Symbol, e("(if (search \"return\" *v9d1*) t nil)"));
    TEST_EQ(eql, T_Symbol, e("(if (search \"call-named\" *v9d1*) t nil)"));   /* (list ...) */
    TEST_EQ(eql, T_Symbol, e("(if (search \"gt2\" *v9d1*) t nil)"));          /* (> x 0) inlined */
    TEST_EQ(eql, T_Symbol, e("(if (search \"mul2\" *v9d1*) t nil)"));         /* (* x x) inlined */
    TEST_EQ(eql, NIL_Symbol, e("(if (search \"unknown\" *v9d1*) t nil)"));
    TEST_EQ(eql, NIL_Symbol, e("(if (search \"WARNING\" *v9d1*) t nil)"));

    /* a loop (jumps) + locals: still decodes clean and octet-exact */
    e("(defvar *v9d2* (with-output-to-string (o)"
      "  (%vm-disassemble (compile nil '(lambda (n) (let ((s 0)) (dotimes (i n s) (setq s (+ s i)))))) o)))");
    TEST_EQ(eql, T_Symbol,   e("(if (search \"jump\" *v9d2*) t nil)"));
    TEST_EQ(eql, NIL_Symbol, e("(if (search \"unknown\" *v9d2*) t nil)"));
    TEST_EQ(eql, NIL_Symbol, e("(if (search \"WARNING\" *v9d2*) t nil)"));

    /* CL:DISASSEMBLE returns NIL and prints the byte-code for a compiled function */
    TEST_EQ(eql, NIL_Symbol, e("(disassemble (compile nil '(lambda (x) (+ x 1))))"));

    close_live_pool(pool);
}

/* ---- vm-9: self-hosting — a real-code file compiles to byte-code and runs ------ */
/* COMPILE-FILE byte-compiles each DEFUN body into the fasl (self-hosting: the byte-code
   compiler, itself running, compiles the file); LOAD installs the templates with no
   re-compilation.  This corpus spans the feature surface real code (and the prelude)
   uses — recursion, closures/HOFs, iteration + multiple values, and conditions — proving
   the pipeline handles breadth, not just a couple of arithmetic functions.  Anything the
   VM can't compile falls back to interpreted (correct, just not byte-code). */
static void test_vm_selfhost(void){
    uword pool = open_live_pool();

    e("(with-open-file (f \"{TMP}/vm9_corpus.lisp\" :direction :output :if-exists :supersede)"
      "  (write-line \"(in-package :common-lisp)\" f)"
      "  (write-line \"(defun v9-fact (n) (if (< n 2) 1 (* n (v9-fact (- n 1)))))\" f)"
      "  (write-line \"(defun v9-adder (k) (lambda (x) (+ x k)))\" f)"                 /* closure */
      "  (write-line \"(defun v9-sumsq (n) (let ((s 0)) (dotimes (i n s) (setq s (+ s (* i i))))))\" f)"
      "  (write-line \"(defun v9-both (a b) (floor a b))\" f)"                          /* multiple values */
      "  (write-line \"(defun v9-safe (x) (handler-case (if (zerop x) (error \\\"zero\\\") (/ 10 x)) (error () :zero)))\" f)"
      "  (write-line \"(defun v9-map (l) (mapcar (v9-adder 100) l))\" f))");
    Ref fasl = e("(compile-file \"{TMP}/vm9_corpus.lisp\")");
    add_to_live_pool(fasl);
    TEST_EQ(eql, T_Symbol,
            e("(if (%cfasl-file-p (namestring (compile-file-pathname \"{TMP}/vm9_corpus.lisp\"))) t nil)"));
    e("(load (compile-file-pathname \"{TMP}/vm9_corpus.lisp\"))");

    /* the DEFUN bodies loaded as byte-code (self-hosted) ... */
    TEST_EQ(eql, T_Symbol, e("(if (and (compiled-function-p (symbol-function 'v9-fact))"
                             "          (compiled-function-p (symbol-function 'v9-adder))"
                             "          (compiled-function-p (symbol-function 'v9-sumsq))"
                             "          (compiled-function-p (symbol-function 'v9-both))"
                             "          (compiled-function-p (symbol-function 'v9-safe))"
                             "          (compiled-function-p (symbol-function 'v9-map))) t nil)"));
    /* ... and every feature runs correctly */
    TEST_EQ(eql, I(3628800), e("(v9-fact 10)"));
    TEST_EQ(eql, I(15),      e("(funcall (v9-adder 5) 10)"));
    TEST_EQ(eql, I(285),     e("(v9-sumsq 10)"));               /* 0+1+4+...+81 */
    TEST_EQ(equal, e("'(3 2)"), e("(multiple-value-list (v9-both 17 5))"));
    TEST_EQ(eql, I(5),       e("(v9-safe 2)"));
    TEST_EQ(eql, e("':zero"), e("(v9-safe 0)"));                /* handler-case caught */
    TEST_EQ(equal, e("'(101 102 103)"), e("(v9-map '(1 2 3))"));

    e("(ignore-errors (delete-file \"{TMP}/vm9_corpus.lisp\"))");
    e("(ignore-errors (delete-file (compile-file-pathname \"{TMP}/vm9_corpus.lisp\")))");
    close_live_pool(pool);
}

TEST_INITIALIZE(test_misc);
TEST_SUITE(test_misc, misc_init, misc_fini)
    TEST_CASE("cfasl primitives",    test_cfasl_primitives)
    TEST_CASE("cfasl writer",        test_cfasl_writer)
    TEST_CASE("cfasl roundtrip",     test_cfasl_roundtrip)
    TEST_CASE("cfasl load file",     test_cfasl_load_file)
    TEST_CASE("cfasl validate",      test_cfasl_validate)
    TEST_CASE("cfasl corpus",        test_cfasl_corpus)
    TEST_CASE("vm compiled-function", test_vm_compiled_function)
    TEST_CASE("vm execute",          test_vm_execute)
    TEST_CASE("vm analyze",          test_vm_analyze)
    TEST_CASE("vm codegen",          test_vm_codegen)
    TEST_CASE("vm control",          test_vm_control)
    TEST_CASE("vm values",           test_vm_values)
    TEST_CASE("vm nlx",              test_vm_nlx)
    TEST_CASE("eql spec + read-seq", test_eql_specializers_and_read_sequence)
    TEST_CASE("flet scoping",        test_flet_scoping)
    TEST_CASE("vm closures",         test_vm_closures)
    TEST_CASE("vm fasl",             test_vm_fasl)
    TEST_CASE("vm conditions",       test_vm_conditions)
    TEST_CASE("vm restarts",         test_vm_restarts)
    TEST_CASE("vm escaping blocks",  test_vm_escaping_blocks)
    TEST_CASE("vm exited extent",    test_vm_exited_extent)
    TEST_CASE("vm compile wiring",   test_vm_compile_wiring)
    TEST_CASE("vm compiled prelude", test_vm_compiled_prelude)
    TEST_CASE("vm tail calls",       test_vm_tail_calls)
    TEST_CASE("vm fast-apply",       test_vm_fast_apply)
    TEST_CASE("vm inline primitives", test_vm_inline_primitives)
    TEST_CASE("vm clos cache",       test_vm_clos_cache)
    TEST_CASE("vm backtrace",        test_vm_backtrace)
    TEST_CASE("vm disassemble",      test_vm_disassemble)
    TEST_CASE("vm selfhost",         test_vm_selfhost)
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
