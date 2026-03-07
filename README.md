# Pizza Legacy

![Pizza Legacy city view](screenshots/citymap.jpg)

Pizza Legacy is an open-source reimplementation of the 1994 DOS game
Pizza Tycoon (released as Pizza Connection in Germany). The goal is to
recreate the original engine from scratch in modern C++20 — only the engine
is reimplemented, so you will need a copy of the original game for its assets.

Website: [pizzalegacy.nl](https://pizzalegacy.nl)

## Getting the original game

Pizza Legacy requires the asset files from the original DOS version of
Pizza Tycoon / Pizza Connection. The game is available digitally and
frequently goes on sale for under a euro:

- [GOG](https://www.gog.com/en/game/pizza_connection)
- [Steam](https://store.steampowered.com/app/598990/Pizza_Connection/)

If you have a legal copy of the original DOS game on disk, that works too.

Before buying the game specifically for Pizza Legacy, please check the
current status below — the project is playable but
still in alpha and not yet feature-complete.

The original game manual is available online at
[pizzatycoon.org](https://www.pizzatycoon.org/p/manual.html).

## Current status

**Alpha.** The core gameplay loop is working — you can open restaurants,
hire staff, design pizzas and compete with computer players. However,
expect rough edges: multiple features are incomplete or completely missing, and crashes may occur. Development is active.

See [pizzalegacy.nl](https://pizzalegacy.nl) for a detailed status overview.

## Building

### Prerequisites

- CMake 3.25+
- [Ninja](https://ninja-build.org/) build system
- A C++20 compiler: **GCC 13+**, **Clang 17+**, or **MSVC 2022+**.
  GCC 12 works with the `libfmt-dev` library as a fallback for `std::format`. GCC 11 and below are not supported.
- SDL2 (>= 2.0.16)
- libpng (optional, for PNG screenshots)
- [libADLMIDI](https://github.com/Wohlstand/libADLMIDI) for music support
  (optional); you can install it on your system or download the sources and put them in
  `third_party/libadlmidi/` and it wil be picked up during the build
  automatically

Platform-specific setup:

- **Linux (Debian/Ubuntu):** `sudo apt-get install ninja-build libsdl2-dev libpng-dev pkg-config`
- **macOS:** `brew install sdl2 libpng pkg-config ninja`
- **Windows:** Install SDL2 and libpng via [vcpkg](https://vcpkg.io/)

### Compile

```bash
cmake --preset release
cmake --build --preset release -j
```

If you don't have Ninja, you can build without presets:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Run

Point the game at your original Pizza Tycoon / Pizza Connection
installation directory when prompted on first launch. You need to point to the
directory that contains PT.EXE.

## History

Pizza Legacy has been in development since 2010 as a hobby project. Over
these years the project has gone through several technology migrations,
some of which might have left marks on the code or architecture.

- C++ standard: from pre-C++11 through C++11, C++17, and now C++20
- Build system: from a hand-written Makefile to autotools to CMake
- Graphics: from SDL 1.2 with 8-bit palettes to SDL2 with 32-bit
  surfaces, eventually replacing `SDL_Surface` with a custom Surface class
  and confining SDL to the engine layer
- Pointers: from raw pointers and `auto_ptr` to `shared_ptr`, then
  to `unique_ptr` with raw non-owning pointers

## Contributing

Development currently happens offline and updates are pushed periodically,
so pull requests may not be reviewed or merged at this time.

Questions are welcome via [Codeberg
Issues](https://codeberg.org/cowomaly/pizzalegacy/issues). Please hold off on
bug reports for now; the project has many known rough edges that will be
addressed once core features are complete.

## License

Pizza Legacy is licensed under the
[GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html).

Third-party libraries are in the `third_party/` directory, each with their
own license.
