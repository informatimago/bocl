all:bocl

VERBOSE=@
#VERBOSE=

CC_SANITIZE_FLAGS = -fsanitize=address -fsanitize=null                              \
            -fsanitize=bounds -fsanitize=vla-bound                                  \
            -fsanitize=unreachable -fsanitize=return -fsanitize=shift               \
            -fsanitize=shift-exponent -fsanitize=shift-base                         \
            -fsanitize=integer-divide-by-zero -fsanitize=signed-integer-overflow    \
            -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow          \
            -fsanitize=nonnull-attribute -fsanitize=returns-nonnull-attribute       \
            -fsanitize=bool -fsanitize=enum -fsanitize-address-use-after-scope      \
            -fsanitize-undefined-trap-on-error -fstack-protector-all                \
            -fstack-check

LD_SANITIZE_FLAGS = -fsanitize=leak
DEBUG_FLAGS       = -g -g3 -ggdb -O0
NODEBUG_FLAGS     = -fsanitize=object-size # incompatible with -O0

CFLAGS    = -x c -std=c11  $(CC_SANITIZE_FLAGS) $(DEBUG_FLAGS)
LDFLAGS   = $(LD_SANITIZE_FLAGS) $(DEBUG_FLAGS)

LIBRARIES = \
	-L/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/12.0.5/lib/darwin \
	-lclang_rt.asan_osx_dynamic

%.o:%.c
	@ printf "# Compiling %s from %s\n" "$@" "$<"
	$(VERBOSE) $(CC) $(CFLAGS) -c -o $@ $<

bocl:main.o kernel.o bocl.o string_list.o kernel_objects.o memory.o
	@ printf "# Linking %s from %s\n" "$@" "$^"
	$(VERBOSE) $(CC) $(LDLAGS) $(LIBRARIES) -o $@ $^

clean:
	@ printf "# Removing object files and target\n"
	-$(VERBOSE) rm -f bocl *.o *.[ld]x{32,64}fsl

gdb:bocl
	gdb ./bocl


# on macOS, gdb must be codesigned.
sign-gdb:
	@ printf "# Signing gdb with sudo\n"
	$(VERBOSE) true See 'https://gist.github.com/gravitylow/fb595186ce6068537a6e9da6d8b5b96d'
	$(VERBOSE) echo '<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd"><plist version="1.0"><dict><key>com.apple.security.cs.debugger</key><true/></dict></plist>' >  gdb-entitlement.xml
	$(VERBOSE) sudo codesign --entitlements gdb-entitlement.xml -fs gdb-cert "$$(which gdb)"

LISP=ccl
LISP_NORC=--no-init
LISP_LOAD=--load
LISP_EVAL=--eval
LISP_QUIT=--eval '(ccl:quit 0)'

kernel_objects.h kernel_objects.c : generate-kernel-objects.lisp
	@printf "# Generating %s from %s\n" "$@" "$^"
	$(VERBOSE) $(LISP) $(LISP_NORC) \
		$(LISP_LOAD) generate-kernel-objects.lisp \
		$(LISP_EVAL) '(com.informatimago.bocl.kernel.generate:generate-c-code)' \
		$(LISP_QUIT)

kernel.o:kernel.c kernel.h kernel_objects.h


check:bocl
    ./bocl
