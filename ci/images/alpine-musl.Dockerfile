# Toolchain image: Alpine (musl libc) + gcc. The musl build is the single most
# valuable cheap target — it catches glibc-isms (GNU extensions, header layout,
# locale/printf differences) that the Debian images never will.
# build-base pulls gcc, musl-dev, make, binutils. ccache wrappers live in
# /usr/lib/ccache/bin on Alpine.
FROM alpine:3.20

RUN apk add --no-cache \
        build-base \
        make \
        binutils \
        cunit-dev \
        ccache \
        zstd \
        xz \
        tar

ENV CC=gcc
