#!/bin/bash
set -euo pipefail

# Profile-Guided Optimization (PGO) build script
# Automates the 3-stage PGO pipeline:
#   1. Instrumented build (-fprofile-generate)
#   2. Training workload (profile_hard_puzzles — 50 deterministic Hard puzzles)
#   3. Optimized rebuild (-fprofile-use)
#
# Usage: ./scripts/pgo_build.sh
# Output: build/PGO/ directory with optimized binaries
#
# Requires: GCC 12+ with PGO support, Conan deps installed for Release

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PGO_DIR="$PROJECT_DIR/build/PGO"
PROFILE_DIR="$PGO_DIR/profiles"
TOOLCHAIN="$PROJECT_DIR/build/Release/generators/conan_toolchain.cmake"

echo "=== PGO Build Pipeline ==="
echo "Project: $PROJECT_DIR"
echo "PGO dir: $PGO_DIR"
echo ""

# Verify toolchain exists (requires: conan install . --build=missing -s build_type=Release)
if [ ! -f "$TOOLCHAIN" ]; then
    echo "ERROR: Conan toolchain not found at: $TOOLCHAIN"
    echo "Run: conan install . --build=missing -s build_type=Release"
    exit 1
fi

# Stage 1: Instrumented build
echo "=== Stage 1: Instrumented build (-fprofile-generate) ==="
rm -rf "$PROFILE_DIR"
mkdir -p "$PROFILE_DIR"

cmake -S "$PROJECT_DIR" -B "$PGO_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -fprofile-generate=$PROFILE_DIR" \
    -G Ninja

cmake --build "$PGO_DIR" -- -j"$(nproc)"
echo "Stage 1 complete."
echo ""

# Stage 2: Training workload
echo "=== Stage 2: Training workload ==="
"$PGO_DIR/bin/profile_hard_puzzles"
echo ""

GCDA_COUNT=$(find "$PROFILE_DIR" -name "*.gcda" 2>/dev/null | wc -l)
echo "Generated $GCDA_COUNT profile data files."
if [ "$GCDA_COUNT" -eq 0 ]; then
    echo "ERROR: No profile data generated. Training workload may have failed."
    exit 1
fi
echo "Stage 2 complete."
echo ""

# Stage 3: Optimized rebuild
echo "=== Stage 3: Optimized rebuild (-fprofile-use) ==="
cmake -S "$PROJECT_DIR" -B "$PGO_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -fprofile-use=$PROFILE_DIR -fprofile-correction -Wno-error=missing-profile" \
    -G Ninja

cmake --build "$PGO_DIR" -- -j"$(nproc)"
echo "Stage 3 complete."
echo ""

# Verify
echo "=== PGO Build Complete ==="
echo "Optimized binaries in: $PGO_DIR/bin/"
echo ""
echo "Run benchmarks with:"
echo "  $PGO_DIR/bin/benchmarks/puzzle_generation_benchmark --difficulty Hard --iterations 250"
echo ""
echo "Run tests with:"
echo "  $PGO_DIR/bin/tests/unit_tests"
echo "  $PGO_DIR/bin/tests/integration_tests"
