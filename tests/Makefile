all: test

include ../sources/common.mk

BOCL        = ../sources/bocl
LIBBOCL     = ../sources/libbocl.$(SO)
# PROGRAMS    = test_ci test_reader test_eval test_arithmetic test_load test_packages test_hash_table test_conditions
PROGRAMS    = test_reader test_eval test_arithmetic test_load test_packages test_hash_table test_conditions test_reader_macros test_macros test_sequences test_setf test_control test_seq_lib test_strings test_numeric_extras test_destructuring test_structures test_handler_bind test_restarts test_format test_loop test_list_utils test_misc test_slice15 test_slice16 test_slice17 test_slice18 test_clos test_slice20 test_slice21 test_slice22 test_slice23 test_slice24 test_slice25 test_slice26 test_slice27 test_slice28 test_slice29
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
	test_numeric_extras.c \
	test_destructuring.c \
	test_structures.c \
	test_handler_bind.c \
	test_restarts.c \
	test_format.c \
	test_loop.c \
	test_list_utils.c

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

test_destructuring_SRCS = test_destructuring.c simple_test.c
test_destructuring_LIBS = -L../sources -lbocl

test_destructuring: $(test_destructuring_SRCS:%.c=%.d) $(test_destructuring_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_destructuring_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_destructuring_SRCS:%.c=%.o) $(test_destructuring_LIBS)

run_test_destructuring:test_destructuring
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_destructuring ;echo status=$$?

test_structures_SRCS = test_structures.c simple_test.c
test_structures_LIBS = -L../sources -lbocl

test_structures: $(test_structures_SRCS:%.c=%.d) $(test_structures_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_structures_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_structures_SRCS:%.c=%.o) $(test_structures_LIBS)

run_test_structures:test_structures
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_structures ;echo status=$$?

test_handler_bind_SRCS = test_handler_bind.c simple_test.c
test_handler_bind_LIBS = -L../sources -lbocl

test_handler_bind: $(test_handler_bind_SRCS:%.c=%.d) $(test_handler_bind_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_handler_bind_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_handler_bind_SRCS:%.c=%.o) $(test_handler_bind_LIBS)

run_test_handler_bind:test_handler_bind
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_handler_bind ;echo status=$$?

test_restarts_SRCS = test_restarts.c simple_test.c
test_restarts_LIBS = -L../sources -lbocl

test_restarts: $(test_restarts_SRCS:%.c=%.d) $(test_restarts_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_restarts_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_restarts_SRCS:%.c=%.o) $(test_restarts_LIBS)

run_test_restarts:test_restarts
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_restarts ;echo status=$$?

test_format_SRCS = test_format.c simple_test.c
test_format_LIBS = -L../sources -lbocl

test_format: $(test_format_SRCS:%.c=%.d) $(test_format_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_format_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_format_SRCS:%.c=%.o) $(test_format_LIBS)

run_test_format:test_format
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_format ;echo status=$$?

test_loop_SRCS = test_loop.c simple_test.c
test_loop_LIBS = -L../sources -lbocl

test_loop: $(test_loop_SRCS:%.c=%.d) $(test_loop_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_loop_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_loop_SRCS:%.c=%.o) $(test_loop_LIBS)

run_test_loop:test_loop
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_loop ;echo status=$$?

test_list_utils_SRCS = test_list_utils.c simple_test.c
test_list_utils_LIBS = -L../sources -lbocl

test_list_utils: $(test_list_utils_SRCS:%.c=%.d) $(test_list_utils_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_list_utils_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_list_utils_SRCS:%.c=%.o) $(test_list_utils_LIBS)

run_test_list_utils:test_list_utils
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_list_utils ;echo status=$$?

test_misc_SRCS = test_misc.c simple_test.c
test_misc_LIBS = -L../sources -lbocl

test_misc: $(test_misc_SRCS:%.c=%.d) $(test_misc_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_misc_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_misc_SRCS:%.c=%.o) $(test_misc_LIBS)

run_test_misc:test_misc
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_misc ;echo status=$$?

test_slice15_SRCS = test_slice15.c simple_test.c
test_slice15_LIBS = -L../sources -lbocl

test_slice15: $(test_slice15_SRCS:%.c=%.d) $(test_slice15_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_slice15_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_slice15_SRCS:%.c=%.o) $(test_slice15_LIBS)

run_test_slice15:test_slice15
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_slice15 ;echo status=$$?

test_slice16_SRCS = test_slice16.c simple_test.c
test_slice16_LIBS = -L../sources -lbocl

test_slice16: $(test_slice16_SRCS:%.c=%.d) $(test_slice16_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_slice16_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_slice16_SRCS:%.c=%.o) $(test_slice16_LIBS)

run_test_slice16:test_slice16
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_slice16 ;echo status=$$?

test_slice17_SRCS = test_slice17.c simple_test.c
test_slice17_LIBS = -L../sources -lbocl

