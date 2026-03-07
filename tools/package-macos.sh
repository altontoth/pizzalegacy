#!/usr/bin/env bash
set -e
cmake --preset release
cmake --build --preset release -j
VERSION="v0.0.1"

APP=build/release/Pizza\ Legacy.app
FRAMEWORKS="$APP/Contents/Frameworks"
BIN="$APP/Contents/MacOS/Pizza Legacy"

mkdir -p "$FRAMEWORKS"
dylibbundler -od -b -x "$BIN" -d "$FRAMEWORKS" -p @executable_path/../Frameworks/

# Strip quarantine
xattr -cr "$APP"

DIST_DIR="Pizza Legacy-${VERSION}-macOS-arm64"

mkdir -p "${DIST_DIR}"
cp -r "$APP" "$DIST_DIR/"
cp dist/README-macOS.txt "$DIST_DIR/README.txt"

# Package
zip -r "${DIST_DIR}.zip" "$DIST_DIR"
