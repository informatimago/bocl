# ----------------------------------------

VERBOSE=@
#VERBOSE=

# ---- Installation layout (GNU-style; overridable) ----------------------------
# Deliverables install under $(DESTDIR)$(PREFIX). DESTDIR is for staged installs
# (packaging); PREFIX is the logical install root baked into the binary.
PREFIX      ?= /usr/local
DESTDIR     ?=
exec_prefix ?= $(PREFIX)
bindir      ?= $(exec_prefix)/bin
libdir      ?= $(exec_prefix)/lib
includedir  ?= $(PREFIX)/include
datarootdir ?= $(PREFIX)/share
datadir     ?= $(datarootdir)
pkgconfdir  ?= $(libdir)/pkgconfig
# bocl-private subdirs:
boclincdir   = $(includedir)/bocl
bocldatadir  = $(datadir)/bocl

# ---- Toolchain (all overridable for cross/alt compilers) ---------------------
AR          ?= ar
ARFLAGS_BOCL = rcs
RANLIB      ?= ranlib
INSTALL     ?= install
STRIP       ?= strip
LN_S        ?= ln -sf

# ---- Version (single source of truth: version.h) -----------------------------
# Resolve version.h relative to THIS file (common.mk), so the extraction works
# whether common.mk is included from sources/ or from tests/.
COMMON_MK_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
VERSION_H     := $(COMMON_MK_DIR)version.h
VERSION_MAJOR := $(shell grep -E 'define[[:space:]]+BOCL_VERSION_MAJOR' $(VERSION_H) | grep -oE '[0-9]+')
VERSION_MINOR := $(shell grep -E 'define[[:space:]]+BOCL_VERSION_MINOR' $(VERSION_H) | grep -oE '[0-9]+')
VERSION_PATCH := $(shell grep -E 'define[[:space:]]+BOCL_VERSION_PATCH' $(VERSION_H) | grep -oE '[0-9]+')
VERSION       := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

# ---- Build profile: debug (default) | release --------------------------------
BUILD ?= debug

# SANITIZE controls AddressSanitizer + the wider -fsanitize=* set below.
# Default: off, so the produced binary has no dependency on libclang_rt.asan_*.
# Enable with: make SANITIZE=yes
SANITIZE ?= no

ifdef DEBUG
DEFINES    += -DDEBUG=yes
DEBUG_SRCS += debug.c
else
DEFINES    +=
DEBUG_SRCS +=
endif


ifeq ($(SANITIZE),yes)
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
NODEBUG_FLAGS     = -fsanitize=object-size # incompatible with -O0
else
CC_SANITIZE_FLAGS =
NODEBUG_FLAGS     =
endif

# LD_SANITIZE_FLAGS = -fsanitize=leak
LD_SANITIZE_FLAGS =

WARN_NONE= \
	-Warith-conversion \
	-Warray-parameter=2

WARN_GCC = \
	-Walloc-zero \
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
	-Wunused-const-variable \
	-Wno-dollar-in-identifier-extension

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


ifeq ($(BUILD),release)
DEBUG_FLAGS       = -O2 -DNDEBUG
else
DEBUG_FLAGS       = -g -g3 -ggdb -O0
endif

# Compiled-in default location of the installed prelude, so an installed binary
# finds it even outside the exe-relative / hard-coded search paths. $BOCL_PRELUDE
# still overrides at runtime (see find_prelude() in main.c).
DEFINES          += -DBOCL_PRELUDE_PATH='"$(bocldatadir)/prelude.lisp"'

UNAME=$(shell uname)

ifeq ($(UNAME),Darwin)

SDKPATH      = /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk

CFLAGS       += $(WARN_FLAGS) $(WARN_CLANG) $(CC_SANITIZE_FLAGS) $(DEBUG_FLAGS) $(DEFINES) -x c -std=iso9899:2018  -D_POSIX_C_SOURCE=200809L
LDFLAGS      += $(LD_SANITIZE_FLAGS)
INCLUDES     +=
LIBRARIES    += -L$(SDKPATH)/usr/lib -lSystem
SO           = dylib
SHARED       = -dylib
# Versioned shared library naming + soname for Mach-O.
SHLIB        = libbocl.$(VERSION).dylib
SHLIB_MAJOR  = libbocl.$(VERSION_MAJOR).dylib
SHLIB_LINK   = libbocl.dylib
SONAME_FLAGS = -install_name $(libdir)/$(SHLIB_MAJOR) \
               -compatibility_version $(VERSION_MAJOR).0.0 -current_version $(VERSION)

