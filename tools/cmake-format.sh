#!/bin/bash -eu

SCRIPT_DIR=$(
    cd $(dirname $0)
    pwd
)
ROOT_DIR=$SCRIPT_DIR/..

FILES=""
FILES+=" ${ROOT_DIR}/CMakeLists.txt"
FILES+=" ${ROOT_DIR}/benchmark/CMakeLists.txt"
FILES+=" ${ROOT_DIR}/cmake/bosimConfig.cmake.in"
FILES+=" ${ROOT_DIR}/cmake/deps.cmake"
FILES+=" ${ROOT_DIR}/cmake/gpu.cmake"
FILES+=" ${ROOT_DIR}/cmake/utils.cmake"
FILES+=" ${ROOT_DIR}/codegen/CMakeLists.txt"
FILES+=" ${ROOT_DIR}/examples/CMakeLists.txt"
FILES+=" ${ROOT_DIR}/server/CMakeLists.txt"
FILES+=" ${ROOT_DIR}/server/src/CMakeLists.txt"
FILES+=" ${ROOT_DIR}/src/CMakeLists.txt"
FILES+=" ${ROOT_DIR}/tests/CMakeLists.txt"

cmake-format -i $FILES
cmake-lint $FILES
