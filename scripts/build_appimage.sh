#!/usr/bin/env bash
# Build an AppImage locally, mirroring the CI workflow.
# Usage: ./scripts/build_appimage.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

VERSION="${VERSION:-dev}"
TOOLS_DIR="$PROJECT_DIR/.appimage-tools"
APPDIR="$PROJECT_DIR/AppDir"

# --- Check prerequisites ---
if ! command -v conan &>/dev/null; then
    echo "ERROR: conan not found. Install with: pip install conan" >&2
    exit 1
fi
if ! command -v cmake &>/dev/null; then
    echo "ERROR: cmake not found." >&2
    exit 1
fi

# Check for FUSE (needed to run AppImage tools)
if ! ldconfig -p 2>/dev/null | grep -q libfuse.so.2; then
    echo "WARNING: libfuse2 not found. Install it:"
    echo "  Fedora: sudo dnf install fuse-libs"
    echo "  Ubuntu: sudo apt-get install libfuse2"
    echo "Continuing anyway (linuxdeploy may fail)..."
fi

# --- Step 1: Conan install ---
echo "=== Installing Conan dependencies ==="
conan install . --build=missing -s build_type=Release

# --- Step 2: Build ---
echo "=== Building Release ==="
source build/Release/generators/conanbuild.sh
cmake --preset release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build --preset release

# --- Step 3: Install to AppDir ---
echo "=== Installing to AppDir ==="
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install build/Release

# --- Step 4: Download linuxdeploy tools (cached) ---
echo "=== Preparing linuxdeploy ==="
mkdir -p "$TOOLS_DIR"

download_tool() {
    local name="$1" url="$2"
    if [[ ! -x "$TOOLS_DIR/$name" ]]; then
        echo "Downloading $name..."
        curl -sL -o "$TOOLS_DIR/$name" "$url"
        chmod +x "$TOOLS_DIR/$name"
    else
        echo "Using cached $name"
    fi
}

download_tool "linuxdeploy-x86_64.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
download_tool "linuxdeploy-plugin-qt-x86_64.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"

# --- Step 5: Create AppImage ---
echo "=== Creating AppImage ==="
export LDAI_OUTPUT="Sudoku-${VERSION}-x86_64.AppImage"
# Disable stripping: linuxdeploy's bundled strip is too old for newer distros
# (fails on .relr.dyn ELF sections from Fedora 43+, Ubuntu 25.04+)
export NO_STRIP=true
"$TOOLS_DIR/linuxdeploy-x86_64.AppImage" \
    --appdir "$APPDIR" \
    --plugin qt \
    --output appimage

echo ""
echo "=== Done! ==="
echo "AppImage: $PROJECT_DIR/$LDAI_OUTPUT"
echo "Run it:   ./$LDAI_OUTPUT"
