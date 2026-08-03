#!/bin/bash
# Tests for the SUDOKU_COVERAGE_CMAKE_ARGS hook in scripts/coverage.sh.
#
# The hook exists so a developer whose local toolchain needs extra configure
# flags can run ./scripts/coverage.sh without hand-priming CMAKE_CXX_FLAGS into
# the build cache first (story 8-17). The contract these tests pin down:
#
#   1. Unset  -> zero extra arguments, i.e. byte-for-byte today's behavior.
#   2. Set    -> each whitespace-separated word becomes its own CMake argument.
#   3. Quoted -> a quoted multi-word value stays ONE argument, so
#                -DCMAKE_CXX_FLAGS="-Wno-error=a -Wno-error=b" survives intact.
#
# Run: ./scripts/tests/test_coverage_env_hook.sh

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# Sourcing with "help" defines the functions and prints usage without building.
# shellcheck source=../coverage.sh
source "${PROJECT_ROOT}/scripts/coverage.sh" help > /dev/null
set +e  # coverage.sh sets -e; do not let a failing assertion abort the run

if ! declare -F get_extra_cmake_args > /dev/null; then
    echo "FAIL: scripts/coverage.sh does not define get_extra_cmake_args"
    exit 1
fi

failures=0

function expect_args() {
    local description="$1"
    local expected="$2"
    local actual="$3"

    if [ "$expected" == "$actual" ]; then
        echo "  PASS: ${description}"
    else
        echo "  FAIL: ${description}"
        echo "    expected: [${expected}]"
        echo "    actual:   [${actual}]"
        failures=$((failures + 1))
    fi
}

echo "Testing SUDOKU_COVERAGE_CMAKE_ARGS forwarding..."

# 1. Unset -> no arguments at all.
unset SUDOKU_COVERAGE_CMAKE_ARGS
mapfile -t args < <(get_extra_cmake_args)
expect_args "unset yields no extra arguments" "0" "${#args[@]}"

# 2. Empty string -> no arguments (same as unset).
SUDOKU_COVERAGE_CMAKE_ARGS="" mapfile -t args < <(get_extra_cmake_args)
expect_args "empty string yields no extra arguments" "0" "${#args[@]}"

# 3. Single argument.
export SUDOKU_COVERAGE_CMAKE_ARGS="-DSUDOKU_ENABLE_BENCHMARKS=OFF"
mapfile -t args < <(get_extra_cmake_args)
expect_args "single argument count" "1" "${#args[@]}"
expect_args "single argument value" "-DSUDOKU_ENABLE_BENCHMARKS=OFF" "${args[0]}"

# 4. Two whitespace-separated arguments.
export SUDOKU_COVERAGE_CMAKE_ARGS="-DFOO=1 -DBAR=2"
mapfile -t args < <(get_extra_cmake_args)
expect_args "two arguments count" "2" "${#args[@]}"
expect_args "second argument value" "-DBAR=2" "${args[1]}"

# 5. A quoted multi-word value stays one argument. This is the case that
#    matters: -DCMAKE_CXX_FLAGS carrying more than one flag.
export SUDOKU_COVERAGE_CMAKE_ARGS='-DCMAKE_CXX_FLAGS="-Wno-error=array-bounds -Wno-error=maybe-uninitialized"'
mapfile -t args < <(get_extra_cmake_args)
expect_args "quoted multi-word value stays one argument" "1" "${#args[@]}"
expect_args "quoted multi-word value is unwrapped" \
    "-DCMAKE_CXX_FLAGS=-Wno-error=array-bounds -Wno-error=maybe-uninitialized" "${args[0]}"

unset SUDOKU_COVERAGE_CMAKE_ARGS

if [ "$failures" -eq 0 ]; then
    echo "All SUDOKU_COVERAGE_CMAKE_ARGS tests passed"
    exit 0
fi

echo "${failures} SUDOKU_COVERAGE_CMAKE_ARGS test(s) failed"
exit 1
