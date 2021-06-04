# project Makefile (used by gitlab CI).

all: documents test
.PHONY: documents bocl clean test specifications design
documents: specifications design
bocl:  			 ; @cd sources        && $(MAKE) $(MFLAGS) bocl
clean: 			 ; @cd sources        && $(MAKE) $(MFLAGS) clean
test:  			 ; @cd sources        && $(MAKE) $(MFLAGS) test
specifications:  ; @cd specifications && $(MAKE) $(MFLAGS) pdfs
design:          ; @cd design         && $(MAKE) $(MFLAGS) pdfs
