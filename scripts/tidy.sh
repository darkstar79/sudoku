#!/bin/bash

# clang-tidy analysis script for sudoku-cpp project
# Provides comprehensive static analysis with multiple output formats

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
TIDY_CONFIG="$PROJECT_ROOT/.clang-tidy"

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

print_usage() {
    echo "Usage: $0 [COMMAND] [OPTIONS]"
    echo ""
    echo "Commands:"
    echo "  check      Run clang-tidy analysis (default)"
    echo "  fix        Run clang-tidy with automatic fixes"
    echo "  build      Build project and run clang-tidy analysis"
    echo "  report     Generate detailed report with statistics"
    echo "  clean      Clean previous tidy outputs"
    echo "  help       Show this help message"
    echo ""
    echo "Options:"
    echo "  --verbose  Show detailed output"
    echo "  --quiet    Suppress non-error output"
    echo "  --config   Use custom clang-tidy config file"
    echo ""
    echo "Examples:"
    echo "  $0 check          # Run basic analysis"
    echo "  $0 fix            # Apply automatic fixes"
    echo "  $0 build --verbose # Build and analyze with detailed output"
    echo "  $0 report         # Generate comprehensive report"
}

log_info() {
    if [[ "${QUIET:-0}" != "1" ]]; then
        echo -e "${BLUE}[INFO]${NC} $*"
    fi
}

log_success() {
    if [[ "${QUIET:-0}" != "1" ]]; then
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
    if ! command -v clang-tidy &> /dev/null; then
        log_error "clang-tidy is not installed or not in PATH"
        log_info "Install with: sudo apt-get install clang-tidy (Ubuntu/Debian)"
        log_info "Or: sudo dnf install clang-tools-extra (Fedora)"
        exit 1
    fi
    
    if ! command -v cmake &> /dev/null; then
        log_error "cmake is not installed or not in PATH"
        exit 1
    fi
    
    CLANG_TIDY_VERSION=$(clang-tidy --version | head -n1)
    log_info "Using $CLANG_TIDY_VERSION"
}

ensure_build_exists() {
    if [[ ! -d "$BUILD_DIR" ]]; then
        log_info "Build directory not found, creating..."
        cd "$PROJECT_ROOT"
        conan install . --build=missing -s build_type=Release
        cmake --preset release
        cmake --build --preset release
    fi
}

get_source_files() {
    # clang-tidy < 19 falls back to C++14 default for headers not in
    # compile_commands.json, breaking std::expected. Only include headers
    # on LLVM 19+ where header lookup is smarter.
    local llvm_major
    llvm_major=$(clang-tidy --version 2>/dev/null | grep -oP 'version \K[0-9]+' | head -1)
    if [[ "${llvm_major:-0}" -ge 19 ]]; then
        find "$PROJECT_ROOT/src" \( -name "*.cpp" -o -name "*.h" \) | sort
    else
        find "$PROJECT_ROOT/src" -name "*.cpp" | sort
    fi
}

run_clang_tidy_check() {
    local source_files
    source_files=$(get_source_files)
    local file_count
    file_count=$(echo "$source_files" | wc -l)
    local jobs
    jobs=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

    log_info "Running clang-tidy analysis on $file_count files ($jobs parallel jobs)..."

    local llvm_major
    llvm_major=$(clang-tidy --version 2>/dev/null | grep -oP 'version \K[0-9]+' | head -1)

    # Prefer run-clang-tidy (LLVM's parallel runner) when available.
    # It uses Python multiprocessing for optimal work distribution across cores,
    # avoiding the load-balancing issues of fixed-batch xargs.
    local run_clang_tidy=""
    for candidate in "run-clang-tidy-${llvm_major:-}" run-clang-tidy; do
        if command -v "$candidate" &>/dev/null; then
            run_clang_tidy="$candidate"
            break
        fi
    done

    # Collect extra compiler args (values only); formatted per runner below.
    local extra_compiler_args=()

    # On LLVM < 19, Clang may compile with a lower C++ standard than what
    # compile_commands.json specifies,
    # causing std::expected's #if __cplusplus > 202002L guard to fail.
    # Force C++23 explicitly and inject GCC's system include paths so
    # <expected> and other C++23 headers are found and enabled.
    if [[ "${llvm_major:-0}" -lt 19 ]]; then
        extra_compiler_args+=("-std=c++23")
        while IFS= read -r inc_dir; do
            [[ -d "$inc_dir" ]] && extra_compiler_args+=("-isystem$inc_dir")
        done < <(g++ -v -x c++ /dev/null -o /dev/null 2>&1 | \
            awk '/#include <...> search starts here:/{f=1;next} /End of search list/{f=0} f{gsub(/^ +/,""); print}')
    fi

    local tidy_args=()
    if [[ -n "$run_clang_tidy" ]]; then
        log_info "Using $run_clang_tidy for parallel analysis"
        # run-clang-tidy uses Python argparse — single-dash, space-separated args
        tidy_args=(
            "-p" "$BUILD_DIR"
            "-config-file" "$TIDY_CONFIG"
            "-warnings-as-errors" "*"
            "-j" "$jobs"
        )
        for arg in "${extra_compiler_args[@]}"; do
            tidy_args+=("-extra-arg" "$arg")
        done
        if [[ "${VERBOSE:-0}" == "1" ]]; then
            tidy_args+=("-explain-config")
        fi
        # run-clang-tidy uses Python multiprocessing for optimal work distribution
        local exit_code=0
        # shellcheck disable=SC2086
        if ! $run_clang_tidy "${tidy_args[@]}" $source_files; then
            exit_code=1
        fi
    else
        log_info "Falling back to xargs-based parallel analysis"
        # clang-tidy uses double-dash, = joined args
        tidy_args=(
            "-p=$BUILD_DIR"
            "--config-file=$TIDY_CONFIG"
            "--warnings-as-errors=*"
        )
        for arg in "${extra_compiler_args[@]}"; do
            tidy_args+=("--extra-arg=$arg")
        done
        if [[ "${VERBOSE:-0}" == "1" ]]; then
            tidy_args+=("--explain-config")
        fi
        # Use -n 1 so each core always works on a fresh file (no batch stalls)
        local exit_code=0
        if ! echo "$source_files" | xargs -P "$jobs" -n 1 clang-tidy "${tidy_args[@]}"; then
            exit_code=1
        fi
    fi

    if [[ $exit_code -eq 0 ]]; then
        log_success "clang-tidy analysis completed successfully"
    else
        log_warning "clang-tidy found issues - see output above"
    fi

    return $exit_code
}

