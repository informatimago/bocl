#!/bin/sh
# POSIX sh — print "name version" for the program this repository ships.
# Read by scripts/make-manifest.sh (via MANIFEST_VERSIONS_CMD, whose
# default is this path) to fill the manifest's `programs:' line.
#
# The extraction lives here, not in make-manifest.sh: that script is a
# vendored copy of the one in the rules repository and must stay
# project-independent.

set -u

h=sources/version.h
[ -f "$h" ] || exit 0

n() { sed -n "s/^#define BOCL_VERSION_$1 *\([0-9][0-9]*\).*/\1/p" "$h" | head -1; }
printf 'bocl %s.%s.%s' "$(n MAJOR)" "$(n MINOR)" "$(n PATCH)"
