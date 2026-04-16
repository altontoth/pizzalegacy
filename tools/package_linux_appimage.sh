#!/usr/bin/env bash
# package_linux_appimage.sh - Build AppImage for Linux
#
# Usage: ./tools/package_linux_appimage.sh <build_dir> <version>
#
#   build_dir  path to cmake build output (e.g. build/release)
#   version    version string for the filename (e.g. 0.0.2)
#
# Requires: linuxdeploy-x86_64.AppImage (downloaded automatically if missing)
#
# Run from project root. The binary must already be built.

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <build_dir> <version>"
    exit 1
fi

BUILD_DIR="$1"
VERSION="$2"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

BINARY="$BUILD_DIR/plegacy"
if [[ ! -f "$BINARY" ]]; then
    echo "ERROR: binary not found at $BINARY"
    echo "  Build first: cmake --preset release && cmake --build --preset release -j"
    exit 1
fi

APPIMAGE_NAME="PizzaLegacy_v${VERSION}_linux-x86_64.AppImage"
APPDIR="AppDir"

# Download linuxdeploy if not present
LINUXDEPLOY="./linuxdeploy-x86_64.AppImage"
if [[ ! -x "$LINUXDEPLOY" ]]; then
    echo "Downloading linuxdeploy..."
    wget -q "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY"
fi

# Set up AppDir
echo "Setting up AppDir..."
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$APPDIR/usr/share/licenses/plegacy"
mkdir -p "$APPDIR/usr/share/doc/plegacy"

cp "$BINARY" "$APPDIR/usr/bin/plegacy"
cp "assets/plegacy.desktop" "$APPDIR/usr/share/applications/plegacy.desktop"

cp LICENSES/* "$APPDIR/usr/share/licenses/plegacy/"
cp "LICENSES/GPL-3.0-or-later.txt" "$APPDIR/usr/share/licenses/plegacy/LICENSE.txt"
cp "$BUILD_DIR/dist/README-Linux.txt" "$APPDIR/usr/share/doc/plegacy/README.txt"

ICON_SRC="assets/plegacy_256.png"
ICON_DEST="$APPDIR/usr/share/icons/hicolor/256x256/apps/plegacy.png"
if [[ -f "$ICON_SRC" ]]; then
    cp "$ICON_SRC" "$ICON_DEST"
else
    echo "$ICON_SRC not found!"
    exit 1
fi

# Build AppImage
echo "Running linuxdeploy..."
"$LINUXDEPLOY" --appimage-extract-and-run \
    --appdir "$APPDIR" \
    --output appimage

# Rename to versioned filename (linuxdeploy generates its own name)
GENERATED=$(ls -1t Pizza_Legacy*.AppImage plegacy*.AppImage 2>/dev/null | head -1 || true)
if [[ -n "$GENERATED" && "$GENERATED" != "$APPIMAGE_NAME" ]]; then
    mv "$GENERATED" "$APPIMAGE_NAME"
fi

echo "Done: $APPIMAGE_NAME"
