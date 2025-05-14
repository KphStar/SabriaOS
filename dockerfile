FROM ubuntu:22.04

# Install build tools
RUN apt-get update && apt-get install -y \
    nasm \
    gcc \
    binutils \
    grub-pc-bin \
    xorriso \
    make \
    grub-common \
    qemu-system-x86 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work