all:deps bocl


VERBOSE=@
#VERBOSE=
SANITIZE=

ifdef DEBUG
DEFINES    = -DDEBUG=yes
DEBUG_SRCS = debug.c
else
DEFINES    =
DEBUG_SRCS =
endif

SRCS = main.c bocl.c string_list.c \
	kernel.c kernel_objects.c memory.c \
	variable.c cons.c data.c \
	stream.c reader.c printer.c filename.c \
	\
	strdup.c \
	\
	$(DEBUG_SRCS)



UNAME=$(shell uname)

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
NODEBUG_FLAGS     = -fsanitize=object-size # incompatible with -O0

WARN_FLAGS        = -Wall -Werror -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align \
					-Wwrite-strings -Wconversion  -Wstrict-prototypes -Wmissing-prototypes \
					-Wmissing-declarations -Wredundant-decls -Wnested-externs -Winline \
					\
					-Wno-unused-parameter

DEBUG_FLAGS       = -g -g3 -ggdb -O0

ifeq ($(UNAME)$(SANITIZE),Darwin)
CFLAGS    = $(WARN_FLAGS) $(CC_SANITIZE_FLAGS) $(DEBUG_FLAGS) $(DEFINES) -x c -std=c11
LDFLAGS   = $(LD_SANITIZE_FLAGS) $(DEBUG_FLAGS)
ASAN_LIB_DIR = /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/12.0.5/lib/darwin
RUN_ENV = DYLD_LIBRARY_PATH=$(ASAN_LIB_DIR)
LIBRARIES = \
	-L$(ASAN_LIB_DIR) \
	-lclang_rt.asan_osx_dynamic \
	-lgmalloc

# LIBRARIES=/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/12.0.5/lib/darwin/libclang_rt.asan_osx_dynamic.dylib \
# 	/usr/lib/libgmalloc.dylib

else
CFLAGS    = $(WARN_FLAGS) $(DEBUG_FLAGS) $(DEFINES) -x c -std=c11
LDFLAGS   = $(DEBUG_FLAGS)
RUN_ENV   =
LIBRARIES =
endif


DEPFLAGS = -E -MT $@ -MM -MP -MF $*.d
COMPILE.c    = $(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@
MAKEDEPEND.c = $(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) $(DEPFLAGS)

%.o : %.c %.d
	@ printf "# Compiling %-20s from %s\n" "$@" "$<"
	$(VERBOSE) $(COMPILE.c) $<

%.o :
%.d : %.c
	@ printf "# Generating dependencies         for %s\n" "$<"
	$(VERBOSE) $(MAKEDEPEND.c) $<

deps:   $(SRCS:%.c=%.d)
include $(SRCS:%.c=%.d)




bocl: $(SRCS:%.c=%.d) $(SRCS:%.c=%.o)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(SRCS:%.c=%.o)"
	$(VERBOSE) $(CC) $(LDLAGS) $(LIBRARIES) -o $@ $(SRCS:%.c=%.o)


clean:
	@ printf "# Removing object files and target\n"
	-$(VERBOSE) rm -rf bocl *.o *.d *.[ld]x{32,64}fsl *.dSYM

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


test:bocl
	@printf "# Testing\n"
	$(VERBOSE) output=$$($(RUN_ENV)  ./bocl --quiet 2>&1) || ( status=$$? ;\
	expected=$$'\n'"ERROR: in bocl_read_eval_cstring:  NOT IMPLEMENTED YET" ;\
	[ "$$output" = "$$expected" ] && [ $$status -eq 65 ] || ( printf "output = x%sx\n" "$$output" ; printf "expected = x%sx\n" "$$expected" ; exit 1 ; ) ; )
	@printf "# Success\n"

run:bocl
	$(VERBOSE) $(RUN_ENV) ./bocl

predefines:
	$(VERBOSE) printf '#define _POSIX_SOURCE\n#include <unistd.h>\n' > test.c
	$(VERBOSE) cpp -dM  test.c | sort
