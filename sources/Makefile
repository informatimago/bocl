all:build

PROGRAMS = print_types bocl

common_SRCS = \
	bocl.c string_list.c \
	kernel.c kernel_objects.c memory.c boerror.c \
	built_in_functions.c arithmetic.c \
	eval.c environment.c \
	variable.c cons.c data.c \
	structure.c kernel_structure_functions.c structure_functions.c \
	stream.c reader.c printer.c filename.c \
	character.c bostring.c bit_vector.c vector.c \
	integer.c float.c \
	\
	strdup.c \
	\
	$(DEBUG_SRCS)

all_SRCS = main.c $(common_SRCS)

libbocl_SRCS 	 = $(common_SRCS)
libbocl_LIBS 	 = -lm -lc
libbocl_INCS 	 =

bocl_SRCS        = main.c $(common_SRCS)
bocl_LIBS        = -L. -lbocl -lm -lc
bocl_INCS        = -I.

print_types_SRCS = print_types.c
print_types_LIBS =
print_types_INCS =

INCLUDES     += $(libbocl_INCS)
LIBRARIES    += $(libbocl_LIBS)

.PHONY = clean prepare-macos deps build predefines run sign-gdb gdb \
	run-print-limits run-print-types

include common.mk
deps:   $(all_SRCS:%.c=%.d)
include $(all_SRCS:%.c=%.d)

build: deps libbocl.$(SO) $(PROGRAMS)

clean:
	@ printf "# Removing object files and target\n"
	-$(VERBOSE) rm -rf libbocl.* $(PROGRAMS) *.o *.d *.e *.dSYM
	-$(VERBOSE)	rm -rf *.tem */*.tem *.[ld]x{32,64}fsl */*.[ld]x{32,64}fsl

prepare-macos:
	- sudo xcode-select --install
	  sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
	  xcode-select --print-path


predefines:
	$(VERBOSE) printf '#define _POSIX_SOURCE\n#include <unistd.h>\n' > __predefines.c
	$(VERBOSE) cpp -dM  __predefines.c | sort
	-$(VERBOSE) rm -f   __predefines.c

gdb: bocl
	$(VERBOSE) $(RUN) gdb ./bocl

# on macOS, gdb must be codesigned.
sign-gdb:
	@ printf "# Signing gdb with sudo\n"
	$(VERBOSE) true See 'https://gist.github.com/gravitylow/fb595186ce6068537a6e9da6d8b5b96d'
	$(VERBOSE) echo '<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd"><plist version="1.0"><dict><key>com.apple.security.cs.debugger</key><true/></dict></plist>' >  gdb-entitlement.xml
	$(VERBOSE) sudo codesign --entitlements gdb-entitlement.xml -fs gdb-cert "$$(which gdb)"

run: bocl
	$(VERBOSE) $(RUN) ./bocl $(ARGUMENTS)

