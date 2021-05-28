# project Makefile (used by gitlab CI).

all: specifications test
bocl:  			 ; @cd src 			  && $(MAKE) $(MFLAGS) bocl
clean: 			 ; @cd src 			  && $(MAKE) $(MFLAGS) clean
test:  			 ; @cd src 			  && $(MAKE) $(MFLAGS) test
specifications:  ; @cd specifications && $(MAKE) $(MFLAGS) pdfs
