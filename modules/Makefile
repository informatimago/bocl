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
modules: asdf

# ---- ASDF -----------------------------------------------------------------
# Source submodule in asdf-src/ (added on authorization; see README.org).  Build
# its single-file release and stage it as the install-ready module asdf/.
asdf:
	@ if [ -f asdf-src/Makefile ] ; then \
	    printf "# Building ASDF (single-file release) with $(LISP)\n" ; \
	    $(MAKE) -C asdf-src build/asdf.lisp && \
	    mkdir -p asdf && \
	    cp asdf-src/build/asdf.lisp asdf/asdf.lisp && \
	    printf '(:name "asdf"\n :files ("asdf.lisp"))\n' > asdf/module.manifest && \
	    printf "# Staged module asdf/ (asdf.lisp + module.manifest)\n" ; \
	  else \
	    printf "# ASDF submodule not present (modules/asdf-src/); skipping.\n" ; \
	    printf "#   add it:  git submodule add -b release \\\\\n" ; \
	    printf "#            https://gitlab.common-lisp.net/asdf/asdf.git modules/asdf-src\n" ; \
	  fi

clean:
	-rm -rf asdf
