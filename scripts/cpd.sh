#!/bin/bash

# Copy-paste detection script for sudoku project
# Uses PMD CPD to find duplicated code blocks in source files

set -euo pipefail

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# PMD configuration
PMD_VERSION="${PMD_VERSION:-7.21.0}"
PMD_INSTALL_DIR="${PMD_HOME:-$HOME/.local/share/pmd}"
PMD_DIR="$PMD_INSTALL_DIR/pmd-bin-$PMD_VERSION"
PMD_BIN="$PMD_DIR/bin/pmd"
MIN_TOKENS="${CPD_MIN_TOKENS:-100}"

print_usage() {
    echo "Usage: $0 [COMMAND] [OPTIONS]"
    echo ""
    echo "Commands:"
    echo "  check      Run copy-paste detection (default)"
    echo "  install    Download and install PMD"
    echo "  help       Show this help message"
    echo ""
    echo "Options:"
    echo "  --tokens N  Minimum token count for duplicates (default: $MIN_TOKENS)"
    echo ""
    echo "Environment variables:"
    echo "  PMD_VERSION      PMD version to use (default: $PMD_VERSION)"
    echo "  PMD_HOME         PMD installation directory (default: ~/.local/share/pmd)"
    echo "  CPD_MIN_TOKENS   Minimum token count (default: 100)"
    echo ""
    echo "Examples:"
    echo "  $0                  # Run CPD with default settings"
    echo "  $0 install          # Download PMD"
    echo "  $0 check --tokens 75  # Lower duplicate threshold"
}

log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $*"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $*" >&2
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*" >&2
}

check_java() {
    if ! command -v java &> /dev/null; then
        log_error "Java runtime not found"
        log_info "Install with: sudo dnf install java-latest-openjdk-headless (Fedora)"
        log_info "         or: sudo apt-get install default-jre-headless (Ubuntu)"
        exit 1
    fi
    log_info "Using $(java -version 2>&1 | head -1)"
}

install_pmd() {
    if [[ -x "$PMD_BIN" ]]; then
        log_info "PMD $PMD_VERSION already installed at $PMD_DIR"
        return 0
    fi

    check_java

    local download_url="https://github.com/pmd/pmd/releases/download/pmd_releases%2F${PMD_VERSION}/pmd-dist-${PMD_VERSION}-bin.zip"
    local tmp_zip
    tmp_zip="$(mktemp /tmp/pmd-XXXXXX.zip)"

    log_info "Downloading PMD $PMD_VERSION..."
    if ! curl -sL "$download_url" -o "$tmp_zip"; then
        log_error "Failed to download PMD from $download_url"
        rm -f "$tmp_zip"
        exit 1
    fi

    log_info "Installing to $PMD_INSTALL_DIR..."
    mkdir -p "$PMD_INSTALL_DIR"
    unzip -q -o "$tmp_zip" -d "$PMD_INSTALL_DIR"
    rm -f "$tmp_zip"

    if [[ -x "$PMD_BIN" ]]; then
        log_success "PMD $PMD_VERSION installed successfully"
    else
        log_error "Installation failed — $PMD_BIN not found"
        exit 1
    fi
}

ensure_pmd() {
    if [[ -x "$PMD_BIN" ]]; then
        return 0
    fi

    # Check if pmd is available in PATH (e.g., CI)
    if command -v pmd &> /dev/null; then
        PMD_BIN="$(command -v pmd)"
        return 0
    fi

    log_warning "PMD not found. Installing..."
    install_pmd
}

run_cpd() {
    log_info "Running copy-paste detection (minimum $MIN_TOKENS tokens)..."
    log_info "Scanning: src/"

    local exit_code=0
    if ! "$PMD_BIN" cpd \
        --minimum-tokens "$MIN_TOKENS" \
        --language cpp \
        --dir "$PROJECT_ROOT/src" \
        --format text \
        --no-fail-on-violation; then
        exit_code=$?
    fi

    # CPD exit codes: 0 = no duplicates, 4 = duplicates found, others = error
    if [[ $exit_code -eq 0 ]]; then
        log_success "No code duplication detected"
    elif [[ $exit_code -eq 4 ]]; then
        log_warning "Code duplication found — see output above"
        return 1
    else
        log_error "CPD encountered an error (exit code $exit_code)"
        return "$exit_code"
    fi
}

main() {
    local command="check"

    while [[ $# -gt 0 ]]; do
        case $1 in
            check|install|help)
                command="$1"
                shift
                ;;
            --tokens)
                MIN_TOKENS="$2"
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

    case "$command" in
        install)
            check_java
            install_pmd
            ;;
        check)
            check_java
            ensure_pmd
            run_cpd
            ;;
    esac
}

main "$@"
