# project Makefile (used by gitlab CI).

all: bocl

bocl:
	@cd src && $(MAKE) $(MFLAGS)

clean:
	@cd src && $(MAKE) $(MFLAGS)

test:
	@cd src && $(MAKE) $(MFLAGS)
