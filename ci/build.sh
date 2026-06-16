#!/bin/sh
# ci/build.sh — compile bocl (exe + shared + static) and run the test + linkage
# gates. Environment-agnostic: every Docker/QEMU image runs this same script.
#
#   CC        compiler to use (default: image's $CC, else cc)
#   BUILD     debug | release   (default: release for CI deliverables)
#   CCACHE_DIR ccache location (set + cached by the CI job)
#
# ccache is engaged transparently by prepending /usr/lib/ccache to PATH in the
# CI job, so CC stays the plain compiler name.
set -eu

CC="${CC:-cc}"
BUILD="${BUILD:-release}"

echo "== bocl build: CC=$CC BUILD=$BUILD =="
"$CC" --version 2>/dev/null | head -1 || true

# 1. Build all deliverables.
make -C sources build CC="$CC" BUILD="$BUILD"

# 2. Unit tests (fast C tests that link the shared lib — also proves -lbocl).
make -C tests test CC="$CC" BUILD="$BUILD" || {
    echo "!! unit tests failed" >&2
    exit 1
}

# 3. Linkage proof: an independent driver against the *installed* libs,
#    linked both statically and dynamically.
make -C sources check-lib CC="$CC" BUILD="$BUILD"

# 4. ccache stats (visibility into cache effectiveness across pipelines).
command -v ccache >/dev/null 2>&1 && ccache -s || true

echo "== build OK =="
