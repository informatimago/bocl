all:build

PROGRAMS = print_types bocl

common_SRCS = \
	bocl.c string_list.c \
	kernel.c kernel_objects.c memory.c boerror.c \
	built_in_functions.c arithmetic.c packages.c hash_table.c conditions.c sequences.c \
	eval.c environment.c \
	variable.c cons.c data.c \
	structure.c kernel_structure_functions.c structure_functions.c \
	stream.c callback_stream.c reader.c printer.c filename.c \
	socket.c \
	character.c bostring.c bit_vector.c vector.c \
	integer.c float.c \
	\
	profile.c \
	cfasl.c \
	bytecode.c \
	strdup.c \
	\
	$(DEBUG_SRCS)

all_SRCS = main.c $(common_SRCS)

libbocl_SRCS 	 = $(common_SRCS)
libbocl_LIBS 	 = -lm -lc
libbocl_INCS 	 =

bocl_SRCS        = main.c $(common_SRCS)
bocl_LIBS        = -L. -lbocl -lm -lc
bocl_INCS        = -I.

print_types_SRCS = print_types.c
print_types_LIBS =
print_types_INCS =

INCLUDES     += $(libbocl_INCS)
LIBRARIES    += $(libbocl_LIBS)

.PHONY = clean prepare-macos deps build predefines run sign-gdb gdb \
	run-print-limits run-print-types prelude-image

include common.mk
deps:   $(all_SRCS:%.c=%.d)
-include $(all_SRCS:%.c=%.d)

build: deps libbocl.a libbocl.$(SO) $(PROGRAMS)

# Deliverables (exe + shared + static), for install/dist.
DELIVERABLES = bocl libbocl.a $(SHLIB)

