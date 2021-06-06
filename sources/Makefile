all:build


all_SRCS = $(common_SRCS) \
	main.c

libbocl_SRCS = $(untested_SRCS) $(tested_SRCS)
libbocl_LIBS = -lm
libbocl_INCS =

tested_SRCS = \
	reader.c

untested_SRCS = \
	bocl.c string_list.c \
	kernel.c kernel_objects.c memory.c boerror.c \
	built_in_functions.c \
	variable.c cons.c data.c \
	stream.c  printer.c filename.c \
	character.c bostring.c bit_vector.c vector.c \
	integer.c float.c \
	\
	strdup.c \
	\
	$(DEBUG_SRCS)

bocl_SRCS        = main.c
bocl_LIBS        = -L. -lbocl
bocl_INCS        = -I.

INCLUDES     += $(libbocl_INCS)
LIBRARIES    += $(libbocl_LIBS)

.PHONY = clean prepare-macos deps build predefines run sign-gdb gdb

include common.mk
deps:   $(all_SRCS:%.c=%.d)
include $(all_SRCS:%.c=%.d)

build: deps libbocl.$(SO) bocl

clean:
	@ printf "# Removing object files and target\n"
	-$(VERBOSE) rm -rf bocl libbocl.* *.o *.d *.[ld]x{32,64}fsl *.dSYM

prepare-macos:
	- sudo xcode-select --install
	  sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
	  xcode-select --print-path


predefines:
	$(VERBOSE) printf '#define _POSIX_SOURCE\n#include <unistd.h>\n' > __predefines.c
	$(VERBOSE) cpp -dM  __predefines.c | sort
	-$(VERBOSE) rm -f   __predefines.c

gdb: bocl
	$(VERBOSE) $(RUN_ENV) gdb ./bocl

# on macOS, gdb must be codesigned.
sign-gdb:
	@ printf "# Signing gdb with sudo\n"
	$(VERBOSE) true See 'https://gist.github.com/gravitylow/fb595186ce6068537a6e9da6d8b5b96d'
	$(VERBOSE) echo '<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd"><plist version="1.0"><dict><key>com.apple.security.cs.debugger</key><true/></dict></plist>' >  gdb-entitlement.xml
	$(VERBOSE) sudo codesign --entitlements gdb-entitlement.xml -fs gdb-cert "$$(which gdb)"

run: bocl
	$(VERBOSE) $(RUN_ENV) ./bocl $(ARGUMENTS)

# ----------------------------------------

libbocl.$(SO): $(libbocl_SRCS:%.c=%.d) $(libbocl_SRCS:%.c=%.o)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(bocl_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKLIB) -o $@ $(libbocl_SRCS:%.c=%.o)

# for now, we link all the bocl objects into bocl:
bocl: $(bocl_SRCS:%.c=%.d) $(bocl_SRCS:%.c=%.o) $(libbocl_SRCS:%.c=%.o)
	@ printf "# Linking %-20s   from %s\n" "$@" "$(bocl_SRCS:%.c=%.o)"
	$(VERBOSE) $(LINKBIN) -o $@ $(bocl_SRCS:%.c=%.o)  $(libbocl_SRCS:%.c=%.o)

#bocl: $(bocl_SRCS:%.c=%.d) $(bocl_SRCS:%.c=%.o) libbocl.$(SO)
#	@ printf "# Linking %-20s   from %s\n" "$@" "$(bocl_SRCS:%.c=%.o)"
#	$(VERBOSE) $(LINKBIN) -o $@ $(bocl_SRCS:%.c=%.o)  $(bocl_LIBS)


LISP=ccl
LISP_NORC=--no-init
LISP_LOAD=--load
LISP_EVAL=--eval
LISP_QUIT=--eval '(ccl:quit 0)'

kernel_objects.h kernel_objects.c : generate-kernel-objects.lisp
	@printf "# Generating %s from %s\n" "$@" "$^"
	$(VERBOSE) $(LISP) $(LISP_NORC) \
		$(LISP_LOAD) generate-kernel-objects.lisp \
		$(LISP_EVAL) '(com.informatimago.bocl.kernel.generate:generate-c-code)' \
		$(LISP_QUIT)

kernel_functions.h built_in_functions.h built_in_functions.c : generate-built-in-functions.lisp
	@printf "# Generating %s from %s\n" "$@" "$^"
	$(VERBOSE) $(LISP) $(LISP_NORC) \
		$(LISP_LOAD) generate-built-in-functions.lisp \
		$(LISP_EVAL) '(com.informatimago.bocl.kernel.generate.kernel-functions:generate-built-in-functions)' \
		$(LISP_QUIT)



#### THE END ####
