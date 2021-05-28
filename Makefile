# project Makefile (used by gitlab CI).

all:   			 ; @cd src 			  && $(MAKE) $(MFLAGS) all
bocl:  			 ; @cd src 			  && $(MAKE) $(MFLAGS) bocl
clean: 			 ; @cd src 			  && $(MAKE) $(MFLAGS) clean
test:  			 ; @cd src 			  && $(MAKE) $(MFLAGS) test
specifications:  ; @cd specifications && $(MAKE) $(MFLAGS) specifications