run_clang_tidy_fix() {
    local source_files
    source_files=$(get_source_files)
    local file_count
    file_count=$(echo "$source_files" | wc -l)

    log_warning "Running clang-tidy with automatic fixes on $file_count files..."
    log_warning "This will modify your source files. Make sure you have committed your changes!"

    read -p "Continue? (y/N): " -r
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_info "Aborted by user"
        return 1
    fi

    local tidy_args=(
        "-p=$BUILD_DIR"
        "--config-file=$TIDY_CONFIG"
        "--fix"
        "--fix-errors"
    )

    # Run sequentially — concurrent --fix can corrupt files
    local exit_code=0
    while IFS= read -r file; do
        if [[ "${VERBOSE:-0}" == "1" ]]; then
            log_info "Fixing: $(basename "$file")"
        fi

        if ! clang-tidy "${tidy_args[@]}" "$file"; then
            exit_code=1
        fi
    done <<< "$source_files"

    if [[ $exit_code -eq 0 ]]; then
        log_success "clang-tidy fixes applied successfully"
    else
        log_warning "clang-tidy fix completed with warnings"
    fi

    return $exit_code
}

generate_report() {
    local report_file="$BUILD_DIR/clang-tidy-report.txt"
    local source_files
    source_files=$(get_source_files)
    
    log_info "Generating comprehensive clang-tidy report..."
    
    {
        echo "========================================"
        echo "clang-tidy Analysis Report"
        echo "========================================"
        echo "Generated: $(date)"
        echo "Project: sudoku-cpp"
        echo "Config: $TIDY_CONFIG"
        echo ""
        
        echo "Source Files Analyzed:"
        echo "$source_files" | while read -r file; do
            echo "  - $file"
        done
        echo ""
        
        echo "Analysis Results:"
        echo "========================================"
        
        clang-tidy \
            -p="$BUILD_DIR" \
            --config-file="$TIDY_CONFIG" \
            --export-fixes="$BUILD_DIR/clang-tidy-fixes.yaml" \
            $(echo "$source_files" | tr '\n' ' ') 2>&1 || true
            
    } > "$report_file"
    
    log_success "Report generated: $report_file"
    
    if [[ -f "$BUILD_DIR/clang-tidy-fixes.yaml" ]]; then
        local fix_count
        fix_count=$(grep -c "Replacements:" "$BUILD_DIR/clang-tidy-fixes.yaml" 2>/dev/null || echo "0")
        log_info "Found $fix_count potential fixes (see clang-tidy-fixes.yaml)"
    fi
    
    # Show summary
    if [[ "${QUIET:-0}" != "1" ]]; then
        echo ""
        echo -e "${PURPLE}Report Summary:${NC}"
        tail -20 "$report_file"
    fi
}

clean_tidy_outputs() {
    log_info "Cleaning clang-tidy output files..."
    
    rm -f "$BUILD_DIR/clang-tidy-report.txt"
    rm -f "$BUILD_DIR/clang-tidy-fixes.yaml"
    
    log_success "Cleanup completed"
}

main() {
    local command="check"
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            check|fix|build|report|clean|help)
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
            --config)
                TIDY_CONFIG="$2"
                shift 2
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
    
    case "$command" in
        check)
            ensure_build_exists
            run_clang_tidy_check
            ;;
        fix)
            ensure_build_exists
            run_clang_tidy_fix
            ;;
        build)
            log_info "Building project with clang-tidy integration..."
            cd "$PROJECT_ROOT"
            conan install . --build=missing -s build_type=Release
            cmake --preset release
            cmake --build --preset release
            run_clang_tidy_check
            ;;
        report)
            ensure_build_exists
            generate_report
            ;;
        clean)
            clean_tidy_outputs
            ;;
        *)
            log_error "Unknown command: $command"
            print_usage
            exit 1
            ;;
    esac
}

main "$@"