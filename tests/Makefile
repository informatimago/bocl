all: test

BOCL        = ../sources/bocl
LIBBOCL     = ../sources/libbocl.$(SO)
# PROGRAMS    = test_ci test_reader
PROGRAMS    = test_reader
RUN_TARGETS = $(PROGRAMS:%=run_%)


test_reader_SRCS = test_reader.c simple_test.c
test_reader_LIBS = -L../sources -lbocl

test_ci_SRCS = test_ci.c
test_ci_LIBS =
# $(test_ci_SRCS) \
#

all_SRCS = \
	$(test_reader_SRCS)

INCLUDES    += -I/opt/local/include -I../sources
LIBRARIES   += -L/opt/local/lib -lcunit
CFLAGS  	+=
LDFLAGS 	+=
RUNLIBS     += $(shell echo $$(pwd)/../sources)

.PHONY: clean deps build test tests $(RUN_TARGETS)

build: deps $(LIBBOCL) $(BOCL) $(PROGRAMS)
tests: deps $(RUN_TARGETS)
test:  deps $(RUN_TARGETS)

include ../sources/common.mk
deps:   $(all_SRCS:%.c=%.d)
include $(all_SRCS:%.c=%.d)


clean:
	@ printf "# Removing object files and target\n"
	-$(VERBOSE) rm -rf $(PROGRAMS) *.o *.d *.[ld]x{32,64}fsl *.dSYM
	-$(VERBOSE) rm -f *-Listing.xml *-Results.xml

test_ci: $(test_ci_SRCS:%.c=%.d) $(test_ci_SRCS:%.c=%.o)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_ci_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -DNO_CI -o $@ $^

run_test_ci: test_ci
	@printf "# Testing %s\n" $^
	$(VERBOSE)  r="$(RUNLIBS)" ; DYLD_LIBRARY_PATH=$${r// /:} ./test_ci

test_reader.e : test_reader.c

test_reader: $(test_reader_SRCS:%.c=%.d) $(test_reader_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_reader_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_reader_SRCS:%.c=%.o) $(test_reader_LIBS)

run_test_reader:test_reader
	@printf "# Testing %s\n" $^
	$(VERBOSE) r="$(RUNLIBS)" ; DYLD_LIBRARY_PATH=$${r// /:} ./test_reader ;echo status=$$?

run_test_bocl: $(BOCL)
	@printf "# Testing bocl\n"
	$(VERBOSE) output=$$($(RUN_ENV) $(BOCL) --quiet 2>&1) || ( status=$$? ;\
	expected="$$(echo '@ERROR: in bocl_read_eval_cstring:  NOT IMPLEMENTED YET' |tr '@' '\012')" ;\
	[ "$$output" = "$$expected" ] && [ $$status -eq 65 ] || ( printf "output = x%sx\n" "$$output" ; printf "expected = x%sx\n" "$$expected" ; exit 1 ; ) ; )
	@printf "# Success\n"

$(LIBBOCL) $(BOCL):
	$(VERBOSE) $(MAKE) $(MFLAGS) all

#### THE END ####
