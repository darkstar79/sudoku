#!/bin/bash
# Coverage analysis script for sudoku project
# Usage: ./scripts/coverage.sh [clean|summary|html|xml|json|all|help] [--report-only]

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/RelWithDebInfo"

function get_cmake_build_dir() {
    echo "${BUILD_DIR}"
}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

REPORT_ONLY=false

function print_usage() {
    echo "Usage: $0 [command] [--report-only]"
    echo ""
    echo "Commands:"
    echo "  clean       Clean coverage data and rebuild in RelWithDebInfo mode"
    echo "  summary     Generate coverage summary to console (default)"
    echo "  html        Generate HTML coverage report"
    echo "  xml         Generate XML coverage report for CI/CD"
    echo "  json        Generate JSON coverage report"
    echo "  all         Generate all report formats"
    echo "  help        Show this help message"
    echo ""
    echo "Options:"
    echo "  --report-only  Skip build and test steps, only generate reports"
    echo "                 (assumes tests were already run and .gcda files exist)"
    echo ""
    echo "Examples:"
    echo "  $0 clean            # Clean rebuild and run tests"
    echo "  $0 html             # Build, test, and generate HTML report"
    echo "  $0 xml --report-only  # Generate XML report from existing coverage data"
    echo "  $0 all              # Generate all formats"
}

function ensure_debug_build() {
    echo -e "${BLUE}Ensuring debug build for coverage analysis...${NC}"

    cd "$PROJECT_ROOT"

    conan install . --build=missing -s build_type=RelWithDebInfo || {
        echo -e "${RED}Failed to install Conan dependencies${NC}"
        exit 1
    }

    cmake --preset relwithdebinfo -DSUDOKU_ENABLE_UI_TESTS=ON || {
        echo -e "${RED}Failed to configure RelWithDebInfo build${NC}"
        exit 1
    }

    cmake --build --preset relwithdebinfo || {
        echo -e "${RED}Failed to build project${NC}"
        exit 1
    }
}

function run_tests() {
    echo -e "${BLUE}Running tests to generate coverage data...${NC}"
    local cmake_build_dir
    cmake_build_dir="$(get_cmake_build_dir)"
    cd "$cmake_build_dir"

    # Run unit tests
    if [ -f "bin/tests/unit_tests" ]; then
        echo -e "${YELLOW}Running unit tests...${NC}"
        ./bin/tests/unit_tests "~[slow]~[pathological]" || {
            echo -e "${RED}Unit tests failed${NC}"
            exit 1
        }
    else
        echo -e "${RED}Unit tests executable not found in ${cmake_build_dir}${NC}"
        exit 1
    fi

    # Run integration tests
    if [ -f "bin/tests/integration_tests" ]; then
        echo -e "${YELLOW}Running integration tests...${NC}"
        ./bin/tests/integration_tests || {
            echo -e "${RED}Integration tests failed${NC}"
            exit 1
        }
    else
        echo -e "${YELLOW}Integration tests executable not found, skipping...${NC}"
    fi

    # Run UI tests (Qt6, headless)
    local ui_test_dir="bin/tests/ui"
    if [ -d "$ui_test_dir" ]; then
        echo -e "${YELLOW}Running UI tests (offscreen)...${NC}"
        for test_exe in "$ui_test_dir"/test_*; do
            if [ -x "$test_exe" ]; then
                echo -e "${YELLOW}  Running $(basename "$test_exe")...${NC}"
                QT_QPA_PLATFORM=offscreen "$test_exe" || {
                    echo -e "${RED}UI test $(basename "$test_exe") failed${NC}"
                    exit 1
                }
            fi
        done
    else
        echo -e "${YELLOW}UI tests directory not found, skipping...${NC}"
    fi
}

function clean_coverage() {
    echo -e "${BLUE}Cleaning coverage data and rebuilding...${NC}"
    
    # Remove build directory
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
    
    # Remove coverage files
    find "$PROJECT_ROOT" -name "*.gcda" -delete 2>/dev/null || true
    find "$PROJECT_ROOT" -name "*.gcno" -delete 2>/dev/null || true
    rm -f "$PROJECT_ROOT"/*.xml "$PROJECT_ROOT"/*.json "$PROJECT_ROOT"/*.html 2>/dev/null || true
    
    ensure_debug_build
    run_tests
}

function generate_coverage_summary() {
    echo -e "${BLUE}Generating coverage summary...${NC}"
    cd "$PROJECT_ROOT"
    
    gcovr --config .gcovr.cfg || {
        echo -e "${RED}Failed to generate coverage summary${NC}"
        exit 1
    }
}

function generate_coverage_html() {
    echo -e "${BLUE}Generating HTML coverage report...${NC}"
    cd "$PROJECT_ROOT"
    
    gcovr --config .gcovr.cfg --html --html-details --output coverage_report.html || {
        echo -e "${RED}Failed to generate HTML coverage report${NC}"
        exit 1
    }
    
    echo -e "${GREEN}HTML coverage report generated: coverage_report.html${NC}"
    
    # Try to open in browser if available
    if command -v xdg-open > /dev/null; then
        echo -e "${YELLOW}Opening coverage report in browser...${NC}"
        xdg-open coverage_report.html &
    fi
}

function generate_coverage_xml() {
    echo -e "${BLUE}Generating XML coverage report...${NC}"
    cd "$PROJECT_ROOT"
    
    gcovr --config .gcovr.cfg --xml --output coverage.xml || {
        echo -e "${RED}Failed to generate XML coverage report${NC}"
        exit 1
    }
    
    echo -e "${GREEN}XML coverage report generated: coverage.xml${NC}"
}

function generate_coverage_json() {
    echo -e "${BLUE}Generating JSON coverage report...${NC}"
    cd "$PROJECT_ROOT"
    
    gcovr --config .gcovr.cfg --json --output coverage.json || {
        echo -e "${RED}Failed to generate JSON coverage report${NC}"
        exit 1
    }
    
    echo -e "${GREEN}JSON coverage report generated: coverage.json${NC}"
}

function generate_all_formats() {
    echo -e "${BLUE}Generating all coverage report formats...${NC}"
    generate_coverage_summary
    generate_coverage_html
    generate_coverage_xml
    generate_coverage_json
    echo -e "${GREEN}All coverage reports generated successfully!${NC}"
}

# Parse --report-only flag from any argument position
COMMAND=""
for arg in "$@"; do
    case "$arg" in
        --report-only)
            REPORT_ONLY=true
            ;;
        *)
            COMMAND="${COMMAND:-$arg}"
            ;;
    esac
done
COMMAND="${COMMAND:-summary}"

function build_and_test() {
    if [ "$REPORT_ONLY" = true ]; then
        echo -e "${BLUE}Report-only mode: skipping build and test steps${NC}"
    else
        ensure_debug_build
        run_tests
    fi
}

# Main script logic
case "$COMMAND" in
    clean)
        clean_coverage
        generate_coverage_summary
        ;;
    summary)
        build_and_test
        generate_coverage_summary
        ;;
    html)
        build_and_test
        generate_coverage_html
        ;;
    xml)
        build_and_test
        generate_coverage_xml
        ;;
    json)
        build_and_test
        generate_coverage_json
        ;;
    all)
        build_and_test
        generate_all_formats
        ;;
    help|--help|-h)
        print_usage
        ;;
    *)
        echo -e "${RED}Unknown command: $COMMAND${NC}"
        print_usage
        exit 1
        ;;
esac