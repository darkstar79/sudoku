#!/bin/bash
# Include-what-you-use analysis script for sudoku project
# Usage: ./scripts/iwyu.sh [--fix] [--help]

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="${PROJECT_ROOT}/build/iwyu-report"

# Reuse same nested-Conan build dir detection as tidy.sh
find_build_dir() {
    if [[ -n "${BUILD_DIR:-}" ]]; then
        echo "$BUILD_DIR"
        return
    fi
    for base in Release RelWithDebInfo Debug; do
        for candidate in \
            "$PROJECT_ROOT/build/$base/build/$base" \
            "$PROJECT_ROOT/build/$base"; do
            if [[ -f "$candidate/compile_commands.json" ]]; then
                echo "$candidate"
                return
            fi
        done
    done
    echo ""
}

FIX_MODE=false

print_usage() {
    echo "Usage: $0 [--fix] [--help]"
    echo ""
    echo "Options:"
    echo "  --fix    Apply IWYU suggestions automatically via fix_includes.py"
    echo "  --help   Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0           # Report include issues (check mode)"
    echo "  $0 --fix     # Apply fixes automatically"
    echo ""
    echo "Prerequisites:"
    echo "  sudo apt-get install iwyu"
    echo "  A compiled build must exist (run cmake --build build/Release first)"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --fix)   FIX_MODE=true; shift ;;
        --help)  print_usage; exit 0 ;;
        *)       echo -e "${RED}Unknown option: $1${NC}"; print_usage; exit 1 ;;
    esac
done

# Check iwyu_tool.py is available
if ! command -v iwyu_tool.py &>/dev/null; then
    echo -e "${RED}Error: iwyu_tool.py not found.${NC}"
    echo "Install with:  sudo apt-get install iwyu"
    exit 1
fi

# Find compile_commands.json
BUILD_DIR="$(find_build_dir)"
if [[ -z "$BUILD_DIR" || ! -f "$BUILD_DIR/compile_commands.json" ]]; then
    echo -e "${RED}Error: compile_commands.json not found.${NC}"
    echo "Build the project first:  cmake --build build/Release"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

echo -e "${BLUE}Running IWYU analysis...${NC}"
echo "  Build dir: $BUILD_DIR"
echo "  Output:    $OUTPUT_DIR/iwyu.log"
echo ""

# Run IWYU on src/ files only, suppressing Qt/stdlib noise
# -Xiwyu --no_fwd_decls: don't suggest forward declarations (too noisy)
iwyu_tool.py -p "$BUILD_DIR" \
    $(find "$PROJECT_ROOT/src" -name "*.cpp" | sort) \
    -- -Xiwyu --no_fwd_decls 2>&1 | tee "$OUTPUT_DIR/iwyu.log"

IWYU_EXIT=${PIPESTATUS[0]}

echo ""

if [[ "$FIX_MODE" == "true" ]]; then
    if ! command -v fix_includes.py &>/dev/null; then
        echo -e "${RED}Error: fix_includes.py not found (should ship with iwyu).${NC}"
        exit 1
    fi
    echo -e "${BLUE}Applying fixes...${NC}"
    fix_includes.py --nosafe_headers < "$OUTPUT_DIR/iwyu.log"
    echo -e "${GREEN}Fixes applied. Review changes with: git diff${NC}"
else
    if [[ $IWYU_EXIT -eq 0 ]]; then
        echo -e "${GREEN}IWYU analysis complete. Report: $OUTPUT_DIR/iwyu.log${NC}"
    else
        echo -e "${YELLOW}IWYU analysis complete with suggestions. Report: $OUTPUT_DIR/iwyu.log${NC}"
        echo "Run with --fix to apply automatically, or review $OUTPUT_DIR/iwyu.log manually."
    fi
fi
