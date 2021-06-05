all:build

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

all_SRCS = $(common_SRCS) \
	main.c \
	test_reader.c

libbocl_SRCS = $(untested_SRCS) $(tested_SRCS)
libbocl_LIBS = -lm
libbocl_INCS =

tested_SRCS = \
	reader.c

untested_SRCS = \
	bocl.c string_list.c \
	kernel.c kernel_objects.c memory.c error.c \
	built_in_functions.c \
	variable.c cons.c data.c \
	stream.c  printer.c filename.c \
	character.c string.c bit_vector.c vector.c \
	\
	strdup.c \
	\
	$(DEBUG_SRCS)

bocl_SRCS        = main.c
bocl_LIBS        = -L. -lbocl
bocl_INCS        = -I.

test_reader_SRCS = test_reader.c # $(untested_SRCS)
test_reader_LIBS = -L. -lbocl
test_reader_INCS = -I.


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

# LD_SANITIZE_FLAGS = -fsanitize=leak
LD_SANITIZE_FLAGS =
NODEBUG_FLAGS     = -fsanitize=object-size # incompatible with -O0

# -Werror
WARN_FLAGS        = -Wall -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align \
					-Wwrite-strings -Wconversion  -Wstrict-prototypes -Wmissing-prototypes \
					-Wmissing-declarations -Wredundant-decls -Wnested-externs -Winline \
					\
					-Wno-unused-parameter

DEBUG_FLAGS       = -g -g3 -ggdb -O0


UNAME=$(shell uname)

ifeq ($(UNAME)$(SANITIZE),Darwin)

SDKPATH      = /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
XCODE_DEV    = $(shell xcode-select --print-path)
ASAN_LIB_DIR = $(XCODE_DEV)/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/12.0.5/lib/darwin

CFLAGS       = $(WARN_FLAGS) $(CC_SANITIZE_FLAGS) $(DEBUG_FLAGS) $(DEFINES) -x c -std=c11
LDFLAGS      = $(LD_SANITIZE_FLAGS)
RUN_ENV      = DYLD_LIBRARY_PATH=$(ASAN_LIB_DIR)
INCLUDES     = $(libbocl_INCS)
LIBRARIES    = $(libbocl_LIBS) \
	-L$(SDKPATH)/usr/lib \
	-L$(ASAN_LIB_DIR) \
	-lclang_rt.asan_osx_dynamic \
	-lgmalloc
# LIBRARIES=$(shell xcode-select --print-path)/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/12.0.5/lib/darwin/libclang_rt.asan_osx_dynamic.dylib \
# 	/usr/lib/libgmalloc.dylib
SO           = dylib
SHARED       = -dylib

else

CFLAGS    = $(WARN_FLAGS) $(DEBUG_FLAGS) $(DEFINES) -x c -std=c11
LDFLAGS   =
RUN_ENV   =
INCLUDES  = $(libbocl_INCS)
LIBRARIES = $(libbocl_LIBS)
SO        = so
SHARED    = -shared

endif

# ----------------------------------------

DEPFLAGS   = -E -MT $@ -MM -MP -MF $*.d
MAKEDEPEND = $(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDES)  $(TARGET_ARCH) $(DEPFLAGS)
COMPILE    = $(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDES)  $(TARGET_ARCH) -fPIC -c
LINKLIB    = $(LD) $(LDFLAGS) $(LIBRARIES) $(TARGET_ARCH) $(SHARED)
LINKBIN    = $(LD) $(LDFLAGS) $(LIBRARIES) $(TARGET_ARCH)

%.d : %.c
	@ printf "# Generating dependencies         for %s\n" "$<"
	$(VERBOSE) $(MAKEDEPEND) $<

%.o : %.c %.d
	@ printf "# Compiling %-20s from %s\n" "$@" "$<"
	$(VERBOSE) $(COMPILE) -o $@ $<

# ----------------------------------------

.PHONY = clean prepare-macos deps build predefines run sign-gdb gdb

clean:
	@ printf "# Removing object files and target\n"
	-$(VERBOSE) rm -rf bocl *.o *.d *.[ld]x{32,64}fsl *.dSYM

prepare-macos:
	- sudo xcode-select --install
	  sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
	  xcode-select --print-path

deps:   $(all_SRCS:%.c=%.d)
include $(all__SRCS:%.c=%.d)

build: deps libbocl.$(SO) bocl test_reader

predefines:
	$(VERBOSE) printf '#define _POSIX_SOURCE\n#include <unistd.h>\n' > test.c
	$(VERBOSE) cpp -dM  test.c | sort

gdb: bocl
	$(VERBOSE) $(RUN_ENV) gdb ./bocl

# on macOS, gdb must be codesigned.
sign-gdb:
	@ printf "# Signing gdb with sudo\n"
	$(VERBOSE) true See 'https://gist.github.com/gravitylow/fb595186ce6068537a6e9da6d8b5b96d'
	$(VERBOSE) echo '<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd"><plist version="1.0"><dict><key>com.apple.security.cs.debugger</key><true/></dict></plist>' >  gdb-entitlement.xml
	$(VERBOSE) sudo codesign --entitlements gdb-entitlement.xml -fs gdb-cert "$$(which gdb)"

run: bocl
	$(VERBOSE) $(RUN_ENV) ./bocl $(ARGUMENTS)

# ----------------------------------------

libbocl.$(SO): $(libbocl_SRCS:%.c=%.d) $(libbocl_SRCS:%.c=%.o)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(bocl_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKLIB) -o $@ $(libbocl_SRCS:%.c=%.o)

# for now, we link all the bocl objects into bocl:
bocl: $(bocl_SRCS:%.c=%.d) $(bocl_SRCS:%.c=%.o) $(libbocl_SRCS:%.c=%.o)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(bocl_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(bocl_SRCS:%.c=%.o)  $(libbocl_SRCS:%.c=%.o)

#bocl: $(bocl_SRCS:%.c=%.d) $(bocl_SRCS:%.c=%.o) libbocl.$(SO)
#	@ printf "# Linking %-20s   from %s\n" "$@" "$(bocl_SRCS:%.c=%.o)"
#	$(VERBOSE) $(LINKBIN) -o $@ $(bocl_SRCS:%.c=%.o)  $(bocl_LIBS)

test_reader: $(test_reader_SRCS:%.c=%.d) $(test_reader_SRCS:%.c=%.o)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_reader_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_reader_SRCS:%.c=%.o) $(test_reader_LIBS)


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

kernel_functions.h built_in_functions.h built_in_functions.c : generate-built-in-functions.lisp
	@printf "# Generating %s from %s\n" "$@" "$^"
	$(VERBOSE) $(LISP) $(LISP_NORC) \
		$(LISP_LOAD) generate-built-in-functions.lisp \
		$(LISP_EVAL) '(com.informatimago.bocl.kernel.generate.kernel-functions:generate-built-in-functions)' \
		$(LISP_QUIT)


#### THE END ####
