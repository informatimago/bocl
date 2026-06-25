# project Makefile (used by gitlab CI).

SUBDIRS   = specifications design sources tests documentation
DOCTARGET = pdfs

all:       		 documents build test
documents: 		 specifications design documentation
.PHONY:    		 documents bocl clean clean-backups regenerate                  \
		 test tests specifications design documentation                  \
		 install install-strip uninstall info dist check-lib

specifications:  ; @cd specifications && $(MAKE) $(MFLAGS) $(DOCTARGET)
design:          ; @cd design         && $(MAKE) $(MFLAGS) $(DOCTARGET)
build:           ; @for dir in sources tests ; do  $(MAKE) -C $$dir $(MFLAGS) build ; done
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

# Remove the *.~N~ Emacs backup files cluttering the tree. They are gitignored
# by the `*~` pattern in .gitignore, but having hundreds of them in the working
# directory makes navigation painful.
clean-backups:
	@printf "# Removing Emacs *.~N~ backup files\n"
	-find . -type f -name '*.~[0-9]*~' -print -delete
