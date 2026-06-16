# Toolchain image: Debian (glibc) + gcc. Built once, pushed to the GitLab
# Container Registry, and reused across pipelines (see .gitlab-ci.yml).
# ccache lives in /usr/lib/ccache; jobs prepend it to PATH so `gcc` is cached
# transparently while CC stays the plain compiler name (clean dist naming).
FROM debian:bookworm-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        gcc \
        binutils \
        make \
        libc6-dev \
        libcunit1 libcunit1-dev \
        libasan8 libubsan1 \
        ccache \
        zstd \
        xz-utils \
        ca-certificates \
 && rm -rf /var/lib/apt/lists/*

ENV CC=gcc