test_slice17: $(test_slice17_SRCS:%.c=%.d) $(test_slice17_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_slice17_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_slice17_SRCS:%.c=%.o) $(test_slice17_LIBS)

run_test_slice17:test_slice17
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_slice17 ;echo status=$$?

test_slice18_SRCS = test_slice18.c simple_test.c
test_slice18_LIBS = -L../sources -lbocl

test_slice18: $(test_slice18_SRCS:%.c=%.d) $(test_slice18_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_slice18_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_slice18_SRCS:%.c=%.o) $(test_slice18_LIBS)

run_test_slice18:test_slice18
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_slice18 ;echo status=$$?

test_clos_SRCS = test_clos.c simple_test.c
test_clos_LIBS = -L../sources -lbocl

test_clos: $(test_clos_SRCS:%.c=%.d) $(test_clos_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_clos_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_clos_SRCS:%.c=%.o) $(test_clos_LIBS)

run_test_clos:test_clos
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_clos ;echo status=$$?

test_slice20_SRCS = test_slice20.c simple_test.c
test_slice20_LIBS = -L../sources -lbocl

test_slice20: $(test_slice20_SRCS:%.c=%.d) $(test_slice20_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_slice20_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_slice20_SRCS:%.c=%.o) $(test_slice20_LIBS)

run_test_slice20:test_slice20
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_slice20 ;echo status=$$?

test_slice21_SRCS = test_slice21.c simple_test.c
test_slice21_LIBS = -L../sources -lbocl

test_slice21: $(test_slice21_SRCS:%.c=%.d) $(test_slice21_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_slice21_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_slice21_SRCS:%.c=%.o) $(test_slice21_LIBS)

run_test_slice21:test_slice21
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_slice21 ;echo status=$$?

test_slice22_SRCS = test_slice22.c simple_test.c
test_slice22_LIBS = -L../sources -lbocl

test_slice22: $(test_slice22_SRCS:%.c=%.d) $(test_slice22_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_slice22_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_slice22_SRCS:%.c=%.o) $(test_slice22_LIBS)

run_test_slice22:test_slice22
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_slice22 ;echo status=$$?

test_slice23_SRCS = test_slice23.c simple_test.c
test_slice23_LIBS = -L../sources -lbocl

test_slice23: $(test_slice23_SRCS:%.c=%.d) $(test_slice23_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_slice23_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_slice23_SRCS:%.c=%.o) $(test_slice23_LIBS)

run_test_slice23:test_slice23
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_slice23 ;echo status=$$?

test_slice24_SRCS = test_slice24.c simple_test.c
test_slice24_LIBS = -L../sources -lbocl

test_slice24: $(test_slice24_SRCS:%.c=%.d) $(test_slice24_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_slice24_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_slice24_SRCS:%.c=%.o) $(test_slice24_LIBS)

run_test_slice24:test_slice24
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_slice24 ;echo status=$$?

test_slice25_SRCS = test_slice25.c simple_test.c
test_slice25_LIBS = -L../sources -lbocl

test_slice25: $(test_slice25_SRCS:%.c=%.d) $(test_slice25_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_slice25_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_slice25_SRCS:%.c=%.o) $(test_slice25_LIBS)

run_test_slice25:test_slice25
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_slice25 ;echo status=$$?

test_slice26_SRCS = test_slice26.c simple_test.c
test_slice26_LIBS = -L../sources -lbocl

test_slice26: $(test_slice26_SRCS:%.c=%.d) $(test_slice26_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_slice26_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_slice26_SRCS:%.c=%.o) $(test_slice26_LIBS)

run_test_slice26:test_slice26
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_slice26 ;echo status=$$?

test_slice27_SRCS = test_slice27.c simple_test.c
test_slice27_LIBS = -L../sources -lbocl

test_slice27: $(test_slice27_SRCS:%.c=%.d) $(test_slice27_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_slice27_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_slice27_SRCS:%.c=%.o) $(test_slice27_LIBS)

run_test_slice27:test_slice27
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_slice27 ;echo status=$$?

test_slice28_SRCS = test_slice28.c simple_test.c
test_slice28_LIBS = -L../sources -lbocl

test_slice28: $(test_slice28_SRCS:%.c=%.d) $(test_slice28_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_slice28_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_slice28_SRCS:%.c=%.o) $(test_slice28_LIBS)

run_test_slice28:test_slice28
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_slice28 ;echo status=$$?

test_slice29_SRCS = test_slice29.c simple_test.c
test_slice29_LIBS = -L../sources -lbocl

test_slice29: $(test_slice29_SRCS:%.c=%.d) $(test_slice29_SRCS:%.c=%.o) $(LIBBOCL)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(test_slice29_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(test_slice29_SRCS:%.c=%.o) $(test_slice29_LIBS)

run_test_slice29:test_slice29
	@printf "# Testing %s\n" $^
	$(VERBOSE) $(RUN) ./test_slice29 ;echo status=$$?
