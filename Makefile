# project Makefile (used by gitlab CI).

SUBDIRS   = specifications design sources tests documentation modules
DOCTARGET = pdfs

help:  ## Show this message (list available targets and their purpose).
	@awk 'BEGIN { FS = ":.*?## "; printf "Usage: make <target>\n\nTargets:\n" } \
	  /^[a-zA-Z_][a-zA-Z0-9_.-]*:.*?## / { printf "  %-24s %s\n", $$1, $$2 }' \
	  $(MAKEFILE_LIST)

all:       		 documents build test  ## Build the documents, the core, and run the tests.
documents: 		 specifications design documentation
.PHONY:    		 help documents bocl clean clean-backups regenerate                  \
		 test tests specifications design documentation modules          \
		 install install-programs install-libraries install-documentation \
		 install-strip uninstall info dist check-lib check-versions      \
		 build-programs build-libraries build-documentation              \
		 stage stage-programs stage-libraries stage-documentation clean-stage

specifications:  ; @cd specifications && $(MAKE) $(MFLAGS) $(DOCTARGET)
design:          ; @cd design         && $(MAKE) $(MFLAGS) $(DOCTARGET)
# Build the C/Lisp core, then the system modules (asdf etc.; a no-op until their
# source submodules are present — see modules/README.org).
build:           ; @for dir in sources tests modules ; do  $(MAKE) -C $$dir $(MFLAGS) build ; done
modules:         ; @$(MAKE) -C modules $(MFLAGS)
test tests:      ; @cd tests          && $(MAKE) $(MFLAGS) test
documentation:   ; @cd documentation  && $(MAKE) $(MFLAGS) $(DOCTARGET)

# ======================================================================
# Build / stage / install / release, split by artefact kind.
# Follows https://gitlab.com/informatimago/rules/-/blob/master/build-rules.md
# (see AGENTS.md). In short:
#
#   stage-X   unprivileged: build, then lay the result out under
#             $(STAGE)/X/ exactly as it must appear on disk.
#   install-X privileged: copy that tree into $(DESTDIR)$(PREFIX)/.
#             It compiles nothing — which is what stops `sudo make
#             install' from leaving root-owned artefacts in the checkout
#             (build-rules.md B2). Under sudo it re-runs the staging half
#             as $SUDO_USER (B3).
#
# $(PREFIX) is COMPILED IN here — BOCL_PRELUDE_PATH in the executable,
# -install_name in the shared library, prefix= in bocl.pc — so a staged
# tree is bound to the PREFIX it was built for, and the stage holds the
# full prefix path ($(STAGE)/programs$(PREFIX)/bin/bocl). Changing PREFIX
# means re-staging. DESTDIR only moves where the tree is written.
# ======================================================================

PREFIX  ?= /opt/local
DESTDIR ?=
STAGE   ?= $(CURDIR)/stage
STAGE_PROGRAMS      := $(STAGE)/programs
STAGE_LIBRARIES     := $(STAGE)/libraries
STAGE_DOCUMENTATION := $(STAGE)/documentation
STAGE_VARS = STAGE=$(STAGE) PREFIX=$(PREFIX)

# Run a staging target unprivileged. -H so the build's caches land in that
# user's home, not root's. Bare root with no SUDO_USER is the packager /
# container case: nobody to drop to, a root-owned tree is harmless there.
define stage-as-user
if [ "$$(id -u)" != 0 ]; then \
  $(MAKE) $(1) $(STAGE_VARS) ; \
elif [ -n "$(SUDO_USER)" ] && [ "$(SUDO_USER)" != root ]; then \
  echo "==> staging as $(SUDO_USER) (dropping root for the build half)" ; \
  sudo -H -u "$(SUDO_USER)" $(MAKE) $(1) $(STAGE_VARS) ; \
else \
  echo "WARNING: running as root with no SUDO_USER; $(1) will build with root" ; \
  echo "         privileges and leave root-owned artefacts in the source tree." ; \
  $(MAKE) $(1) $(STAGE_VARS) ; \
fi
endef

# Copy a staged tree into $(DESTDIR)$(PREFIX). tar, not cp: symlinks (the
# libbocl soname links) and modes must survive and the copy must merge
# into an existing prefix; --no-same-owner so installed files belong to
# the installing user rather than to whoever staged them.
define copy-stage
dest="$(DESTDIR)$(PREFIX)" ; src="$(1)$(PREFIX)" ; \
if [ -z "$$dest" ]; then \
  echo "install: PREFIX and DESTDIR are both empty" >&2 ; exit 1 ; fi ; \
if [ ! -d "$$src" ]; then \
  echo "install: staging area $$src is missing (run the matching stage-* target)" >&2 ; \
  exit 1 ; fi ; \
$(INSTALL) -d "$$dest" ; \
tar -C "$$src" -cf - . | tar -C "$$dest" --no-same-owner -xf - ; \
echo "installed: $$src/ -> $$dest/"
endef

# Every staged tree carries a manifest naming the commit it was built
# from (build-rules.md B8); nothing else installed does.
MANIFEST_DIR = share/doc/bocl
define stage-manifest
$(INSTALL) -d "$(1)$(PREFIX)/$(MANIFEST_DIR)" ; \
sh scripts/make-manifest.sh $(2) > "$(1)$(PREFIX)/$(MANIFEST_DIR)/manifest-$(2).txt" ; \
echo "staged: $(1)$(PREFIX)/$(MANIFEST_DIR)/manifest-$(2).txt"
endef

