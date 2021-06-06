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

CFLAGS       += $(WARN_FLAGS) $(CC_SANITIZE_FLAGS) $(DEBUG_FLAGS) $(DEFINES) -x c -std=c11
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

CFLAGS    += $(WARN_FLAGS) $(DEBUG_FLAGS) $(DEFINES) -x c -std=c17
LDFLAGS   +=
RUNLIBS   +=
INCLUDES  += 
LIBRARIES += 
SO        = so
SHARED    = -shared

endif

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
