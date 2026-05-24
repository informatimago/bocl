all: test

include ../sources/common.mk

BOCL        = ../sources/bocl
LIBBOCL     = ../sources/libbocl.$(SO)
# PROGRAMS    = test_ci test_reader test_eval test_arithmetic test_load test_packages
PROGRAMS    = test_reader test_eval test_arithmetic test_load test_packages
RUN_TARGETS = $(PROGRAMS:%=run_%)


test_reader_SRCS = test_reader.c simple_test.c
test_reader_LIBS = -L../sources -lbocl

test_eval_SRCS = test_eval.c simple_test.c
test_eval_LIBS = -L../sources -lbocl

test_arithmetic_SRCS = test_arithmetic.c simple_test.c
test_arithmetic_LIBS = -L../sources -lbocl

test_load_SRCS = test_load.c simple_test.c
test_load_LIBS = -L../sources -lbocl

test_packages_SRCS = test_packages.c simple_test.c
test_packages_LIBS = -L../sources -lbocl

test_ci_SRCS = test_ci.c
test_ci_LIBS =
# $(test_ci_SRCS) \
#

all_SRCS = \
	$(test_reader_SRCS) \
	test_eval.c \
	test_arithmetic.c \
	test_load.c \
	test_packages.c

ifeq ($(UNAME),Darwin)
INCLUDES    += -I/opt/local/include -I../sources
LIBRARIES   += -L/opt/local/lib -lcunit
else
INCLUDES    += -I../sources
LIBRARIES   += -lcunit
endif
CFLAGS  	+=
LDFLAGS 	+=
RUNLIBS     += $(shell echo $$(pwd)/../sources)

.PHONY: clean deps build test tests $(RUN_TARGETS)

build: deps $(LIBBOCL) $(BOCL) $(PROGRAMS)
tests: deps $(RUN_TARGETS)
test:  deps $(RUN_TARGETS)

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
	$(VERBOSE) $(RUN) ./test_ci

test_reader.e : test_reader.c

test_reader: $(test_reader_SRCS:%.c=%.d) $(test_reader_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_reader_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_reader_SRCS:%.c=%.o) $(test_reader_LIBS)

run_test_reader:test_reader
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_reader ;echo status=$$?

test_eval: $(test_eval_SRCS:%.c=%.d) $(test_eval_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_eval_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_eval_SRCS:%.c=%.o) $(test_eval_LIBS)

run_test_eval:test_eval
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_eval ;echo status=$$?

test_arithmetic: $(test_arithmetic_SRCS:%.c=%.d) $(test_arithmetic_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_arithmetic_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_arithmetic_SRCS:%.c=%.o) $(test_arithmetic_LIBS)

run_test_arithmetic:test_arithmetic
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_arithmetic ;echo status=$$?

test_load: $(test_load_SRCS:%.c=%.d) $(test_load_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_load_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_load_SRCS:%.c=%.o) $(test_load_LIBS)

run_test_load:test_load
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_load ;echo status=$$?

test_packages: $(test_packages_SRCS:%.c=%.d) $(test_packages_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_packages_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_packages_SRCS:%.c=%.o) $(test_packages_LIBS)

run_test_packages:test_packages
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_packages ;echo status=$$?

run_test_bocl: $(BOCL)
	@printf "# Testing bocl\n"
	$(VERBOSE) output=$$($(RUN_ENV) $(BOCL) --quiet 2>&1) || ( status=$$? ;\
	expected="$$(echo '@ERROR: in bocl_read_eval_cstring:  NOT IMPLEMENTED YET' |tr '@' '\012')" ;\
	[ "$$output" = "$$expected" ] && [ $$status -eq 65 ] || ( printf "output = x%sx\n" "$$output" ; printf "expected = x%sx\n" "$$expected" ; exit 1 ; ) ; )
	@printf "# Success\n"

$(LIBBOCL) $(BOCL):
	$(VERBOSE) $(MAKE) $(MFLAGS) -C ../sources build

#### THE END ####
