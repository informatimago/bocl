# -*- mode:makefile-gmake; coding:utf-8 -*-
# Build "system" modules into install-ready directories (each with a
# module.manifest).  The top-level `build` target runs `make -C modules`.
# Modules with no build step (plain .lisp + committed manifest) need nothing here.

# Host Common Lisp used to build ASDF's single-file release (ASDF's own Makefile).
LISP ?= $(firstword $(shell command -v ccl 2>/dev/null) \
                    $(shell command -v sbcl 2>/dev/null) \
                    $(shell command -v clisp 2>/dev/null))

.PHONY: all build modules asdf clean

all build: modules
modules: asdf factorial

# ---- ASDF -----------------------------------------------------------------
# Source submodule in asdf-src/ (added on authorization; see README.org).  Build
# its single-file release and stage it as the install-ready module asdf/.
#
# The bocl UIOP port lives as a patch (asdf-bocl-port.patch) applied to the
# checked-out submodule.  We do NOT guess whether it is already applied: the old
# `git apply --reverse --check` skip was fragile, because `make clean` only does
# `rm -rf asdf` and never restores asdf-src.  An interrupted or partial build
# could leave the submodule half-patched (some target files patched, some not) —
# a state that BOTH the reverse-check (skip) and a forward apply reject, breaking
# every subsequent build until asdf-src is reset by hand (seen on the macOS
# runner).  Instead we make patching idempotent and self-healing: restore exactly
# the files the patch touches to pristine submodule HEAD, then apply forward.
# This always starts from a clean base regardless of prior state, so it never
# lands in (or trips over) a half-patched tree.
asdf:
	@ if [ -f asdf-src/Makefile ] ; then \
	    printf "# Applying bocl UIOP port (asdf-bocl-port.patch) onto pristine sources\n" ; \
	    bocl_patch_files=`git -C asdf-src apply --numstat ../asdf-bocl-port.patch | awk '{print $$3}'` && \
	    git -C asdf-src checkout -- $$bocl_patch_files && \
	    git -C asdf-src apply ../asdf-bocl-port.patch && printf "#   applied\n" ; \
	    printf "# Building ASDF (single-file release; raw cat of sources)\n" ; \
	    $(MAKE) -C asdf-src build/asdf.lisp && \
	    mkdir -p asdf && \
	    cp asdf-src/build/asdf.lisp asdf/asdf.lisp && \
	    printf '(:name "asdf"\n :files ("asdf"))\n' > asdf/module.manifest && \
	    printf "# Staged module asdf/ (asdf.lisp + module.manifest)\n" ; \
	  else \
	    printf "# ASDF submodule not present (modules/asdf-src/); skipping.\n" ; \
	    printf "#   add it:  git submodule add -b release \\\\\n" ; \
	    printf "#            https://gitlab.common-lisp.net/asdf/asdf.git modules/asdf-src\n" ; \
	  fi

factorial:
	: nothing to do

clean:
	-rm -rf asdf
