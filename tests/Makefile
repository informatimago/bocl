CFLAGS  	= -I/opt/local/include
LDFLAGS 	= -L/opt/local/lib -lcunit
PROGRAMS    = bocl test_reader test_ci
RUN_TARGETS = $(PROGRAMS:%=run_%)
BOCL    	= ../sources/BOCL

all: test

.PHONY: test tests $(RUN_TARGETS)

test tests: $(RUN_TARGETS)


test_ci:ci_test.c
	$(CC) $(CFLAGS) -DNO_CI -o $@ $^

run_test_ci:test_ci
	./test_ci

run_test_reader:test_reader
	@printf "# Testing test_reader\n"
	$(VERBOSE) ./test_reader

run_test_bocl: $(BOCL)
	@printf "# Testing bocl\n"
	$(VERBOSE) output=$$($(RUN_ENV) $(BOCL) --quiet 2>&1) || ( status=$$? ;\
	expected="$$(echo '@ERROR: in bocl_read_eval_cstring:  NOT IMPLEMENTED YET' |tr '@' '\012')" ;\
	[ "$$output" = "$$expected" ] && [ $$status -eq 65 ] || ( printf "output = x%sx\n" "$$output" ; printf "expected = x%sx\n" "$$expected" ; exit 1 ; ) ; )
	@printf "# Success\n"