clean:
	@ printf "# Removing object files and target\n"
	-$(VERBOSE) rm -rf libbocl.* $(PROGRAMS) *.o *.d *.e *.dSYM bocl.pc
	-$(VERBOSE) rm -rf $(DISTSTAGE) check-stage driver_static driver_shared bocl-*.tar.* bocl-*.sha256
	-$(VERBOSE)	rm -rf *.tem */*.tem *.[ld]x{32,64}fsl */*.[ld]x{32,64}fsl
	@ printf "# Removing byte-compiled Lisp images (*.cfasl)\n"
	-$(VERBOSE) rm -f $(BOOTSTRAP_CFASL) ../modules/*/*.cfasl

prepare-macos:
	- sudo xcode-select --install
	  sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
	  xcode-select --print-path


predefines:
	$(VERBOSE) printf '#define _POSIX_SOURCE\n#include <unistd.h>\n' > __predefines.c
	$(VERBOSE) cpp -dM  __predefines.c | sort
	-$(VERBOSE) rm -f   __predefines.c

gdb: bocl
	$(VERBOSE) $(RUN) gdb ./bocl

# on macOS, gdb must be codesigned.
sign-gdb:
	@ printf "# Signing gdb with sudo\n"
	$(VERBOSE) true See 'https://gist.github.com/gravitylow/fb595186ce6068537a6e9da6d8b5b96d'
	$(VERBOSE) echo '<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd"><plist version="1.0"><dict><key>com.apple.security.cs.debugger</key><true/></dict></plist>' >  gdb-entitlement.xml
	$(VERBOSE) sudo codesign --entitlements gdb-entitlement.xml -fs gdb-cert "$$(which gdb)"

run: bocl
	$(VERBOSE) $(RUN) ./bocl $(ARGUMENTS)

# ----------------------------------------
print_types: $(print_types_SRCS:%.c=%.d) $(print_types_SRCS:%.c=%.o)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(print_types_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(print_types_SRCS:%.c=%.o)

# Static archive.
libbocl.a: $(libbocl_SRCS:%.c=%.d) $(libbocl_SRCS:%.c=%.o)
	@ printf "# Archiving %-20s\n" "$@"
	-$(VERBOSE) rm -f $@
	$(VERBOSE) $(AR) $(ARFLAGS_BOCL) $@ $(libbocl_SRCS:%.c=%.o)
	-$(VERBOSE) $(RANLIB) $@ >/dev/null 2>&1 || true

# Versioned shared library (carries the soname / install_name).
$(SHLIB): $(libbocl_SRCS:%.c=%.d) $(libbocl_SRCS:%.c=%.o)
	@ printf "# Linking %-20s\n" "$@"
	$(VERBOSE) $(LINKLIB) $(SONAME_FLAGS) -o $@ $(libbocl_SRCS:%.c=%.o) $(LDLIBS)

# Unversioned dev symlink (libbocl.$(SO)) + the major-soname runtime symlink.
libbocl.$(SO): $(SHLIB)
	$(VERBOSE) $(LN_S) $(SHLIB) $(SHLIB_MAJOR)
	$(VERBOSE) $(LN_S) $(SHLIB) $(SHLIB_LINK)

# for now, we link all the bocl objects into bocl:
bocl: $(bocl_SRCS:%.c=%.d) $(bocl_SRCS:%.c=%.o)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(bocl_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(bocl_SRCS:%.c=%.o) $(LDLIBS)

# ----------------------------------------------------------------------
# vm-10 / cfasl — byte-compiled boot images.  COMPILE-FILE each system Lisp
# file to a .cfasl sibling so the runtime boots pre-byte-compiled with no
# reader and no macroexpansion.  A stage-0 bocl (booted from source via
# --no-compiled-prelude) does the compilation, with *PACKAGE* = COMMON-LISP —
# the package these files are loaded in at boot, so their bare kernel symbols
# (%EXPORT-STRINGS, %BIGNUM-*, ...) resolve to the same symbols LOAD sees.
#
# The runtime auto-detects each .cfasl and prefers it over its .lisp source ONLY
# when the .cfasl is at least as new (date-ordering; a stale cfasl is ignored)
# and version-compatible (bocl_fasl_load returns NIL on a cfasl/byte-code
# mismatch -> source fallback, never a half image).  --no-compiled-prelude forces
# the prelude/cl-exports source.
#
# Dependency policy: a .cfasl depends on its .lisp source (rebuilt when the
# source changes) but only ORDER-ONLY on bocl (the `| bocl` prerequisite) — a
# plain relink after a C edit must NOT trigger a multi-minute recompile of every
# image, and a byte-code/cfasl version bump is handled at runtime by the source
# fallback (regenerate with `make -B fasls` when you want fresh images).
# ----------------------------------------------------------------------
BOOTSTRAP_LISP  = ../bootstrap/prelude.lisp     ../bootstrap/cl-exports.lisp        \
                  ../bootstrap/bignum.lisp      ../bootstrap/bignum-integration.lisp \
                  ../bootstrap/socket.lisp      ../bootstrap/vm-compiler.lisp
BOOTSTRAP_CFASL = $(BOOTSTRAP_LISP:.lisp=.cfasl)

# Byte-compile one system .lisp -> .cfasl with the boot package (COMMON-LISP).
# A file that cannot be externalised (one that embeds a live object PRIN1 cannot
# round-trip — e.g. a function via #.(function ...) in quoted data) writes no
# cfasl and simply loads from source: COMPILE-FILE detects it and make tolerates
# the missing target.  (All current system files DO compile.)
../bootstrap/%.cfasl: ../bootstrap/%.lisp | bocl
	@ printf "# Byte-compiling %s (prelude takes a few minutes)\n" "$(@F)"
	$(VERBOSE) ./bocl --no-compiled-prelude --norc -b \
	    -e '(let ((*package* (find-package "COMMON-LISP"))) (compile-file "$<" :output-file "$@"))' \
	    --quit

# All system images + the module images (see the `modules` root target).
fasls: $(BOOTSTRAP_CFASL) module-fasls
prelude-image: $(BOOTSTRAP_CFASL)          # kept as an alias for the vm-10 name

# Byte-compile every installable module's *.lisp -> *.cfasl (best effort: a
# module whose sources are absent, or that fails to compile, is simply left to
# load from source).  Uses a stage-0 bocl already carrying its images.
module-fasls: | bocl
	@ printf "# Byte-compiling module images\n"
	-$(VERBOSE) for mf in $(wildcard ../modules/*/module.manifest) ; do \
	    md=`dirname $$mf` ; \
	    for lf in $$md/*.lisp ; do \
	        [ -f "$$lf" ] || continue ; \
	        cf=`printf '%s' "$$lf" | sed 's/\.lisp$$/.cfasl/'` ; \
	        [ "$$cf" -nt "$$lf" ] && continue ; \
	        printf "#   %s\n" "$$cf" ; \
	        ./bocl --norc -b -e "(compile-file \"$$lf\" :output-file \"$$cf\")" --quit || true ; \
	    done ; \
	done

.PHONY: fasls prelude-image module-fasls

#bocl: $(bocl_SRCS:%.c=%.d) $(bocl_SRCS:%.c=%.o) libbocl.$(SO)
#	@ printf "# Linking %-20s   from %s\n" "$@" "$(bocl_SRCS:%.c=%.o)"
#	$(VERBOSE) $(LINKBIN) -o $@ $(bocl_SRCS:%.c=%.o)  $(bocl_LIBS)


# ======================================================================
# Install / package the deliverables into $(DESTDIR)$(PREFIX).
# ======================================================================

# Public headers: install the whole header set under include/bocl so a driver
# can #include <bocl/bocl.h> (internal "..." includes resolve in the same dir).
INSTALL_HEADERS = $(wildcard *.h)

# The "system" Lisp files the installed executable loads at run time: the prelude
# and its siblings (cl-exports.lisp loaded by bocl_load_prelude(), bignum*.lisp
# and socket.lisp loaded by the prelude itself).  They install together into
# $(bocllibexecdir) so the runtime sibling-resolution keeps working.  Add new
# system files (e.g. ASDF) here as they appear — a prelude hook that LOADs a
# sibling will abort the whole boot if that sibling was not installed.
SYSTEM_LISP = \
	../bootstrap/prelude.lisp \
	../bootstrap/cl-exports.lisp \
	../bootstrap/bignum.lisp \
	../bootstrap/bignum-integration.lisp \
	../bootstrap/socket.lisp \
	../bootstrap/vm-compiler.lisp

# Manual page (committed roff) and Texinfo source/output for the info manual.
MAN_PAGE  = ../man/bocl.1
INFO_TEXI = ../documentation/bocl.texi
INFO_FILE = ../documentation/bocl.info

# Documentation installed under $(docdir): the .org sources are always present;
# the .pdf renders are installed only when they have been built (doc targets).
DOC_ORG = ../README.org ../documentation/documentation.org \
	../specifications/specifications.org ../specifications/debugger-specifications.org
DOC_PDF = $(wildcard ../documentation/documentation.pdf \
	../specifications/specifications.pdf ../specifications/debugger-specifications.pdf)

# Installable "system" modules: each ../modules/<name>/ that carries a
# module.manifest is install-ready (its loadable *.lisp + the manifest).  Modules
# that need building (e.g. ASDF, a submodule) stage their built file + manifest into
# such a directory via the root `modules` target before install.
MODULE_MANIFESTS = $(wildcard ../modules/*/module.manifest)

