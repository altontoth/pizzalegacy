#!/usr/bin/env bash
# gen_icons.sh - Generate platform icon assets from the master .icns file.
#
# Produces:
#   assets/plegacy_256.png - for Linux AppImage
#   assets/plegacy.ico     - for Windows executable (via .rc file)
#
# Requires:
#   sips        - macOS built-in; resizes .icns to PNG at each size
#   ImageMagick - ImageMagick; assembles the multi-resolution .ico
#              Install: brew install imagemagick
#
# Run from the project root.

set -euo pipefail

ICNS="assets/Pizza Legacy.icns"
OUT_PNG="assets/plegacy_256.png"
OUT_ICO="assets/plegacy.ico"

if [[ ! -f "$ICNS" ]]; then
    echo "ERROR: source icon not found: $ICNS"
    exit 1
fi

# Accept either v7 magick or older convert command
if command -v magick &>/dev/null; then
    MAGICK="magick"
elif command -v convert &>/dev/null; then
    MAGICK="convert"
else
    echo "ERROR: ImageMagick not found (need 'magick' or 'convert')."
    echo "  Install: brew install imagemagick"
    exit 1
fi

TMPDIR_ICONS="$(mktemp -d)"
# Delete directory on exit from this script:
trap 'rm -rf "$TMPDIR_ICONS"' EXIT

echo "Extracting PNG sizes from $ICNS..."
for SIZE in 16 32 48 256; do
    sips -s format png "$ICNS" \
        --resampleHeightWidth "$SIZE" "$SIZE" \
        --out "$TMPDIR_ICONS/plegacy_${SIZE}.png" \
        >/dev/null
done

echo "Writing $OUT_PNG..."
cp "$TMPDIR_ICONS/plegacy_256.png" "$OUT_PNG"

echo "Writing $OUT_ICO..."
"$MAGICK" \
    "$TMPDIR_ICONS/plegacy_16.png" \
    "$TMPDIR_ICONS/plegacy_32.png" \
    "$TMPDIR_ICONS/plegacy_48.png" \
    "$TMPDIR_ICONS/plegacy_256.png" \
    "$OUT_ICO"

echo "Done:"
echo "  $OUT_PNG"
echo "  $OUT_ICO"