INSTALL ?= install

# --- build phases -----------------------------------------------------

build-programs:  ## Build the executable and the runtime Lisp images it boots from.
	@$(MAKE) -C sources $(MFLAGS) build
	@$(MAKE) -C sources $(MFLAGS) fasls
	@$(MAKE) -C modules $(MFLAGS)

build-libraries:  ## Build libbocl.{a,$(SO)} and the pkg-config file.
	@$(MAKE) -C sources $(MFLAGS) build
	@$(MAKE) -C sources $(MFLAGS) bocl.pc

build-documentation:  ## Render the documents and the Info manual (needs the doc toolchain).
	@$(MAKE) $(MFLAGS) documents
	@$(MAKE) -C sources $(MFLAGS) info

# --- staging (unprivileged; this is the half that compiles) -----------

stage: stage-programs stage-libraries stage-documentation  ## Build + stage everything under $(STAGE)/.

stage-programs: build-programs  ## Stage bin/bocl + libexec/bocl/ under $(STAGE)/programs.
	@rm -rf "$(STAGE_PROGRAMS)"
	@$(MAKE) -C sources $(MFLAGS) install-programs PREFIX=$(PREFIX) DESTDIR=$(STAGE_PROGRAMS)
	@$(call stage-manifest,$(STAGE_PROGRAMS),programs)

stage-libraries: build-libraries  ## Stage lib/libbocl.* + include/bocl/ + pkgconfig under $(STAGE)/libraries.
	@rm -rf "$(STAGE_LIBRARIES)"
	@$(MAKE) -C sources $(MFLAGS) install-libraries PREFIX=$(PREFIX) DESTDIR=$(STAGE_LIBRARIES)
	@$(call stage-manifest,$(STAGE_LIBRARIES),libraries)

stage-documentation: build-documentation  ## Stage man/info/doc under $(STAGE)/documentation.
	@rm -rf "$(STAGE_DOCUMENTATION)"
	@$(MAKE) -C sources $(MFLAGS) install-documentation PREFIX=$(PREFIX) DESTDIR=$(STAGE_DOCUMENTATION)
	@$(call stage-manifest,$(STAGE_DOCUMENTATION),documentation)

# --- install (privileged; a pure copy) --------------------------------

install: install-programs install-libraries install-documentation  ## Install everything into $$PREFIX by copying the staged trees (stages first, as $$SUDO_USER under sudo; never builds as root).

install-programs:  ## Install only bin/bocl + its runtime Lisp files.
	@$(call stage-as-user,stage-programs)
	@$(call copy-stage,$(STAGE_PROGRAMS))

install-libraries:  ## Install only libbocl.{a,$(SO)}, the headers and bocl.pc.
	@$(call stage-as-user,stage-libraries)
	@$(call copy-stage,$(STAGE_LIBRARIES))

install-documentation:  ## Install only the man page, Info manual and documents.
	@$(call stage-as-user,stage-documentation)
	@$(call copy-stage,$(STAGE_DOCUMENTATION))
	@for f in "$(STAGE_DOCUMENTATION)$(PREFIX)"/share/info/*.info; do \
	  [ -f "$$f" ] || continue ; \
	  install-info --info-dir="$(DESTDIR)$(PREFIX)/share/info" \
	               "$(DESTDIR)$(PREFIX)/share/info/`basename $$f`" 2>/dev/null \
	    || echo "[skip] install-info dir update" ; \
	done

install-strip: install  ## Install, then strip the installed executable and shared library.
	@cd sources && $(MAKE) $(MFLAGS) strip-installed DESTDIR=$(DESTDIR) PREFIX=$(PREFIX)

clean-stage:  ## Remove the staging area ($(STAGE)).
	rm -rf "$(STAGE)"

# uninstall / info / dist / check-lib still live in sources/Makefile.
uninstall info dist check-lib:
	@cd sources && $(MAKE) $(MFLAGS) $@
clean:
	-@rm -f test.xml
	@for dir in $(SUBDIRS)     ; do $(MAKE) -C $$dir $(MFLAGS) clean ; done

# Regenerate the Lisp-generated C sources (kernel_objects.*, built_in_functions.*,
# kernel_structure_functions.*, structure_functions.*). Needs a host Common Lisp.
# Override the impl with `make LISP=sbcl regenerate` etc.
regenerate:
	@printf "# Regenerating generated C sources via host CL\n"
	$(MAKE) -C sources $(MFLAGS) regenerate

# Audit the release tags / version-* pointers against the shared version
# rules (see AGENTS.md). Run `git fetch --prune --tags` first.
check-versions:
	sh scripts/check-versions.sh

# Remove the *.~N~ Emacs backup files cluttering the tree. They are gitignored
# by the `*~` pattern in .gitignore, but having hundreds of them in the working
# directory makes navigation painful.
clean-backups:
	@printf "# Removing Emacs *.~N~ backup files\n"
	-find . -type f -name '*.~[0-9]*~' -print -delete
