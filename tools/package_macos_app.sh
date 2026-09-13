#!/usr/bin/env bash
# package_macos_app.sh - Bundle and zip the macOS .app for distribution
#
# Usage: ./tools/package_macos_app.sh <build_dir> <version>
#
#   build_dir  path to cmake build output (e.g. build/release)
#   version    version string for the filename (e.g. 0.0.2)
#
# Requires: dylibbundler
#
# Run from project root. The .app bundle must already be built.

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <build_dir> <version>"
    exit 1
fi

BUILD_DIR="$1"
VERSION="$2"

APP="$BUILD_DIR/Pizza Legacy.app"
FRAMEWORKS="$APP/Contents/Frameworks"
BIN="$APP/Contents/MacOS/Pizza Legacy"

if [[ ! -d "$APP" ]]; then
    echo "ERROR: .app bundle not found at $APP"
    echo "  Build first: cmake --preset release && cmake --build --preset release -j"
    exit 1
fi

RESOURCES="$APP/Contents/Resources"

mkdir -p "$FRAMEWORKS"

# Build extra -s search path args from pkg-config so dylibbundler can resolve
# @rpath references
DYLIBBUNDLER_SEARCH=()
for lib in libadlmidi sdl3; do
    dir=$(pkg-config --variable=libdir "$lib" 2>/dev/null || true)
    if [[ -n "$dir" ]]; then
        DYLIBBUNDLER_SEARCH+=(-s "$dir")
    fi
done

dylibbundler -od -b -x "$BIN" -d "$FRAMEWORKS" -p @executable_path/../Frameworks/ \
    "${DYLIBBUNDLER_SEARCH[@]}"

# dylibbundler rewrites every rpath to the same path, and dyld refuses a binary
# that lists an rpath twice.
rpath_count() {
    otool -l "$BIN" | grep -A2 LC_RPATH | grep -c "path @executable_path/../Frameworks/ " || true
}
while (( $(rpath_count) > 1 )); do
    install_name_tool -delete_rpath @executable_path/../Frameworks/ "$BIN" 2>/dev/null
done
codesign --force --sign - "$BIN"

cp -r "LICENSES" "$RESOURCES/LICENSES"
cp "LICENSES/GPL-3.0-or-later.txt" "$RESOURCES/LICENSE.txt"

# Strip quarantine
xattr -cr "$APP"

# Label artifact from binary architecture
read -ra ARCHS <<< "$(lipo -archs "$BIN")"
if (( ${#ARCHS[@]} > 1 )); then
    ARCH_LABEL="universal"
else
    ARCH_LABEL="${ARCHS[0]}"
fi

DIST_DIR="PizzaLegacy-${VERSION}-macOS-${ARCH_LABEL}"

mkdir -p "${DIST_DIR}"
cp -r "$APP" "$DIST_DIR/"
if [[ -f "$BUILD_DIR/dist/README-macOS.txt" ]]; then
    cp "$BUILD_DIR/dist/README-macOS.txt" "$DIST_DIR/README.txt"
fi
if [[ -f "$BUILD_DIR/dist/THIRD_PARTIES.txt" ]]; then
    cp "$BUILD_DIR/dist/THIRD_PARTIES.txt" "$DIST_DIR/THIRD_PARTIES.txt"
fi

# Package
zip -r "${DIST_DIR}.zip" "$DIST_DIR"
rm -rf "${DIST_DIR}"

echo "Done: ${DIST_DIR}.zip"
