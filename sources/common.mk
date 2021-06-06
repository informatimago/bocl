# ----------------------------------------

VERBOSE=@
#VERBOSE=
SANITIZE=

ifdef DEBUG
DEFINES    += -DDEBUG=yes
DEBUG_SRCS += debug.c
else
DEFINES    +=
DEBUG_SRCS +=
endif


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


WARN_GCC = \
	-Walloc-zero \
	-Warith-conversion \
	-Warray-parameter=2 \
	-Wduplicated-branches \
	-Wduplicated-cond \
	-Wformat-signedness \
	-Wformat-truncation \
	-Wfree-nonheap-object \
	-Wjump-misses-init \
	-Wlogical-op \
	-Wshift-overflow=2 \
	-Wstringop-overflow=4 \
	-Wswitch-unreachable \
	-Wtrampolines \
	-Wunsafe-loop-optimizations \
	-Wunused-but-set-variable

WARN_CLANG = \
	-Walloca \
	-Wdangling-else \
	-Wexpansion-to-defined \
	-Wframe-address \
	-Wunused-const-variable

# -Werror
WARN_FLAGS = \
	-Wall -Wpedantic -Wextra \
	-Waggregate-return \
	-Warray-bounds \
	-Wbad-function-cast \
	-Wcast-align \
	-Wcast-qual \
	-Wconversion  \
	-Wconversion \
	-Wdate-time \
	-Wformat-nonliteral \
	-Wformat-security \
	-Winline \
	-Wmissing-declarations \
	-Wmissing-include-dirs \
	-Wmissing-prototypes \
	-Wnested-externs \
	-Wnull-dereference \
	-Woverlength-strings \
	-Wpointer-arith \
	-Wredundant-decls \
	-Wshadow \
	-Wstack-protector \
	-Wstrict-prototypes \
	-Wswitch-default \
	-Wswitch-enum \
	-Wundef \
	-Wunused \
	-Wunused-local-typedefs \
	-Wunused-macros \
	-Wunused-parameter \
	-Wwrite-strings \


DEBUG_FLAGS       = -g -g3 -ggdb -O0


UNAME=$(shell uname)

ifeq ($(UNAME)$(SANITIZE),Darwin)

SDKPATH      = /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
XCODE_DEV    = $(shell xcode-select --print-path)
ASAN_LIB_DIR = $(XCODE_DEV)/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/12.0.5/lib/darwin

CFLAGS       += $(WARN_FLAGS) $(WARN_CLANG) $(CC_SANITIZE_FLAGS) $(DEBUG_FLAGS) $(DEFINES) -x c -std=iso9899:2018  -D_POSIX_C_SOURCE=200809L
LDFLAGS      += $(LD_SANITIZE_FLAGS)
RUNLIBS      += $(ASAN_LIB_DIR)
INCLUDES     +=
LIBRARIES    +=  \
	-L$(SDKPATH)/usr/lib \
	-L$(ASAN_LIB_DIR) \
	-lclang_rt.asan_osx_dynamic \
	-lgmalloc -lSystem
SO           = dylib
SHARED       = -dylib

else

CC        = gcc
CFLAGS    += $(WARN_FLAGS) $(WARN_GCC) $(DEBUG_FLAGS) $(DEFINES) -x c -std=c17 -D_POSIX_C_SOURCE=200809L
LDFLAGS   +=
RUNLIBS   +=
INCLUDES  +=
LIBRARIES +=
SO        = so
SHARED    = -shared

endif

# /bin/sh:
RUN       =  DYLD_LIBRARY_PATH=$$(echo "$(RUNLIBS)" |tr ' ' ':')

# ----------------------------------------

DEPFLAGS   = -E -MT $@ -MM -MP -MF $*.d
MAKEDEPEND = $(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDES) $(TARGET_ARCH) $(DEPFLAGS)
COMPILE    = $(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDES) $(TARGET_ARCH) -fPIC -c
LINKLIB    = $(LD) $(LDFLAGS) $(LIBRARIES) $(TARGET_ARCH) $(SHARED)
LINKBIN    = $(LD) $(LDFLAGS) $(LIBRARIES) $(TARGET_ARCH)

%.d : %.c
	@ printf "# Generating dependencies         for %s\n" "$<"
	$(VERBOSE) $(MAKEDEPEND) $<

%.o : %.c %.d
	@ printf "# Compiling %-20s from %s\n" "$@" "$<"
	$(VERBOSE) $(COMPILE) -o $@ $<

%.e : %.c %.d
	@ printf "# Preprocessing %-20s from %s\n" "$@" "$<"
	$(VERBOSE) $(COMPILE) -E -o $@ $<

# ----------------------------------------
