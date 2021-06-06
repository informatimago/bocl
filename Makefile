# project Makefile (used by gitlab CI).

SUBDIRS   = specifications design sources tests documentation
DOCTARGET = pdfs

all:       		 documents build test
documents: 		 specifications design documentation
.PHONY:    		 documents bocl clean test tests specifications design documentation

specifications:  ; @cd specifications && $(MAKE) $(MFLAGS) $(DOCTARGET)
design:          ; @cd design         && $(MAKE) $(MFLAGS) $(DOCTARGET)
build:           ; @for dir in sources tests ; do  $(MAKE) -C $$dir $(MFLAGS) build DEFINES=-DGITLAB_CI=1 VERBOSE= ; done
test tests:      ; @cd tests          && $(MAKE) $(MFLAGS) test  DEFINES=-DGITLAB_CI=1 VERBOSE=
documentation:   ; @cd documentation  && $(MAKE) $(MFLAGS) $(DOCTARGET)
clean:           ; @for dir in $(SUBDIRS)     ; do $(MAKE) -C $$dir $(MFLAGS) clean ; done
