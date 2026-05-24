# project Makefile (used by gitlab CI).

SUBDIRS   = specifications design sources tests documentation
DOCTARGET = pdfs

all:       		 documents build test
documents: 		 specifications design documentation
.PHONY:    		 documents bocl clean clean-backups regenerate                  \
		 test tests specifications design documentation

specifications:  ; @cd specifications && $(MAKE) $(MFLAGS) $(DOCTARGET)
design:          ; @cd design         && $(MAKE) $(MFLAGS) $(DOCTARGET)
build:           ; @for dir in sources tests ; do  $(MAKE) -C $$dir $(MFLAGS) build ; done
test tests:      ; @cd tests          && $(MAKE) $(MFLAGS) test
documentation:   ; @cd documentation  && $(MAKE) $(MFLAGS) $(DOCTARGET)
clean:
	-@rm -f test.xml
	@for dir in $(SUBDIRS)     ; do $(MAKE) -C $$dir $(MFLAGS) clean ; done

# Regenerate the Lisp-generated C sources (kernel_objects.*, built_in_functions.*,
# kernel_structure_functions.*, structure_functions.*). Needs a host Common Lisp.
# Override the impl with `make LISP=sbcl regenerate` etc.
regenerate:
	@printf "# Regenerating generated C sources via host CL\n"
	$(MAKE) -C sources $(MFLAGS) \
		kernel_objects.h kernel_objects.c \
		kernel_functions.h built_in_functions.h built_in_functions.c \
		kernel_structure_functions.h kernel_structure_functions.c \
		structure_functions.h structure_functions.c

# Remove the *.~N~ Emacs backup files cluttering the tree. They are gitignored
# by the `*~` pattern in .gitignore, but having hundreds of them in the working
# directory makes navigation painful.
clean-backups:
	@printf "# Removing Emacs *.~N~ backup files\n"
	-find . -type f -name '*.~[0-9]*~' -print -delete
