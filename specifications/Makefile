# -*- mode:makefile-gmake; coding:utf-8; indent-tabs-mode:t -*-


NAME=specifications
PDFS=$(NAME).pdf

VERBOSE=@

all:documents 

documents pdfs :$(PDFS)

.PHONY:: documents pdfs

# Generate PDF from org-mode document.
%.pdf : %.org
	@ printf "# Generating %-20s from %s\n" "$@" "$<"
	-$(VERBOSE) rm -f $@
	$(VERBOSE) emacs --batch \
		--eval '(find-file "'$<'")' \
		--funcall org-latex-export-to-pdf \
		--kill

clean::
	@ printf "# Cleaning up\n"
	-$(VERBOSE) rm -f $(NAME).tex missfont.log ./--version.lock


.PHONY::  questions tbd

questions:
	grep -nH -e '\<\(QUESTIONS\?\|HOW\)\>' *.{txt,org}

tbd:
	grep -nH -e '\<\(TBD\|GENERAL_DESCRIPTION\|DESCRIPTION_OF_THE_RESPONSIBILITIES_OF_THE_ENTITY\|LIST_OF_ATTRIBUTES\|LIST_OF_ASSOCIATIONS_WITH_OTHER_ENTITIES\)\>' *.txt
