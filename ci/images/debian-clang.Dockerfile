# Toolchain image: Debian (glibc) + clang/LLVM. Same caching contract as the
# gcc image. clang exercises the WARN_CLANG warning set and a different codegen
# path than gcc — cheap, high-value divergence on the same libc.
FROM debian:bookworm-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        clang \
        lld \
        binutils \
        make \
        libc6-dev \
        libcunit1 libcunit1-dev \
        ccache \
        zstd \
        xz-utils \
        ca-certificates \
 && rm -rf /var/lib/apt/lists/*

ENV CC=clang
