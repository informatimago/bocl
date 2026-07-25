# AGENTS.md

General directives for this repository. Project-wide conventions and
the phased work plan live in [PLAN.md](PLAN.md); this file records the
directives an agent (or a human) must follow whatever it is working on.

## Versions and releases

The normative rules are shared across projects and live in
[`version-rules.md`](https://gitlab.com/informatimago/rules/-/blob/master/version-rules.md)
(repository: <https://gitlab.com/informatimago/rules>) — read it before
cutting a release. What follows is the summary and the parts specific
to bocl.

**The three kinds of ref, never mixed:**

- `release-M.m.d` — an **annotated tag**. A frozen feature set; never
  moves, never deleted. This is the ref the CI `collect` job cuts a
  GitLab Release from. `release-*` is a tag-only namespace: no branch
  may be named that way.
- `version-M.m` and `version-M` — **pointer branches**. Nothing is ever
  committed on them; they are only fast-forwarded onto a commit that
  carries a `release-*` tag, so checking one out always yields a
  released state. `version-1.9` is what a user tracking "1.9" follows.
- `master`, `maint-M.m`, `dev-M.m` — **development lines**, the only
  branches commits land on. `maint-M.m` is created on demand at the
  newest `release-M.m.d` when a fix must ship for a frozen series
  without dragging in trunk work.
- `fix-<slug>`, `feat-<slug>` — **work branches**. Every change gets
  one, forked from the *oldest* line that could need it, one concern
  per branch, deleted once merged. Merges only ever flow forward, from
  older line to newer.

**Version numbers.** `M` = incompatible change, `m` = user-visible
feature change, `d` = bug fixes and changes with no user-visible
effect. The number is held in [sources/version.h](sources/version.h) as
`BOCL_VERSION_MAJOR.BOCL_VERSION_MINOR.BOCL_VERSION_PATCH`, and the
banner, `--version` and `(LISP-IMPLEMENTATION-VERSION)` all read from
there.

bocl bumps `BOCL_VERSION_PATCH` by one on **every** commit touching
`sources/`, `tests/`, `bootstrap/` or documents (PLAN.md § Version
policy), so `d` advances much faster than releases are cut: `d` is
monotonic within a series but **not** contiguous across releases —
`release-1.9.1` may be followed by `release-1.9.7`. That is explicitly
allowed (version-rules.md R1).

**Before tagging** — the release snapshot must be self-describing.
Refresh, in the same commit the tag will point at:

1. [CL-STATUS.md](CL-STATUS.md) — re-survey the CL-symbol coverage
   table, fold in everything landed since the previous snapshot,
   re-cross-reference "still open" against [issues/open/](issues/open/).
2. [TODO.md](TODO.md) — consolidate "Landed since" for the released
   band, regroup "Still Remaining" against
   [issues/open/index.txt](issues/open/index.txt), move verified
   closures to "Completed items".
3. [README.org](README.org) — "Current status" (version, coverage,
   test/CI state) and "Future plans".

**Cutting a release** (full procedure in `version-rules.md` § 5):

1. refresh the three documents above, bump `sources/version.h`, commit;
2. `git tag -a release-M.m.d`;
3. fast-forward `version-M.m` and `version-M` onto that commit
   (creating `version-M.m` when `d = 0`);
4. push the tag and both pointers to `origin`; the CI `collect` job
   uploads the tarballs and cuts the GitLab Release from the tag;
5. post-release bump of `BOCL_VERSION_PATCH` on master, so no untagged
   commit claims a released version.

## Build, install and release

The Makefile follows the shared
[`build-rules.md`](https://gitlab.com/informatimago/rules/-/blob/master/build-rules.md):
three phases — `programs` (bin/bocl plus the `libexec/bocl/` system Lisp
files and modules it boots from), `libraries` (`libbocl.{a,dylib,so}` +
soname links, `include/bocl/`, `bocl.pc`), `documentation` (man page,
Info manual, org/pdf) — crossed with `build-`, `stage-`, `install-`.

`make install` **never compiles**: it copies the staged trees. `sudo
make install` re-runs the staging half as `$SUDO_USER`, so no compiler
ever runs as root — this repository had nine root-owned files
(`sources/bocl.pc`, the bootstrap `.cfasl`s) from before that split.

**`$PREFIX` is compiled in**, unlike most projects: `BOCL_PRELUDE_PATH`
in the executable, `-install_name` in the shared library, `prefix=` in
`bocl.pc`. A staged tree is therefore bound to the PREFIX it was built
for and holds the full prefix path
(`stage/programs$PREFIX/bin/bocl`); `DESTDIR` only moves where it is
written. **Changing `PREFIX` means re-staging** — `make clean-stage`
first.

`sources/Makefile` owns the FHS layout and the three `install-<phase>`
targets, which are pure copies; the root Makefile owns the build/stage
half. `make dist` and `make check-lib` keep their own DESTDIR staging
(they package and link-test a `/usr`-rooted tree) and are unaffected.

Every staged tree carries `share/doc/bocl/manifest-<phase>.txt` naming
the commit it was built from (`scripts/make-manifest.sh`, vendored from
the rules repository; `scripts/manifest-versions.sh` is the local hook).

**Verification.** `make check-versions` audits the repository against
the invariants; run `git fetch --prune --tags` first. It is expected to
pass on master at all times. `scripts/check-versions.sh` is a vendored
copy of the one in the rules repository — fix it there, then re-vendor.

**Legacy refs.** Releases up to 1.9.1 were originally marked `vM.m.d`
with `release-M.m.d` *branches*; the branches are gone, replaced by the
tags and pointers above. The old `vM.m.d` tags are kept as immutable
aliases of the corresponding `release-M.m.d` tags: that format is
**deprecated**, not deleted — use `release-M.m.d` for every new release.
