# notes/

Scratch material rescued from untracked files in the working tree. None of
this is wired into the build — it's reference material for future phases.

## Files

- [lambda-list-and-unwind-examples.lisp](lambda-list-and-unwind-examples.lisp)
  Worked examples of `&optional` / `&key` evaluation order, `&whole`,
  `(block … (return-from …))` interactions with `unwind-protect`, and
  the expected output for each call. Useful when implementing the rest of
  the lambda-list (Phase 2) and the condition system (Phase 4).
  Originally `sources/scratch.lisp`.

- [cpp-variadic-macros.h](cpp-variadic-macros.h)
  CPP toolkit (EVAL/REPEAT/IF/COMPL/INC/DEC, deferred expansion). Could
  cover the C-side generation of vararg shims for built-in functions if
  we ever want to do it without the Lisp generator. Currently unused.
  Originally `tests/macs.h`.
