# project Makefile (used by gitlab CI).

SUBDIRS   = specifications design sources tests documentation modules
DOCTARGET = pdfs

all:       		 documents build test
documents: 		 specifications design documentation
.PHONY:    		 documents bocl clean clean-backups regenerate                  \
		 test tests specifications design documentation modules          \
		 install install-strip uninstall info dist check-lib             \
		 check-versions

specifications:  ; @cd specifications && $(MAKE) $(MFLAGS) $(DOCTARGET)
design:          ; @cd design         && $(MAKE) $(MFLAGS) $(DOCTARGET)
# Build the C/Lisp core, then the system modules (asdf etc.; a no-op until their
# source submodules are present — see modules/README.org).
build:           ; @for dir in sources tests modules ; do  $(MAKE) -C $$dir $(MFLAGS) build ; done
modules:         ; @$(MAKE) -C modules $(MFLAGS)
test tests:      ; @cd tests          && $(MAKE) $(MFLAGS) test
documentation:   ; @cd documentation  && $(MAKE) $(MFLAGS) $(DOCTARGET)

# Install / package the deliverables.  These live in sources/Makefile (which knows
# the FHS layout under $(DESTDIR)$(PREFIX)); delegate to it.  Pass PREFIX / DESTDIR
# on the command line, e.g.  make install PREFIX=/opt/local DESTDIR=/tmp/stage
install install-strip uninstall info dist check-lib:
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
