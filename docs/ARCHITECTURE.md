# Engine Architecture

Pizza Legacy is a new C++20 engine written from scratch to reproduce the
original game's behavior while providing a modern, portable way to play it.
It is not a decompilation or port of the original engine; it was developed
through a combination of gameplay observation, asset file analysis, and study
of the disassembled DOS executable.

## Original asset format support

The engine parses the original 1994 asset formats:

- **BOB sprites** with animation sequences
- **VGA graphics** with indexed-color palettes
- **FNT bitmap fonts**
- **CMF music** (Creative Music Format, OPL3 FM synthesis via libADLMIDI)
- **VOC audio** (Creative Voice Format, decoded and resampled to stereo)
- City map data, pizza recipes, restaurant layouts, and character data from
  original formats (English version only)

All assets are loaded on demand through a template-based `ResourceManager<T, Loader>`
that caches resources by filename.

## Type-safe coordinate systems

The engine uses phantom types to make coordinate spaces distinct at compile
time. `WorldPoint`, `ViewPoint`, and `ScreenPoint` are separate types that
cannot be accidentally mixed. Camera and viewport transforms convert between
them explicitly. This is zero-cost: the types have the same runtime
representation as their underlying data (e.g. int). The same pattern is used
for IDs (players, restaurants, cities) to prevent mix-ups.

## Rendering pipeline

Rendering uses a layered composition model with per-layer dirty tracking:

- Each view implements a `Renderable` interface with dirty-state tracking
- Layers are cached to SDL surfaces and only recomposed when marked dirty
- 8-bit indexed-color surfaces are expanded to RGBA during composition
- A CRT scaling mode upscales the native 640x400 to 640x480 to approximate
  the original CRT aspect ratio

Screens are managed as a stack, with an overlay system composited on top.
Popups, tooltips, and a debug layer are all independent
overlays that can block or pass through input to the view beneath them.
Screen transitions use palette-based fade-out/fade-in.

## Input system

Input is abstracted into two layers: physical keys (SDL backend) and semantic
actions (game-level). Key bindings support chords (e.g. Ctrl+Shift+B) and are
fully configurable (though not through a UI yet). Listeners register for
actions with priority ordering, and an IME-aware text input mode uses RAII
tokens for safe session management. Continuous axes are computed from input
sources to support keyboard and gamepad input.

## Interaction and UI framework

Mouse interaction is based on rectangular hit regions registered with
callbacks, per layer. Events include enter/leave/hover, click, double-click,
scrolling and drag-and-drop. Registration uses RAII tokens for automatic
cleanup. Tooltips, custom cursors, and exclusive capture modes are built in.

## Audio

The audio system mixes sound effects in real time and plays music through OPL3
FM synthesis. VOC samples are decoded from 8-bit mono to 16-bit stereo
(duplicated channels). Separate volume controls for master, music, and effects,
with gain control.

## Debug console and asset viewers

A built-in debug console accepts commands at runtime for inspecting and
manipulating game state. The engine also ships with dedicated viewer modes for
the original asset formats (sprites, graphics, fonts, palettes, sounds),
launchable via CLI flags, used for helping reverse engineering the original
game's data files.

## Cross-platform

Binary releases are available for Windows, Linux, macOS, and Haiku. The engine
is designed to be highly portable and should build on most modern desktop platforms
with SDL3 support, including the BSDs and other Unix-like systems.

Only the primary release platforms are regularly tested, so support for BSD and
other secondary platforms is currently best-effort, but portability is a major
project goal and platform-specific code is kept to a minimum.

Save data and configuration files are stored in the appropriate user data
directories for each platform; the only external files accessed are the
original game assets from the directory you selected.
