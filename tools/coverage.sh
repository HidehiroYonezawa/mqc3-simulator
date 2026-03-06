#!/bin/bash -eu

SCRIPT_DIR=$(
    cd $(dirname $0)
    pwd
)
ROOT_DIR=$SCRIPT_DIR/..
BIN_DIR=$ROOT_DIR/externals/bin
JQ=$BIN_DIR/jq

if [ ! -d $BIN_DIR ]; then
  echo "Create directory: $BIN_DIR."
  mkdir -p $BIN_DIR
fi

if [ ! -f $JQ ]; then
  echo "Download jq at $BIN_DIR."
  curl -o $JQ -L https://github.com/stedolan/jq/releases/download/jq-1.5/jq-linux64
  chmod +x $JQ
fi

test_list=(
    # base.
    base_circuit
    base_log
    base_math
    base_operation
    base_parallel
    base_state
    circuit_teleportation
    # DO NOT ADD GPU TESTS.
    # DO NOT ADD PYTHON TESTS.
)

bin_files=""
prof_files=""
for test in "${test_list[@]}"; do
    echo $test
    ./build/tests/$test
    mv default.profraw ${test}.profdata
    bin_files+=" -object ./build/tests/$test"
    prof_files+=" $test.profdata"
done

llvm-profdata merge $prof_files -o merged.profdata
# Generate html.
llvm-cov show   --ignore-filename-regex=".*externals.*|.*mqc3_cloud.*|.*tests.*" $bin_files -instr-profile=merged.profdata -format=html -output-dir=coverage-html
# Dump total line coverage percent.
llvm-cov export --ignore-filename-regex=".*externals.*|.*mqc3_cloud.*|.*tests.*" $bin_files -instr-profile=merged.profdata | $JQ '.data[0].totals.lines.percent'
