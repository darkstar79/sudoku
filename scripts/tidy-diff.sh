#!/bin/bash

# tidy-diff.sh: Fast clang-tidy on changed lines only
#
# Uses clang-tidy-diff.py to run tidy on changed translation units,
# reporting warnings only on the lines you changed — ignores pre-existing
# issues in untouched code.
#
# Usage:
#   ./scripts/tidy-diff.sh              # staged changes (pre-commit mode)
#   ./scripts/tidy-diff.sh main         # diff vs main (PR / branch review)
#   ./scripts/tidy-diff.sh HEAD~3       # last 3 commits
#
# Reliability note:
#   Changes to .cpp files are fully covered. Changes to .h files are covered
#   only if the header appears in compile_commands.json (rare). For header-only
#   changes, pair with TIDY=1 git commit or run ./scripts/tidy.sh check.
#
# Exit code: 0 = clean, 1 = warnings/errors found or tool missing

set -eo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

REPO_ROOT=$(git rev-parse --show-toplevel)
cd "$REPO_ROOT"

BASE="${1:-}"
BUILD_DIR="build/Release"

# Locate clang-tidy-diff.py (installed with LLVM/clang-tidy package)
CLANG_TIDY_DIFF=$(find /usr -name "clang-tidy-diff.py" 2>/dev/null | head -1 || true)
if [ -z "$CLANG_TIDY_DIFF" ]; then
    echo -e "${RED}clang-tidy-diff.py not found.${NC}"
    echo "Install with: sudo dnf install clang-tools-extra  (Fedora)"
    echo "           or: sudo apt install clang-tidy          (Debian/Ubuntu)"
    exit 1
fi

# --- clang-tidy version parity with CI -------------------------------------
# CI pins clang-tidy-19 (see the clang-tidy-diff job in .github/workflows/ci.yml).
# A different local version diagnoses differently -- checks are added, removed, and
# renamed across LLVM releases -- which is the exact "passes locally, fails CI" trap
# (e.g. designated-initializers / move-const-arg / cognitive-complexity). Resolve the
# pinned version when it is installed and warn loudly on a mismatch so the divergence
# is visible before the push, not after CI. Override with CLANG_TIDY_BIN=/path or
# CLANG_TIDY_VERSION=NN.
CI_TIDY_VERSION="${CLANG_TIDY_VERSION:-19}"
if [ -n "${CLANG_TIDY_BIN:-}" ]; then
    TIDY_BIN="$CLANG_TIDY_BIN"
elif command -v "clang-tidy-${CI_TIDY_VERSION}" >/dev/null 2>&1; then
    TIDY_BIN="clang-tidy-${CI_TIDY_VERSION}"
else
    TIDY_BIN="clang-tidy"
fi

if ! command -v "$TIDY_BIN" >/dev/null 2>&1 && [ ! -x "$TIDY_BIN" ]; then
    echo -e "${RED}clang-tidy binary '$TIDY_BIN' not found.${NC}"
    echo "Install clang-tidy-${CI_TIDY_VERSION} for exact CI parity, or set CLANG_TIDY_BIN."
    exit 1
fi

LOCAL_TIDY_VERSION=$("$TIDY_BIN" --version 2>/dev/null | grep -oE 'version [0-9]+' | grep -oE '[0-9]+' | head -1 || true)
if [ -n "$LOCAL_TIDY_VERSION" ] && [ "$LOCAL_TIDY_VERSION" != "$CI_TIDY_VERSION" ]; then
    echo -e "${YELLOW}[tidy-diff] WARNING: local clang-tidy is version ${LOCAL_TIDY_VERSION}, CI pins ${CI_TIDY_VERSION}.${NC}"
    echo -e "${YELLOW}           Diagnostics may differ from CI. Install clang-tidy-${CI_TIDY_VERSION} for exact parity.${NC}"
fi

if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Build directory '$BUILD_DIR' not found.${NC}"
    echo "Run: cmake --preset release && cmake --build --preset release"
    exit 1
fi

# Get the diff (-U0 = no context lines, required for accurate line mapping)
if [ -z "$BASE" ]; then
    DIFF=$(git diff --cached -U0 -- '*.cpp' '*.h' '*.hpp' || true)
    MODE="staged changes"
else
    DIFF=$(git diff "${BASE}...HEAD" -U0 -- '*.cpp' '*.h' '*.hpp' || true)
    MODE="changes vs $BASE"
fi

if [ -z "$DIFF" ]; then
    echo -e "${GREEN}No C++ changes to check.${NC}"
    exit 0
fi

CHANGED=$(echo "$DIFF" | grep '^+++ b/' | sed 's|^+++ b/||' | grep -E '\.(cpp|h|hpp)$' | wc -l)
echo -e "${YELLOW}[tidy-diff] Checking $CHANGED file(s) — $MODE...${NC}"

OUTPUT=$(echo "$DIFF" | python3 "$CLANG_TIDY_DIFF" \
    -clang-tidy-binary "$TIDY_BIN" \
    -p1 \
    -path "$BUILD_DIR" \
    -quiet \
    -j "$(nproc)" 2>&1 || true)

if [ -n "$OUTPUT" ]; then
    echo "$OUTPUT"
    if echo "$OUTPUT" | grep -q "warning:\|error:"; then
        echo -e "${RED}[tidy-diff] Warnings found in changed lines.${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}[tidy-diff] OK${NC}"
exit 0
