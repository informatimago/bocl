#ifndef bocl_backtrace_compat_h
#define bocl_backtrace_compat_h
/* backtrace() / backtrace_symbols_fd() live in <execinfo.h>, a glibc/macOS
   extension that musl (and some others) lack. Where present, use the real ones;
   elsewhere provide no-op stubs so the opt-in native-backtrace debug aids
   (BOCL_BACKTRACE, BOCL_POISON_BT, the GC use-after-free quarantine dumps) build
   everywhere and simply print nothing on libcs without execinfo. */
#if defined(__GLIBC__) || defined(__APPLE__)
#  include <execinfo.h>
#  define BOCL_HAVE_BACKTRACE 1
#else
#  define BOCL_HAVE_BACKTRACE 0
static inline int  backtrace(void** buffer, int size){
    (void)buffer; (void)size; return 0; }
static inline void backtrace_symbols_fd(void* const* buffer, int size, int fd){
    (void)buffer; (void)size; (void)fd; }
#endif

#endif
