# Reproducible build environment for the Python dashboard and the bare-metal firmware.
# Used by the dev container, by CI and by `docker build` on a developer machine, so
# "works on my machine" and "works in CI" are the same statement.
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG ARM_GNU_VERSION=14.2.rel1

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates curl xz-utils git make ninja-build cmake \
      gcc-14 g++-14 clang-format-18 clang-tidy-18 cppcheck \
      qemu-system-arm \
      python3 python3-pip python3-venv python3-numpy \
 && rm -rf /var/lib/apt/lists/* \
 && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 140 \
 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 140 \
 && update-alternatives --install /usr/bin/cc  cc  /usr/bin/gcc-14 140 \
 && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-14 140

# Official Arm GNU Toolchain (bare-metal, newlib): AArch64 for the Zynq UltraScale+ APU
# (Cortex-A53) and AArch32 for the RPU (Cortex-R5F) and Cortex-M microcontrollers.
RUN set -eux; \
    base="https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_GNU_VERSION}/binrel"; \
    for triple in aarch64-none-elf arm-none-eabi; do \
      curl -fsSL "${base}/arm-gnu-toolchain-${ARM_GNU_VERSION}-x86_64-${triple}.tar.xz" \
        | tar -xJ -C /opt; \
      ln -s "/opt/arm-gnu-toolchain-${ARM_GNU_VERSION}-x86_64-${triple}" "/opt/${triple}"; \
    done

ENV PATH="/opt/aarch64-none-elf/bin:/opt/arm-none-eabi/bin:${PATH}"

WORKDIR /workspace