.PHONY: install install-strip uninstall dist pkgconfig info

# (Re)build the Texinfo info manual.  Needs makeinfo (texinfo); best-effort.
info: $(INFO_FILE)
$(INFO_FILE): $(INFO_TEXI)
	@ printf "# Generating %-20s from %s\n" "$(@F)" "$(<F)"
	-$(VERBOSE) ( cd ../documentation && makeinfo bocl.texi )

bocl.pc:
	@ printf "# Generating %-20s\n" "$@"
	$(VERBOSE) { \
	  printf 'prefix=%s\n'              '$(PREFIX)';        \
	  printf 'exec_prefix=$${prefix}\n';                    \
	  printf 'libdir=%s\n'              '$(libdir)';        \
	  printf 'includedir=%s\n'          '$(includedir)';    \
	  printf '\n';                                          \
	  printf 'Name: bocl\n';                                \
	  printf 'Description: BOCL Common Lisp runtime library\n'; \
	  printf 'Version: %s\n'            '$(VERSION)';        \
	  printf 'Libs: -L$${libdir} -lbocl -lm\n';             \
	  printf 'Cflags: -I$${includedir}\n';                  \
	} > $@

install: build fasls bocl.pc
	@ printf "# Installing bocl %s into %s\n" "$(VERSION)" "$(DESTDIR)$(PREFIX)"
	$(VERBOSE) $(INSTALL) -d $(DESTDIR)$(bindir) $(DESTDIR)$(libdir) \
	    $(DESTDIR)$(boclincdir) $(DESTDIR)$(bocllibexecdir) $(DESTDIR)$(pkgconfdir) \
	    $(DESTDIR)$(man1dir) $(DESTDIR)$(infodir) $(DESTDIR)$(docdir)
	@ printf "#   bin/bocl\n"
	$(VERBOSE) $(INSTALL) -m 755 bocl $(DESTDIR)$(bindir)/bocl
	@ printf "#   lib/libbocl.{a,$(SO)} + soname links\n"
	$(VERBOSE) $(INSTALL) -m 644 libbocl.a $(DESTDIR)$(libdir)/libbocl.a
	-$(VERBOSE) $(RANLIB) $(DESTDIR)$(libdir)/libbocl.a >/dev/null 2>&1 || true
	$(VERBOSE) $(INSTALL) -m 755 $(SHLIB) $(DESTDIR)$(libdir)/$(SHLIB)
	$(VERBOSE) cd $(DESTDIR)$(libdir) && $(LN_S) $(SHLIB) $(SHLIB_MAJOR) && $(LN_S) $(SHLIB) $(SHLIB_LINK)
	@ printf "#   include/bocl/*.h\n"
	$(VERBOSE) for h in $(INSTALL_HEADERS) ; do \
	    $(INSTALL) -m 644 $$h $(DESTDIR)$(boclincdir)/$$h ; done
	@ printf "#   libexec/bocl/ system Lisp files (+ .cfasl images)\n"
	$(VERBOSE) for f in $(SYSTEM_LISP) ; do \
	    $(INSTALL) -m 644 $$f $(DESTDIR)$(bocllibexecdir)/ ; \
	    cf=`printf '%s' "$$f" | sed 's/\.lisp$$/.cfasl/'` ; \
	    [ -f "$$cf" ] && $(INSTALL) -m 644 $$cf $(DESTDIR)$(bocllibexecdir)/ || true ; \
	done
	@ printf "#   libexec/bocl/modules/ (installed modules + .cfasl images)\n"
	$(VERBOSE) for mf in $(MODULE_MANIFESTS) ; do \
	    md=`dirname $$mf` ; name=`basename $$md` ; \
	    $(INSTALL) -d $(DESTDIR)$(bocllibexecdir)/modules/$$name ; \
	    $(INSTALL) -m 644 $$mf $(DESTDIR)$(bocllibexecdir)/modules/$$name/module.manifest ; \
	    for lf in $$md/*.lisp $$md/*.cfasl ; do \
	        [ -f "$$lf" ] && $(INSTALL) -m 644 $$lf $(DESTDIR)$(bocllibexecdir)/modules/$$name/ || true ; \
	    done ; \
	done
	@ printf "#   (touch every installed .cfasl so it is newer than its source:\n"
	@ printf "#    a type-less LOAD then prefers the byte-compiled image)\n"
	-$(VERBOSE) find $(DESTDIR)$(bocllibexecdir) -name '*.cfasl' -exec touch {} + 2>/dev/null || true
	@ printf "#   share/man/man1/bocl.1\n"
	$(VERBOSE) $(INSTALL) -m 644 $(MAN_PAGE) $(DESTDIR)$(man1dir)/bocl.1
	@ printf "#   share/info/bocl.info\n"
	-$(VERBOSE) $(MAKE) $(MFLAGS) $(INFO_FILE) >/dev/null 2>&1 || true
	-$(VERBOSE) test -f $(INFO_FILE) && $(INSTALL) -m 644 $(INFO_FILE) $(DESTDIR)$(infodir)/bocl.info || \
	    printf "#   (skipped bocl.info: makeinfo unavailable)\n"
	@ printf "#   share/doc/bocl/ (org + pdf)\n"
	$(VERBOSE) for d in $(DOC_ORG) $(DOC_PDF) ; do \
	    $(INSTALL) -m 644 $$d $(DESTDIR)$(docdir)/ ; done
	@ printf "#   lib/pkgconfig/bocl.pc\n"
	$(VERBOSE) $(INSTALL) -m 644 bocl.pc $(DESTDIR)$(pkgconfdir)/bocl.pc

# Same as install but strip the executable and shared library.
install-strip: install
	-$(VERBOSE) $(STRIP) $(DESTDIR)$(bindir)/bocl
	-$(VERBOSE) $(STRIP) -x $(DESTDIR)$(libdir)/$(SHLIB)

uninstall:
	@ printf "# Uninstalling bocl from %s\n" "$(DESTDIR)$(PREFIX)"
	-$(VERBOSE) rm -f $(DESTDIR)$(bindir)/bocl
	-$(VERBOSE) rm -f $(DESTDIR)$(libdir)/libbocl.a
	-$(VERBOSE) rm -f $(DESTDIR)$(libdir)/$(SHLIB) $(DESTDIR)$(libdir)/$(SHLIB_MAJOR) $(DESTDIR)$(libdir)/$(SHLIB_LINK)
	-$(VERBOSE) rm -f $(DESTDIR)$(pkgconfdir)/bocl.pc
	-$(VERBOSE) rm -f $(DESTDIR)$(man1dir)/bocl.1
	-$(VERBOSE) rm -f $(DESTDIR)$(infodir)/bocl.info
	-$(VERBOSE) rm -rf $(DESTDIR)$(boclincdir)
	-$(VERBOSE) rm -rf $(DESTDIR)$(bocllibexecdir)
	-$(VERBOSE) rm -rf $(DESTDIR)$(docdir)
	-$(VERBOSE) rm -rf $(DESTDIR)$(bocldatadir)

# ----------------------------------------------------------------------
# Binary distribution tarball:  bocl-<ver>-<os>-<arch>-<libc>-<cc>.<ext>
# Relocatable layout (bin/ lib/ include/ share/ at the tarball root).
# COMPRESS = gzip (default, universal) | zstd | xz
# ----------------------------------------------------------------------
DIST_OS   := $(shell uname -s | tr '[:upper:]' '[:lower:]')
DIST_ARCH := $(shell uname -m)
DIST_LIBC := $(shell if ldd --version 2>&1 | grep -qi musl ; then echo musl ; \
                     elif [ "$(UNAME)" = Darwin ] ; then echo darwin ; \
                     else echo glibc ; fi)
DIST_CC   := $(notdir $(CC))
DIST_NAME := bocl-$(VERSION)-$(DIST_OS)-$(DIST_ARCH)-$(DIST_LIBC)-$(DIST_CC)
DISTSTAGE := dist-stage
COMPRESS  ?= gzip

ifeq ($(COMPRESS),zstd)
DIST_EXT  = tar.zst
DIST_PIPE = zstd -19 -q -c >
else ifeq ($(COMPRESS),xz)
DIST_EXT  = tar.xz
DIST_PIPE = xz -9 -c >
else
DIST_EXT  = tar.gz
DIST_PIPE = gzip -9 -c >
endif

dist: build bocl.pc
	@ printf "# Packaging %s.%s\n" "$(DIST_NAME)" "$(DIST_EXT)"
	-$(VERBOSE) rm -rf $(DISTSTAGE)
	$(VERBOSE) $(MAKE) $(MFLAGS) install-strip DESTDIR=$(CURDIR)/$(DISTSTAGE) PREFIX=/$(DIST_NAME)
	$(VERBOSE) tar -C $(CURDIR)/$(DISTSTAGE)/$(DIST_NAME) -cf - . | $(DIST_PIPE) $(DIST_NAME).$(DIST_EXT)
	$(VERBOSE) ( cd $(CURDIR) && { shasum -a 256 $(DIST_NAME).$(DIST_EXT) 2>/dev/null \
	             || sha256sum $(DIST_NAME).$(DIST_EXT) ; } > $(DIST_NAME).$(DIST_EXT).sha256 )
	-$(VERBOSE) rm -rf $(DISTSTAGE)
	@ printf "# Produced %s.%s (+ .sha256)\n" "$(DIST_NAME)" "$(DIST_EXT)"

# ----------------------------------------------------------------------
# Linkage proof: stage an install, then compile ../tests/driver.c against
# the installed headers and link it BOTH ways (static archive + shared lib),
# running each. This is the per-environment portability gate in CI.
# ----------------------------------------------------------------------
CHECKSTAGE = check-stage
.PHONY: check-lib
check-lib: build bocl.pc
	@ printf "# Linkage proof: building independent driver against installed libs\n"
	-$(VERBOSE) rm -rf $(CHECKSTAGE)
	$(VERBOSE) $(MAKE) $(MFLAGS) install DESTDIR=$(CURDIR)/$(CHECKSTAGE) PREFIX=/usr
	@ printf "# -> static link (libbocl.a)\n"
	$(VERBOSE) $(CC) -std=c17 -I$(CURDIR)/$(CHECKSTAGE)/usr/include ../tests/driver.c \
	    $(CURDIR)/$(CHECKSTAGE)/usr/lib/libbocl.a -lm -o driver_static
	$(VERBOSE) ./driver_static
	@ printf "# -> shared link (-lbocl)\n"
	$(VERBOSE) $(CC) -std=c17 -I$(CURDIR)/$(CHECKSTAGE)/usr/include ../tests/driver.c \
	    -L$(CURDIR)/$(CHECKSTAGE)/usr/lib -lbocl -lm -o driver_shared
	$(VERBOSE) LD_LIBRARY_PATH=$(CURDIR)/$(CHECKSTAGE)/usr/lib \
	           DYLD_LIBRARY_PATH=$(CURDIR)/$(CHECKSTAGE)/usr/lib ./driver_shared
	-$(VERBOSE) rm -rf $(CHECKSTAGE) driver_static driver_shared
	@ printf "# Linkage proof OK\n"

# Host CL used to (re)generate kernel_objects.* / built_in_functions.* /
# structure_functions.* etc. The generated outputs are committed, so the host
# CL is only needed for `make regenerate`, not for an ordinary build.
#
# Auto-detect ccl, sbcl, or clisp on PATH. Override with `make LISP=<impl>`.
LISP ?= $(firstword $(shell command -v ccl 2>/dev/null) \
                    $(shell command -v sbcl 2>/dev/null) \
                    $(shell command -v clisp 2>/dev/null))

LISP_BASENAME = $(notdir $(LISP))

ifeq ($(LISP_BASENAME),ccl)
LISP_NORC = --no-init
LISP_LOAD = --load
LISP_EVAL = --eval
LISP_QUIT = --eval '(ccl:quit 0)'
else ifeq ($(LISP_BASENAME),sbcl)
LISP_NORC = --no-userinit --no-sysinit
LISP_LOAD = --load
LISP_EVAL = --eval
LISP_QUIT = --quit
else ifeq ($(LISP_BASENAME),clisp)
LISP_NORC = -norc
LISP_LOAD = -i
LISP_EVAL = -x
LISP_QUIT = -x '(ext:quit 0)'
else
LISP      = $(error No host Common Lisp found on PATH; install ccl, sbcl, or clisp, or set LISP=<impl> on the make command line)
LISP_NORC =
LISP_LOAD =
LISP_EVAL =
LISP_QUIT =
endif

# The committed generated C sources. The generator .lisp files are ORDER-ONLY
# prerequisites of the rules below, so an ordinary `make build` rebuilds them
# only when one is MISSING — never on a checkout-mtime skew (git/tar set
# arbitrary mtimes, which previously made `make build` try to regenerate and
# demand a host CL that is not present in build-only environments like CI).
# `make regenerate` forces a real regeneration by deleting them first.
GENERATED_SOURCES = \
	kernel_objects.h kernel_objects.c \
	kernel_functions.h built_in_functions.h built_in_functions.c \
	kernel_structure_functions.h kernel_structure_functions.c \
	structure_functions.h structure_functions.c

.PHONY: regenerate
regenerate:
	@printf "# Forcing regeneration of committed generated C sources via host CL\n"
	$(VERBOSE) rm -f $(GENERATED_SOURCES)
	$(VERBOSE) $(MAKE) $(GENERATED_SOURCES)

kernel_objects.h kernel_objects.c : | generate-kernel-objects.lisp
	@printf "# Generating %s from %s\n" "$@" "$|"
	$(VERBOSE) $(LISP) $(LISP_NORC) \
		$(LISP_LOAD) generate-kernel-objects.lisp \
		$(LISP_EVAL) '(com.informatimago.bocl.kernel.generate:generate-c-code)' \
		$(LISP_QUIT)

kernel_functions.h built_in_functions.h built_in_functions.c \
lisp/kernel-package.lisp : | generate-built-in-functions.lisp
	@printf "# Generating %s from %s\n" "$@" "$|"
	$(VERBOSE) $(LISP) $(LISP_NORC) \
		$(LISP_LOAD) "$$HOME/quicklisp/setup.lisp" \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/common-lisp/cesarum/"   asdf:*central-registry*)' \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/common-lisp/lisp-sexp/" asdf:*central-registry*)' \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/common-lisp/"           asdf:*central-registry*)' \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/"                       asdf:*central-registry*)' \
		$(LISP_EVAL) '(ql:quickload :com.informatimago.common-lisp.cesarum)' \
		$(LISP_LOAD) generate-built-in-functions.lisp \
		$(LISP_EVAL) '(com.informatimago.bocl.kernel.generate.kernel-functions:generate-built-in-functions)' \
		$(LISP_QUIT)


kernel_structure_functions.h kernel_structure_functions.c \
structure_functions.h structure_functions.c \
lisp/kernel-package+structure.lisp : | generate-built-in-functions.lisp generate-structures.lisp
	@printf "# Generating %s from %s\n" "$@" "$|"
	$(VERBOSE) $(LISP) $(LISP_NORC) \
		$(LISP_LOAD) "$$HOME/quicklisp/setup.lisp" \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/common-lisp/cesarum/"   asdf:*central-registry*)' \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/common-lisp/lisp-sexp/" asdf:*central-registry*)' \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/common-lisp/"           asdf:*central-registry*)' \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/"                       asdf:*central-registry*)' \
		$(LISP_EVAL) '(ql:quickload :com.informatimago.common-lisp.cesarum)' \
		$(LISP_LOAD) generate-built-in-functions.lisp \
		$(LISP_LOAD) generate-structures.lisp \
		$(LISP_EVAL) '(com.informatimago.bocl.kernel.generate.kernel-functions:generate-structure-functions)' \
		$(LISP_QUIT)


run-print-limits: # bocl
	$(VERBOSE) $(LISP) $(LISP_NORC) \
		$(LISP_LOAD) lisp/print-limits.lisp \
		$(LISP_EVAL) '(print-limits)' \
		$(LISP_QUIT)
#	 $(VERBOSE) $(RUN) ./bocl $(LISP_NORC) \
#	 	$(LISP_LOAD) lisp/print-limits.lisp \
#	 	$(LISP_EVAL) '(print-limits)' \
#	 	$(LISP_QUIT)

run-print-types:print_types
	$(VERBOSE) $(RUN) ./print_types


environment.e: environment.c
	gcc -E   -o environment.e  environment.c && cat  environment.e

#### THE END ####
