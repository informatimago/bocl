# project Makefile (used by gitlab CI).

all: documents test
.PHONY: documents bocl clean test specifications design
documents: specifications design
bocl:  			 ; @cd src 			  && $(MAKE) $(MFLAGS) bocl
clean: 			 ; @cd src 			  && $(MAKE) $(MFLAGS) clean
test:  			 ; @cd src 			  && $(MAKE) $(MFLAGS) test
specifications:  ; @cd specifications && $(MAKE) $(MFLAGS) pdfs
design:          ; @cd design         && $(MAKE) $(MFLAGS) pdfs