# ----------------------------------------
print_types: $(print_types_SRCS:%.c=%.d) $(print_types_SRCS:%.c=%.o)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(print_types_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(print_types_SRCS:%.c=%.o)

libbocl.$(SO): $(libbocl_SRCS:%.c=%.d) $(libbocl_SRCS:%.c=%.o)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(bocl_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKLIB) -o $@ $(libbocl_SRCS:%.c=%.o)

# for now, we link all the bocl objects into bocl:
bocl: $(bocl_SRCS:%.c=%.d) $(bocl_SRCS:%.c=%.o)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(bocl_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(bocl_SRCS:%.c=%.o)

#bocl: $(bocl_SRCS:%.c=%.d) $(bocl_SRCS:%.c=%.o) libbocl.$(SO)
#	@ printf "# Linking %-20s   from %s\n" "$@" "$(bocl_SRCS:%.c=%.o)"
#	$(VERBOSE) $(LINKBIN) -o $@ $(bocl_SRCS:%.c=%.o)  $(bocl_LIBS)


# Host CL used to (re)generate kernel_objects.* / built_in_functions.* /
# structure_functions.* etc. The generated outputs are committed, so the host
# CL is only needed for `make regenerate`, not for an ordinary build.
#
# Auto-detect ccl, sbcl, or clisp on PATH. Override with `make LISP=<impl>`.
LISP ?= $(firstword $(shell command -v ccl 2>/dev/null) \
                    $(shell command -v sbcl 2>/dev/null) \
                    $(shell command -v clisp 2>/dev/null))

LISP_BASENAME = $(notdir $(LISP))

ifeq ($(LISP_BASENAME),ccl)
LISP_NORC = --no-init
LISP_LOAD = --load
LISP_EVAL = --eval
LISP_QUIT = --eval '(ccl:quit 0)'
else ifeq ($(LISP_BASENAME),sbcl)
LISP_NORC = --no-userinit --no-sysinit
LISP_LOAD = --load
LISP_EVAL = --eval
LISP_QUIT = --quit
else ifeq ($(LISP_BASENAME),clisp)
LISP_NORC = -norc
LISP_LOAD = -i
LISP_EVAL = -x
LISP_QUIT = -x '(ext:quit 0)'
else
LISP      = $(error No host Common Lisp found on PATH; install ccl, sbcl, or clisp, or set LISP=<impl> on the make command line)
LISP_NORC =
LISP_LOAD =
LISP_EVAL =
LISP_QUIT =
endif

kernel_objects.h kernel_objects.c : generate-kernel-objects.lisp
	@printf "# Generating %s from %s\n" "$@" "$^"
	$(VERBOSE) $(LISP) $(LISP_NORC) \
		$(LISP_LOAD) generate-kernel-objects.lisp \
		$(LISP_EVAL) '(com.informatimago.bocl.kernel.generate:generate-c-code)' \
		$(LISP_QUIT)

kernel_functions.h built_in_functions.h built_in_functions.c \
lisp/kernel-package.lisp : generate-built-in-functions.lisp
	@printf "# Generating %s from %s\n" "$@" "$^"
	$(VERBOSE) $(LISP) $(LISP_NORC) \
		$(LISP_LOAD) "$$HOME/quicklisp/setup.lisp" \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/common-lisp/cesarum/"   asdf:*central-registry*)' \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/common-lisp/lisp-sexp/" asdf:*central-registry*)' \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/common-lisp/"           asdf:*central-registry*)' \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/"                       asdf:*central-registry*)' \
		$(LISP_EVAL) '(ql:quickload :com.informatimago.common-lisp.cesarum)' \
		$(LISP_LOAD) generate-built-in-functions.lisp \
		$(LISP_EVAL) '(com.informatimago.bocl.kernel.generate.kernel-functions:generate-built-in-functions)' \
		$(LISP_QUIT)


kernel_structure_functions.h kernel_structure_functions.c \
structure_functions.h structure_functions.c \
lisp/kernel-package+structure.lisp : generate-built-in-functions.lisp generate-structures.lisp
	@printf "# Generating %s from %s\n" "$@" "$^"
	$(VERBOSE) $(LISP) $(LISP_NORC) \
		$(LISP_LOAD) "$$HOME/quicklisp/setup.lisp" \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/common-lisp/cesarum/"   asdf:*central-registry*)' \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/common-lisp/lisp-sexp/" asdf:*central-registry*)' \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/common-lisp/"           asdf:*central-registry*)' \
		$(LISP_EVAL) '(push #P"~/src/public/com-informatimago/"                       asdf:*central-registry*)' \
		$(LISP_EVAL) '(ql:quickload :com.informatimago.common-lisp.cesarum)' \
		$(LISP_LOAD) generate-built-in-functions.lisp \
		$(LISP_LOAD) generate-structures.lisp \
		$(LISP_EVAL) '(com.informatimago.bocl.kernel.generate.kernel-functions:generate-structure-functions)' \
		$(LISP_QUIT)


run-print-limits: # bocl
	$(VERBOSE) $(LISP) $(LISP_NORC) \
		$(LISP_LOAD) lisp/print-limits.lisp \
		$(LISP_EVAL) '(print-limits)' \
		$(LISP_QUIT)
#	 $(VERBOSE) $(RUN) ./bocl $(LISP_NORC) \
#	 	$(LISP_LOAD) lisp/print-limits.lisp \
#	 	$(LISP_EVAL) '(print-limits)' \
#	 	$(LISP_QUIT)

run-print-types:print_types
	$(VERBOSE) $(RUN) ./print_types


environment.e: environment.c
	gcc -E   -o environment.e  environment.c && cat  environment.e

#### THE END ####
