# Building Pizza Legacy

## Prerequisites

- CMake 3.25+
- [Ninja](https://ninja-build.org/) build system (optional)
- A C++20 compiler: **GCC 13+**, **Clang 17+**, or **MSVC 2022+**.
  GCC 12 works with the libfmt-dev library as a fallback for std::format; GCC
  11 and below are not supported.
- SDL3 (>= 3.4.0)
- zlib (save file compression)
- [libADLMIDI](https://github.com/Wohlstand/libADLMIDI) for music support
  (optional); you can install it on your system or download the sources and put
  them in `third_party/libadlmidi/` and it will be picked up during the build
  automatically

Platform-specific setup:

- **Linux (Debian/Ubuntu):** `sudo apt-get install ninja-build libsdl3-dev pkg-config`
- **macOS:** `brew install sdl3 pkg-config cmake ninja`
- **Windows:** Install SDL3 via `vcpkg`

## Compile

```bash
cmake --preset release
cmake --build --preset release -j
```

If you don't have Ninja, you can build without presets:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```
