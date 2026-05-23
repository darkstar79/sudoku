#!/bin/bash
# Automated code formatting script for sudoku project
# Uses clang-format to enforce consistent code style

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Find clang-format
CLANG_FORMAT=$(command -v clang-format || echo "")
if [ -z "$CLANG_FORMAT" ]; then
    echo -e "${RED}Error: clang-format not found${NC}"
    echo "Install with: sudo dnf install clang-tools-extra"
    exit 1
fi

# Check clang-format version
VERSION=$($CLANG_FORMAT --version | grep -oP '(?<=version )\d+' | head -1)
if [ "$VERSION" -lt 15 ]; then
    echo -e "${YELLOW}Warning: clang-format version $VERSION detected${NC}"
    echo -e "${YELLOW}InsertBraces feature requires version 15+${NC}"
    echo -e "${YELLOW}Install newer version: sudo dnf install clang-tools-extra${NC}"
fi

# Source files to format (exclude third-party code)
SOURCE_DIRS=(
    "${PROJECT_ROOT}/include"
    "${PROJECT_ROOT}/src"
    "${PROJECT_ROOT}/tests"
)

# Files to exclude (glob patterns)
EXCLUDE_PATTERNS=(
    "*/build/*"
    "*/.conan2/*"
)

# Function to check if file should be excluded
should_exclude() {
    local file="$1"
    for pattern in "${EXCLUDE_PATTERNS[@]}"; do
        if [[ "$file" == $pattern ]]; then
            return 0
        fi
    done
    return 1
}

# Function to format files
format_files() {
    local dry_run=$1
    local file_count=0
    local changed_count=0

    echo -e "${BLUE}Searching for C++ source files...${NC}"

    # Find all C++ files in source directories
    for dir in "${SOURCE_DIRS[@]}"; do
        if [ ! -d "$dir" ]; then
            continue
        fi

        while IFS= read -r -d '' file; do
            # Skip excluded files
            if should_exclude "$file"; then
                continue
            fi

            file_count=$((file_count + 1))

            if [ "$dry_run" = true ]; then
                # Check if file would be changed
                if ! $CLANG_FORMAT --dry-run --Werror "$file" 2>/dev/null; then
                    echo -e "  ${YELLOW}Would format:${NC} $file"
                    changed_count=$((changed_count + 1))
                fi
            else
                # Format the file
                echo -e "  ${GREEN}Formatting:${NC} $file"
                $CLANG_FORMAT -i "$file"
            fi
        done < <(find "$dir" -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print0)
    done

    echo ""
    echo -e "${BLUE}Summary:${NC}"
    echo -e "  Total files scanned: $file_count"
    if [ "$dry_run" = true ]; then
        echo -e "  Files that would be formatted: $changed_count"
        if [ $changed_count -gt 0 ]; then
            echo -e "${YELLOW}Run './scripts/format.sh' to apply formatting${NC}"
        else
            echo -e "${GREEN}All files are properly formatted!${NC}"
        fi
    else
        echo -e "  ${GREEN}Files formatted: $file_count${NC}"
    fi
}

# Function to format specific files
format_specific() {
    local files=("$@")
    local file_count=${#files[@]}

    echo -e "${BLUE}Formatting $file_count specified files...${NC}"

    for file in "${files[@]}"; do
        if [ ! -f "$file" ]; then
            echo -e "  ${RED}Not found:${NC} $file"
            continue
        fi

        if should_exclude "$file"; then
            echo -e "  ${YELLOW}Skipped (excluded):${NC} $file"
            continue
        fi

        echo -e "  ${GREEN}Formatting:${NC} $file"
        $CLANG_FORMAT -i "$file"
    done

    echo -e "${GREEN}Done!${NC}"
}

# Function to show help
show_help() {
    cat << EOF
Usage: $0 [OPTION] [FILES...]

Format C++ source code using clang-format.

Options:
  check         Check formatting without modifying files (dry-run)
  <no args>     Format all project source files
  FILES...      Format specific files

Examples:
  $0                           # Format all project files
  $0 check                     # Check which files need formatting
  $0 src/core/game_state.cpp   # Format specific file

Configuration:
  - Uses .clang-format in project root
  - Excludes build artifacts and Conan cache
  - Enforces braces around all control statements

EOF
}

# Main script logic
case "${1:-}" in
    -h|--help|help)
        show_help
        exit 0
        ;;
    check)
        format_files true
        exit 0
        ;;
    "")
        format_files false
        exit 0
        ;;
    *)
        format_specific "$@"
        exit 0
        ;;
esac
