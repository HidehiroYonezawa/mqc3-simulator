#!/bin/bash -eu

TIMEOUT=60

if [ $# -ge 1 ]; then
    TIMEOUT=$1
fi

echo "Timeout = ${TIMEOUT}"

SCRIPT_DIR=$(
    cd $(dirname $0)
    pwd
)
ROOT_DIR=$SCRIPT_DIR/..

DIRS=(
    "server"
    "src"
)

if [ -d clang-tidy-log ]; then
    echo "Directory exists."
    exit 1
fi

mkdir -p clang-tidy-log

index=0
for dir in "${DIRS[@]}"; do
    CPP_SRC_FILES=$(find ${dir} -name "*.*" | grep -E "(\.cc$|\.cpp$|\.cu$|\.h$|\.hpp$)")
    ARR=(${CPP_SRC_FILES})
    for file in "${ARR[@]}"; do
        echo ${index}: ${file}
        timeout ${TIMEOUT} \
        clang-tidy \
            --config-file=${ROOT_DIR}/.clang-tidy \
            -p ${ROOT_DIR}/build \
            -header-filter=${ROOT_DIR} \
            --quiet \
            --use-color \
            -header-filter=${ROOT_DIR} \
            ${file} >clang-tidy-log/${index}.log 2>&1 &
        index=$(($index+1))
    done
done

wait

index=0
for dir in "${DIRS[@]}"; do
    CPP_SRC_FILES=$(find ${dir} -name "*.*" | grep -E "(\.cc$|\.cpp$|\.cu$|\.h$|\.hpp$)")
    ARR=(${CPP_SRC_FILES})
    for file in "${ARR[@]}"; do
        echo "------------------------------"
        echo "CHECK: ${file}"
        cat clang-tidy-log/${index}.log \
        | sed 's/\x1B[@A-Z\\\]^_]\|\x1B\[[0-9:;<=>?]*[-!"#$%&'"'"'()*+,.\/]*[][\\@A-Z^_`a-z{|}~]//g'
        index=$(($index+1))
    done
done

rm -rf clang-tidy-log
