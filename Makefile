# project Makefile (used by gitlab CI).

SUBDIRS   = specifications design sources tests documentation
DOCTARGET = pdfs

all:       		 documents build test
documents: 		 specifications design documentation
.PHONY:    		 documents bocl clean test tests specifications design documentation

specifications:  ; @cd specifications && $(MAKE) $(MFLAGS) $(DOCTARGET)
design:          ; @cd design         && $(MAKE) $(MFLAGS) $(DOCTARGET)
build:           ; @for dir in sources tests ; do  $(MAKE) -C $$dir $(MFLAGS) build ; done
test tests:      ; @cd tests          && $(MAKE) $(MFLAGS) test
documentation:   ; @cd documentation  && $(MAKE) $(MFLAGS) $(DOCTARGET)
clean:
	-@rm -f test.xml
	@for dir in $(SUBDIRS)     ; do $(MAKE) -C $$dir $(MFLAGS) clean ; done
