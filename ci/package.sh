#!/bin/sh
# ci/package.sh — produce the relocatable binary distribution tarball for the
# current environment and stage it under dist/ for the collect job to gather.
#
#   CC        compiler used to build (default: image's $CC, else cc)
#   COMPRESS  gzip | zstd | xz   (default: zstd for releases)
#
# DIST_CC is forced to the plain compiler basename so the ccache PATH shim never
# leaks into the artifact name (bocl-<ver>-<os>-<arch>-<libc>-<cc>.tar.zst).
set -eu

CC="${CC:-cc}"
COMPRESS="${COMPRESS:-zstd}"
MAKE="${MAKE:-make}"
DIST_CC="$(basename "$CC")"

echo "== bocl package: CC=$CC COMPRESS=$COMPRESS MAKE=$MAKE =="

"$MAKE" -C sources dist CC="$CC" BUILD=release COMPRESS="$COMPRESS" DIST_CC="$DIST_CC"

mkdir -p dist
# Matches both the tarball and its .sha256 sidecar.
mv sources/bocl-*.tar.* dist/

echo "== packaged: =="
ls -l dist/
