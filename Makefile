# project Makefile (used by gitlab CI).

SUBDIRS   = specifications design sources tests documentation
DOCTARGET = pdfs

all:       		 documents build test
documents: 		 specifications design documentation
.PHONY:    		 documents bocl clean test tests specifications design documentation

specifications:  ; @cd specifications && $(MAKE) $(MFLAGS) $(DOCTARGET)
design:          ; @cd design         && $(MAKE) $(MFLAGS) $(DOCTARGET)
build:           ; @cd sources        && $(MAKE) $(MFLAGS) build
test tests:      ; @cd tests          && $(MAKE) $(MFLAGS) test
documentation:   ; @cd documentation  && $(MAKE) $(MFLAGS) $(DOCTARGET)
clean:           ; @for dir in $(SUBDIRS) ; do $(MAKE) -C $$dir $(MFLAGS) clean ; done
