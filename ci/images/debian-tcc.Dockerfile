# Toolchain image: Debian (glibc) + tcc (TinyCC). The compiler-matrix stress
# target — a non-GNU C17 compiler that does NOT provide the GNU/Clang builtins,
# so it guards against bocl creeping back onto gcc/clang-only intrinsics.
# binutils provides ar/ranlib/strip (tcc bundles none of those).
# No ccache (it does not wrap tcc); tcc compiles fast anyway.
FROM debian:bookworm-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        tcc \
        binutils \
        make \
        libc6-dev \
        libcunit1 libcunit1-dev \
        ca-certificates \
 && rm -rf /var/lib/apt/lists/*

ENV CC=tcc