ifeq ($(SANITIZE),yes)
XCODE_DEV    = $(shell xcode-select --print-path)
# The clang version dir varies between Xcode releases; pick the first match.
ASAN_LIB_DIR = $(firstword $(wildcard $(XCODE_DEV)/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/*/lib/darwin))
RUNLIBS      += $(ASAN_LIB_DIR)
LIBRARIES    += -L$(ASAN_LIB_DIR) -lclang_rt.asan_osx_dynamic -lgmalloc
# Embed an rpath so the sanitised binary can find libclang_rt.asan_*.dylib without
# DYLD_LIBRARY_PATH set externally. Only needed in the sanitised build.
LDFLAGS      += -rpath $(ASAN_LIB_DIR)
endif

else

# Default to gcc on non-Darwin, but let env/command-line CC win (e.g. CC=clang).
ifeq ($(origin CC),default)
CC        = gcc
endif
# tcc (TinyCC) is a supported C17 compiler but lacks gcc-style dependency
# generation (-MT/-MM/-MP) and -Wl,-soname; detect it to special-case those.
ifneq (,$(findstring tcc,$(notdir $(CC))))
CC_IS_TCC := 1
endif
# Pick the gcc- or clang-specific warning set from the actual compiler.
CC_VERSION := $(shell $(CC) --version 2>/dev/null)
ifneq (,$(findstring clang,$(CC_VERSION)))
WARN_FAMILY = $(WARN_CLANG)
else
WARN_FAMILY = $(WARN_GCC)
endif
CFLAGS    += $(WARN_FLAGS) $(WARN_FAMILY) $(CC_SANITIZE_FLAGS) $(DEBUG_FLAGS) $(DEFINES) -x c -std=c17 -D_POSIX_C_SOURCE=200809L
LDFLAGS   += $(LD_SANITIZE_FLAGS)
INCLUDES  +=
LIBRARIES +=
SO        = so
SHARED    = -shared
# Versioned shared library naming + soname for ELF.
SHLIB        = libbocl.so.$(VERSION)
SHLIB_MAJOR  = libbocl.so.$(VERSION_MAJOR)
SHLIB_LINK   = libbocl.so
SONAME_FLAGS = -Wl,-soname,$(SHLIB_MAJOR)
ifdef CC_IS_TCC
SONAME_FLAGS =                      # tcc does not support -Wl,-soname
endif

ifeq ($(SANITIZE),yes)
# Link through the sanitizer driver flags so gcc pulls in the asan/ubsan
# runtimes correctly. Linking -lasan/-lubsan by hand ahead of the objects fails
# under GNU ld --as-needed with undefined __asan_*/__ubsan_* references (and
# print_types, linked without them at all, failed outright).
LDFLAGS += -fsanitize=address -fsanitize=undefined
endif

endif

# Libraries that MUST come AFTER the object files on the link line. bocl calls
# pow/sqrt/sin/... — on glibc/musl these live in libm (separate from libc), so
# the executable needs -lm. It is placed after the objects because modern ld
# defaults to --as-needed and would drop a -lm that preceded the objects that
# reference it. (On Darwin libSystem already provides the math symbols; -lm is
# an accepted no-op alias there, so keeping it unconditional is harmless.)
LDLIBS    += -lm

# /bin/sh:
RUN       =  ldpath=$$(echo "$(RUNLIBS)" |tr ' ' ':') ; LD_LIBRARY_PATH=$${ldpath} DYLD_LIBRARY_PATH=$${ldpath}

# ----------------------------------------

DEPFLAGS   = -E -MT $@ -MM -MP -MF $*.d
MAKEDEPEND = $(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDES) $(TARGET_ARCH) $(DEPFLAGS)
ifdef CC_IS_TCC
# tcc lacks -MT/-MM/-MP; emit empty dep files (CI builds clean, so header-change
# tracking via .d is not needed for a tcc validation build).
MAKEDEPEND = printf '' >$@ ;:
endif
COMPILE    = $(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDES) $(TARGET_ARCH) -fPIC -c
ifeq ($(UNAME),Darwin)
LINKLIB    = $(LD) $(LDFLAGS) $(LIBRARIES) $(TARGET_ARCH) $(SHARED)
LINKBIN    = $(LD) $(LDFLAGS) $(LIBRARIES) $(TARGET_ARCH)
else
LINKLIB    = $(CC) $(LDFLAGS) $(LIBRARIES) $(TARGET_ARCH) $(SHARED)
LINKBIN    = $(CC) $(LDFLAGS) $(LIBRARIES) $(TARGET_ARCH)
endif

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
