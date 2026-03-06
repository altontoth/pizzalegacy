#!/bin/sh

# Generate source_manifest.cmake with curated source/header file lists.

# Use C locale for consistent sorting across different systems
export LC_ALL=C

ROOTDIR="$(dirname "$0")/.."
ASSETDIR="$ROOTDIR/assets"
OUTFILE="$ROOTDIR/source_manifest.cmake"

echo "Generating $OUTFILE..."

# Start clean
rm -f "$OUTFILE"

# Header
{
    echo "# Auto-generated file. Do not edit manually."
    echo "# Run tools/gen_source_manifest.sh to regenerate."
    echo ""
} >> "$OUTFILE"



# All game modules, plus other things not dependent on SDL
echo "set(GAME_PLEGACY_SOURCES" >> "$OUTFILE"
find "$ROOTDIR/src/core" -type f \( -name '*.cpp' \) | sort | sed "s|^|    |" >> "$OUTFILE"
find "$ROOTDIR/src/conversations" -type f \( -name '*.cpp' \) | sort | sed "s|^|    |" >> "$OUTFILE"
find "$ROOTDIR/src/factories" -type f \( -name '*.cpp' \) | sort | sed "s|^|    |" >> "$OUTFILE"
find "$ROOTDIR/src/views" -type f \( -name '*.cpp' \) | sort | sed "s|^|    |" >> "$OUTFILE"
find "$ROOTDIR/src/gfx" -type f \( -name '*.cpp' \) | sort | sed "s|^|    |" >> "$OUTFILE"
find "$ROOTDIR/src/resources" -type f \( -name '*.cpp' -o -name '*.c' \) | sort | sed "s|^|    |" >> "$OUTFILE"
echo ")" >> "$OUTFILE"
echo "" >> "$OUTFILE"

# SDL/engine logic: engine/ and audio stuff
echo "set(ENGINE_PLEGACY_SOURCES" >> "$OUTFILE"
find "$ROOTDIR/src/engine" -type f \( -name '*.cpp' -o -name '*.c' \) | sort | sed "s|^|    |" >> "$OUTFILE"
find "$ROOTDIR/src" -maxdepth 1 \( -name '*.cpp' \) | sort | sed "s|^|    |" >> "$OUTFILE"
find "$ROOTDIR/third_party/savepng/" \( -name '*.c' \) | sort | sed "s|^|    |" >> "$OUTFILE"
find "$ROOTDIR/third_party/tinyfiledialogs/" \( -name '*.c' \) | sort | sed "s|^|    |" >> "$OUTFILE"
echo ")" >> "$OUTFILE"


# Header and inline files
echo "set(PLEGACY_HEADERS" >> "$OUTFILE"
find "$ROOTDIR/src" -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.inl' \) | sort | sed "s|^|    |" >> "$OUTFILE"
find "$ROOTDIR/third_party" -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.inl' \) | sort | sed "s|^|    |" >> "$OUTFILE"
echo ")" >> "$OUTFILE"
echo "" >> "$OUTFILE"

# Extras (manual)
echo "set(PLEGACY_DIST_EXTRA" >> "$OUTFILE"

# Top-level files
for f in CMakeLists.txt README.md LICENSE; do
    [ -f "$ROOTDIR/$f" ] && echo "    $f" >> "$OUTFILE"
done
echo "    $OUTFILE" >> "$OUTFILE"

# Assets directory
[ -d "$ASSETDIR" ] && echo "    assets/" >> "$OUTFILE"

echo ")" >> "$OUTFILE"

sed -i '' -E 's|\.\./tools/\.\./||g' "$OUTFILE"
sed -i '' -E 's|tools/\.\./||g' "$OUTFILE"

echo "Done. Wrote $OUTFILE"
