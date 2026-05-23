#!/bin/bash

# cppcheck static analysis script for sudoku project
# Complements clang-tidy with additional bug detection (buffer overflows,
# null pointer dereferences, memory leaks, and more)

set -euo pipefail

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Find build directory containing compile_commands.json.
# Conan versions differ in output structure:
#   Newer Conan (local): build/<Type>/build/<Type>/
#   Older Conan (CI):    build/<Type>/
# Accept BUILD_DIR override, otherwise auto-detect from Release or RelWithDebInfo.
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
    echo "$PROJECT_ROOT/build/Release"
}
BUILD_DIR="$(find_build_dir)"

# Verbose/quiet flags
VERBOSE=0
QUIET=0

print_usage() {
    echo "Usage: $0 [COMMAND] [OPTIONS]"
    echo ""
    echo "Commands:"
    echo "  check      Run cppcheck analysis (default)"
    echo "  report     Generate XML report for CI integration"
    echo "  help       Show this help message"
    echo ""
    echo "Options:"
    echo "  --verbose  Show detailed output (--enable=information)"
    echo "  --quiet    Suppress non-error output"
    echo ""
    echo "Environment variables:"
    echo "  BUILD_DIR  Override build directory for compile_commands.json"
    echo ""
    echo "Examples:"
    echo "  $0                  # Run cppcheck analysis"
    echo "  $0 report           # Generate XML report"
    echo "  $0 check --verbose  # Verbose analysis"
}

log_info() {
    if [[ "$QUIET" != "1" ]]; then
        echo -e "${BLUE}[INFO]${NC} $*"
    fi
}

log_success() {
    if [[ "$QUIET" != "1" ]]; then
        echo -e "${GREEN}[SUCCESS]${NC} $*"
    fi
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $*" >&2
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*" >&2
}

check_dependencies() {
    if ! command -v cppcheck &> /dev/null; then
        log_error "cppcheck is not installed or not in PATH"
        log_info "Install with: sudo dnf install cppcheck (Fedora)"
        log_info "         or: sudo apt-get install cppcheck (Ubuntu)"
        exit 1
    fi

    local version
    version=$(cppcheck --version 2>&1)
    log_info "Using $version"
}

ensure_compile_commands() {
    if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
        log_error "compile_commands.json not found in $BUILD_DIR"
        log_info "Build the project first: cmake --build build/Release"
        log_info "Or set BUILD_DIR to point to the correct build directory"
        exit 1
    fi
    log_info "Using compile database: $BUILD_DIR/compile_commands.json"
}

build_cppcheck_args() {
    local -n args_ref=$1
    local output_format="${2:-text}"

    args_ref=(
        "--project=$BUILD_DIR/compile_commands.json"
        "--std=c++23"
        "--enable=all"
        "--error-exitcode=1"
        "--file-filter=*/src/*"
        "--suppress=unmatchedSuppression"
        "--suppress=missingIncludeSystem"
        "--suppress=unusedFunction"
        "--suppress=checkersReport"
        "--inline-suppr"
        "-j" "$(nproc 2>/dev/null || echo 4)"
    )

    if [[ "$output_format" == "xml" ]]; then
        args_ref+=("--xml")
    else
        args_ref+=("--template=gcc")
    fi

    if [[ "$VERBOSE" == "1" ]]; then
        args_ref+=("--enable=information" "--verbose")
    fi
}

run_cppcheck() {
    log_info "Running cppcheck analysis..."
    log_info "Scanning: src/ (via compile_commands.json)"

    local cppcheck_args=()
    build_cppcheck_args cppcheck_args text

    local exit_code=0
    if ! cppcheck "${cppcheck_args[@]}" 2>&1; then
        exit_code=$?
    fi

    if [[ $exit_code -eq 0 ]]; then
        log_success "cppcheck analysis completed — no issues found"
    else
        log_warning "cppcheck found issues — see output above"
    fi

    return $exit_code
}

generate_report() {
    local report_file="$BUILD_DIR/cppcheck-report.xml"

    log_info "Generating cppcheck XML report..."

    local cppcheck_args=()
    build_cppcheck_args cppcheck_args xml

    # XML report: don't fail on findings, just capture them
    cppcheck "${cppcheck_args[@]}" 2> "$report_file" || true

    log_success "Report generated: $report_file"

    # Show summary
    if [[ "$QUIET" != "1" ]]; then
        local error_count
        error_count=$(grep -c '<error ' "$report_file" 2>/dev/null || echo "0")
        echo -e "${PURPLE}Report Summary:${NC}"
        echo -e "  Issues found: $error_count"
        echo -e "  Report file: $report_file"
    fi
}

main() {
    local command="check"

    while [[ $# -gt 0 ]]; do
        case $1 in
            check|report|help)
                command="$1"
                shift
                ;;
            --verbose)
                VERBOSE=1
                shift
                ;;
            --quiet)
                QUIET=1
                shift
                ;;
            *)
                log_error "Unknown argument: $1"
                print_usage
                exit 1
                ;;
        esac
    done

    if [[ "$command" == "help" ]]; then
        print_usage
        exit 0
    fi

    check_dependencies
    ensure_compile_commands

    case "$command" in
        check)
            run_cppcheck
            ;;
        report)
            generate_report
            ;;
    esac
}

main "$@"
