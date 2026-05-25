all: test

include ../sources/common.mk

BOCL        = ../sources/bocl
LIBBOCL     = ../sources/libbocl.$(SO)
# PROGRAMS    = test_ci test_reader test_eval test_arithmetic test_load test_packages test_hash_table test_conditions
PROGRAMS    = test_reader test_eval test_arithmetic test_load test_packages test_hash_table test_conditions test_reader_macros test_macros test_sequences test_setf test_control test_seq_lib test_strings test_numeric_extras
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

test_hash_table_SRCS = test_hash_table.c simple_test.c
test_hash_table_LIBS = -L../sources -lbocl

test_conditions_SRCS = test_conditions.c simple_test.c
test_conditions_LIBS = -L../sources -lbocl

test_reader_macros_SRCS = test_reader_macros.c simple_test.c
test_reader_macros_LIBS = -L../sources -lbocl

test_macros_SRCS = test_macros.c simple_test.c
test_macros_LIBS = -L../sources -lbocl

test_sequences_SRCS = test_sequences.c simple_test.c
test_sequences_LIBS = -L../sources -lbocl

test_setf_SRCS = test_setf.c simple_test.c
test_setf_LIBS = -L../sources -lbocl

test_control_SRCS = test_control.c simple_test.c
test_control_LIBS = -L../sources -lbocl

test_seq_lib_SRCS = test_seq_lib.c simple_test.c
test_seq_lib_LIBS = -L../sources -lbocl

test_strings_SRCS = test_strings.c simple_test.c
test_strings_LIBS = -L../sources -lbocl

test_numeric_extras_SRCS = test_numeric_extras.c simple_test.c
test_numeric_extras_LIBS = -L../sources -lbocl

test_ci_SRCS = test_ci.c
test_ci_LIBS =
# $(test_ci_SRCS) \
#

all_SRCS = \
	$(test_reader_SRCS) \
	test_eval.c \
	test_arithmetic.c \
	test_load.c \
	test_packages.c \
	test_hash_table.c \
	test_conditions.c \
	test_reader_macros.c \
	test_macros.c \
	test_sequences.c \
	test_setf.c \
	test_control.c \
	test_seq_lib.c \
	test_strings.c \
	test_numeric_extras.c

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

test_hash_table: $(test_hash_table_SRCS:%.c=%.d) $(test_hash_table_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_hash_table_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_hash_table_SRCS:%.c=%.o) $(test_hash_table_LIBS)

run_test_hash_table:test_hash_table
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_hash_table ;echo status=$$?

test_conditions: $(test_conditions_SRCS:%.c=%.d) $(test_conditions_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_conditions_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_conditions_SRCS:%.c=%.o) $(test_conditions_LIBS)

run_test_conditions:test_conditions
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_conditions ;echo status=$$?

test_reader_macros: $(test_reader_macros_SRCS:%.c=%.d) $(test_reader_macros_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_reader_macros_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_reader_macros_SRCS:%.c=%.o) $(test_reader_macros_LIBS)

run_test_reader_macros:test_reader_macros
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_reader_macros ;echo status=$$?

test_macros: $(test_macros_SRCS:%.c=%.d) $(test_macros_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_macros_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_macros_SRCS:%.c=%.o) $(test_macros_LIBS)

run_test_macros:test_macros
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_macros ;echo status=$$?

test_sequences: $(test_sequences_SRCS:%.c=%.d) $(test_sequences_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_sequences_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_sequences_SRCS:%.c=%.o) $(test_sequences_LIBS)

run_test_sequences:test_sequences
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_sequences ;echo status=$$?

test_setf: $(test_setf_SRCS:%.c=%.d) $(test_setf_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_setf_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_setf_SRCS:%.c=%.o) $(test_setf_LIBS)

run_test_setf:test_setf
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_setf ;echo status=$$?

test_control: $(test_control_SRCS:%.c=%.d) $(test_control_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_control_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_control_SRCS:%.c=%.o) $(test_control_LIBS)

run_test_control:test_control
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_control ;echo status=$$?

test_seq_lib: $(test_seq_lib_SRCS:%.c=%.d) $(test_seq_lib_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_seq_lib_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_seq_lib_SRCS:%.c=%.o) $(test_seq_lib_LIBS)

run_test_seq_lib:test_seq_lib
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_seq_lib ;echo status=$$?

test_strings: $(test_strings_SRCS:%.c=%.d) $(test_strings_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_strings_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_strings_SRCS:%.c=%.o) $(test_strings_LIBS)

run_test_strings:test_strings
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_strings ;echo status=$$?

test_numeric_extras: $(test_numeric_extras_SRCS:%.c=%.d) $(test_numeric_extras_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_numeric_extras_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_numeric_extras_SRCS:%.c=%.o) $(test_numeric_extras_LIBS)

run_test_numeric_extras:test_numeric_extras
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_numeric_extras ;echo status=$$?

run_test_bocl: $(BOCL)
	@printf "# Testing bocl\n"
	$(VERBOSE) output=$$($(RUN_ENV) $(BOCL) --quiet 2>&1) || ( status=$$? ;\
	expected="$$(echo '@ERROR: in bocl_read_eval_cstring:  NOT IMPLEMENTED YET' |tr '@' '\012')" ;\
	[ "$$output" = "$$expected" ] && [ $$status -eq 65 ] || ( printf "output = x%sx\n" "$$output" ; printf "expected = x%sx\n" "$$expected" ; exit 1 ; ) ; )
	@printf "# Success\n"

$(LIBBOCL) $(BOCL):
	$(VERBOSE) $(MAKE) $(MFLAGS) -C ../sources build

#### THE END ####
