FROM nvidia/cuda:12.6.3-devel-ubuntu22.04

ARG CLANG_VERSION=17

# Tools for installing LLVM.
RUN apt-get update \
    && apt-get install -y lsb-release software-properties-common wget tar

# Install LLVM.
RUN wget -O - https://apt.llvm.org/llvm.sh | bash -s -- ${CLANG_VERSION} all
RUN update-alternatives --verbose \
    --install /usr/bin/clang          clang          /usr/lib/llvm-${CLANG_VERSION}/bin/clang 100 \
    --slave   /usr/bin/clang++        clang++        /usr/lib/llvm-${CLANG_VERSION}/bin/clang++ \
    --slave   /usr/bin/clang-tidy     clang-tidy     /usr/lib/llvm-${CLANG_VERSION}/bin/clang-tidy \
    --slave   /usr/bin/llvm-profdata  llvm-profdata  /usr/lib/llvm-${CLANG_VERSION}/bin/llvm-profdata \
    --slave   /usr/bin/llvm-cov       llvm-cov       /usr/lib/llvm-${CLANG_VERSION}/bin/llvm-cov \
    --slave   /usr/bin/asan_symbolize asan_symbolize /usr/bin/asan_symbolize-${CLANG_VERSION} \
    --slave   /usr/bin/ld.lld         ld.lld         /usr/lib/llvm-${CLANG_VERSION}/bin/lld \
    --slave   /usr/bin/lld            lld            /usr/lib/llvm-${CLANG_VERSION}/bin/lld \
    --slave   /usr/bin/lld-link       lld-link       /usr/lib/llvm-${CLANG_VERSION}/bin/lld \
    --slave   /usr/bin/wasm-ld        wasm-ld        /usr/lib/llvm-${CLANG_VERSION}/bin/lld
ENV CC clang
ENV CXX clang++

# Tools for C++ development.
RUN apt-get update \
    && apt-get install -y git ninja-build

# CMake
RUN wget https://github.com/Kitware/CMake/releases/download/v3.31.7/cmake-3.31.7-linux-x86_64.tar.gz \
    && tar -xzvf cmake-3.31.7-linux-x86_64.tar.gz \
    && mkdir -p /opt/cmake-3.31 \
    && mv cmake-3.31.7-linux-x86_64/* /opt/cmake-3.31 \
    && ln -s /opt/cmake-3.31/bin/cmake /usr/local/bin/cmake \
    && ln -s /opt/cmake-3.31/bin/ccmake /usr/local/bin/ccmake \
    && ln -s /opt/cmake-3.31/bin/cpack /usr/local/bin/cpack \
    && ln -s /opt/cmake-3.31/bin/ctest /usr/local/bin/ctest \
    && rm cmake-3.31.7-linux-x86_64.tar.gz

# Python.
RUN apt-get update && apt-get install -y python3-pip python3-dev
RUN pip3 install pipenv cpplint lizard jinja2

# Tools for vcpkg.
RUN apt-get update \
    && apt-get install -y curl zip unzip tar pkg-config autoconf-archive

# Install vcpkg.
RUN cd / && git clone https://github.com/microsoft/vcpkg.git \
    && cd /vcpkg && ./bootstrap-vcpkg.sh

# Install libraries using vcpkg in manifest mode.
COPY vcpkg.json /vcpkg
RUN cd /vcpkg && ./vcpkg install

# Cleanup.
RUN apt-get autoremove -y && apt-get clean && rm -rf /var/lib/apt/lists/*
