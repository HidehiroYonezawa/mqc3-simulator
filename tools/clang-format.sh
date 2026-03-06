#!/bin/bash -eu

DIRS=(
    "benchmark"
    "examples"
    "server"
    "src"
    "tests"
)

for dir in "${DIRS[@]}"; do
    CPP_SRC_FILES=$(find ./${dir} -name "*.*" | grep -E "(\.cc$|\.cpp$|\.cu$|\.h$|\.hpp$)")
    clang-format \
        --style=file \
        -i \
        --verbose \
        ${CPP_SRC_FILES}
done
