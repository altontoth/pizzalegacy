# SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
#
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(plegacy VERSION 0.1.0 LANGUAGES C CXX)

# Generate Version.h from the project version
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/Version.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/Version.h
    @ONLY
)

# Generate versioned READMEs for distribution packages into the build dir
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/dist/README-macOS.txt.in
    ${CMAKE_BINARY_DIR}/dist/README-macOS.txt
    @ONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/dist/README-Windows.txt.in
    ${CMAKE_BINARY_DIR}/dist/README-Windows.txt
    @ONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/dist/README-Linux.txt.in
    ${CMAKE_BINARY_DIR}/dist/README-Linux.txt
    @ONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/dist/README-Haiku.txt.in
    ${CMAKE_BINARY_DIR}/dist/README-Haiku.txt
    @ONLY
)
# Copy the credits/third-party listing into the build dist dir
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/docs/CREDITS.md
    ${CMAKE_BINARY_DIR}/dist/THIRD_PARTIES.txt
    COPYONLY
)

# ---------------------------------------------------------
#  User selectable options
# -----------------------------------------------------

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_EXPORT_COMPILE_COMMANDS YES)
option(USE_CCACHE "Use ccache if available" ON)

if(USE_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}" CACHE STRING "C compiler launcher" FORCE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "C++ compiler launcher" FORCE)
  else()
    message(WARNING "USE_CCACHE=ON but 'ccache' not found; continuing without it")
    unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
    unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
  endif()
else()
  unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
  unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
endif()

# ---------------------------------------------------------
#  Compiler diagnostics & sanitizers
# -----------------------------------------------------

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wno-shadow
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wnull-dereference
        -Wimplicit-fallthrough
        -Wno-missing-designated-field-initializers
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
    add_link_options(
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Wall
        #-Wextra
        #-Wno-shadow
        #-Wno-unused-parameter
        #-Wno-unused-variable
        #-Wnull-dereference
        #-Wimplicit-fallthrough
        $<$<CONFIG:Debug>:-g1>
        $<$<CONFIG:Debug>:-O0>
    )
elseif(MSVC)
    add_compile_options(
        /W4
        /wd4100
        /wd4189
        /permissive-
        /MP
        $<$<CONFIG:Debug>:/Od>     # Disable optimizations in Debug
        $<$<CONFIG:Debug>:/Zi>     # Debug info
        $<$<CONFIG:Debug>:/RTC1>   # Runtime checks
    )
    add_link_options(
        $<$<CONFIG:Debug>:/DEBUG>
    )
else()
    # Non-GNU/Clang/MSVC compilers still get warnings below (if any).
endif()

# ---------------------------------------------------------
#  Check that the compiler really supports std::format
# -----------------------------------------------------
include(CheckCXXSourceCompiles)
check_cxx_source_compiles(
    "#include <format>\n#include <string>\nint main(){ std::string s = std::format(\"Hello, {}!\", \"world\"); }"
    HAS_STD_FORMAT
)

# If std::format missing, try to find fmt library
if(NOT HAS_STD_FORMAT)
    find_package(fmt QUIET)

    if(NOT fmt_FOUND)
        message(FATAL_ERROR
            "No std::format available and fmt library not found. Please upgrade your toolchain or install the fmt library.")
    endif()
endif()

# Bit hacky but needed this to get it to work for some reason
# (that reason probably being me not understanding cmake)
if(HAS_STD_FORMAT)
    set(HAS_STD_FORMAT_VALUE 1)
else()
    set(HAS_STD_FORMAT_VALUE 0)
endif()

add_compile_definitions(HAS_STD_FORMAT=${HAS_STD_FORMAT_VALUE})

# ---------------------------------------------------------
#  SDL2 - not much happening without that
# -----------------------------------------------------
if(WIN32)
    find_package(SDL2 CONFIG REQUIRED)
    set(SDL2_LIBS SDL2::SDL2 SDL2::SDL2main)
    set(SDL2_CFLAGS "")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2>=2.0.16)
    set(SDL2_LIB_DIRS ${SDL2_LIBRARY_DIRS})
    set(SDL2_LIBS   ${SDL2_LIBRARIES})
    set(SDL2_CFLAGS ${SDL2_CFLAGS_OTHER})
endif()

# ---------------------------------------------------------
#  libpng - for PNG screenshots
# -----------------------------------------------------
if(WIN32)
    find_package(PNG CONFIG REQUIRED)
    set(PNG_LIBS PNG::PNG)
    set(PNG_CFLAGS "")
    message(STATUS "libpng: found (system CONFIG package)")
else()
    if(NOT PkgConfig_FOUND)
        find_package(PkgConfig REQUIRED)
    endif()
    pkg_check_modules(PNG REQUIRED libpng)
    set(PNG_LIBS    ${PNG_LIBRARIES})
    set(PNG_CFLAGS  ${PNG_CFLAGS_OTHER})
    message(STATUS "libpng: found (pkg-config)")
endif()

# ---------------------------------------------------------
#  zlib - save file compression
# -----------------------------------------------------
find_package(ZLIB REQUIRED)
message(STATUS "zlib: found")

# ---------------------------------------------------------
# Case sensitivy options
# -----------------------------------------------------
set(PLEGACY_CASE_INSENSITIVE_FS "" CACHE STRING "Enable case-insensitive fallback when opening files (ON, OFF, or empty for auto)")

if(PLEGACY_CASE_INSENSITIVE_FS STREQUAL "")
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "Case insensitivity disabled")
    set(_PLEGACY_CASE_INSENSITIVE_FS OFF)
  else()
    message(STATUS "Case insensitivity enabled")
    set(_PLEGACY_CASE_INSENSITIVE_FS ON)
  endif()
else()
  set(_PLEGACY_CASE_INSENSITIVE_FS ${PLEGACY_CASE_INSENSITIVE_FS})
endif()

add_compile_definitions(PLEGACY_CASE_INSENSITIVE_FS=$<BOOL:${_PLEGACY_CASE_INSENSITIVE_FS}>)


#  Source file lists taken from include file
include(source_manifest.cmake)

# ---------------------------------------------------------
#  libADLMIDI - OPL2/CMF for music support
#
#  Search order when PLEGACY_ENABLE_MUSIC=ON:
#    1. System-installed libADLMIDI (pkg-config on Unix, CONFIG on Windows)
#    2. Vendored copy at third_party/libadlmidi/ (if present)
#  Set -DPLEGACY_ENABLE_MUSIC=OFF to disable music entirely
# -----------------------------------------------------
option(PLEGACY_ENABLE_MUSIC "Enable OPL2/CMF music playback (requires libADLMIDI)" ON)

if(PLEGACY_ENABLE_MUSIC)
  set(_ADLMIDI_FOUND    FALSE)
  set(_ADLMIDI_VENDORED FALSE)

  # system package
  if(WIN32)
    find_package(libADLMIDI CONFIG QUIET)
    if(libADLMIDI_FOUND)
      set(_ADLMIDI_FOUND TRUE)
      message(STATUS "libADLMIDI: using system CONFIG package")
    endif()
  else()
    pkg_check_modules(ADLMIDI QUIET libADLMIDI)
    if(ADLMIDI_FOUND)
      set(_ADLMIDI_FOUND TRUE)
      message(STATUS "libADLMIDI: using system pkg-config package")
    endif()
  endif()

  # vendored fallback
  if(NOT _ADLMIDI_FOUND)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libadlmidi/CMakeLists.txt")
      message(STATUS "libADLMIDI: using vendored copy from third_party/")
      set(_ADLMIDI_VENDORED TRUE)
      set(_ADLMIDI_FOUND    TRUE)

      set(WITH_UNIT_TESTS     OFF)
      set(WITH_MIDI_SEQUENCER ON)
      set(WITH_EMBEDDED_BANKS ON)
      set(WITH_HQ_RESAMPLER   OFF)
      set(USE_NUKED_EMULATOR  ON)
      set(USE_DOSBOX_EMULATOR OFF)
      set(USE_YMFM_EMULATOR   OFF)
      set(USE_OPAL_EMULATOR   OFF)
      set(USE_JAVA_EMULATOR   OFF)
      set(USE_ESFMU_EMULATOR  OFF)
      set(USE_MAME_EMULATOR   OFF)
      set(WITH_ADLMIDI2       OFF)
      set(libADLMIDI_STATIC   OFF  CACHE BOOL "" FORCE)
      set(libADLMIDI_SHARED   ON CACHE BOOL "" FORCE)

      add_subdirectory(third_party/libadlmidi)

      # Suppress warnings from vendored C sources so they don't pollute output
      if(MSVC)
        target_compile_options(ADLMIDI_shared PRIVATE /W0)
      else()
        target_compile_options(ADLMIDI_shared PRIVATE -w)
      endif()
    else()
      message(FATAL_ERROR
        "PLEGACY_ENABLE_MUSIC is ON but libADLMIDI is not available:\n"
        "  - system package not found\n"
        "  - no vendored copy at third_party/libadlmidi/\n"
        "Either install libADLMIDI, add the vendored source tree, or "
        "configure with -DPLEGACY_ENABLE_MUSIC=OFF")
    endif()
  endif()
endif()

add_compile_definitions(PLEGACY_ENABLE_MUSIC=$<BOOL:${PLEGACY_ENABLE_MUSIC}>)

# ---------------------------------------------------------
#  Use static MSVC runtime (/MT, /MTd)
# -----------------------------------------------------

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---------------------------------------------------------
#  Build the game static library
# -----------------------------------------------------
add_library(game_plegacy STATIC ${GAME_PLEGACY_SOURCES})

if(NOT HAS_STD_FORMAT)
    # Add include dirs & compile defs to the static library
    target_include_directories(game_plegacy PUBLIC fmt::fmt)
    target_link_libraries(game_plegacy PUBLIC fmt::fmt)
    target_compile_definitions(game_plegacy PUBLIC HAS_STD_FORMAT=0)
else()
    target_compile_definitions(game_plegacy PUBLIC HAS_STD_FORMAT=1)
endif()

# We expose include dirs so that dependent targets can #include headers directly.
target_include_directories(game_plegacy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(APPLE)
    # ZLIB::ZLIB adds the macOS SDK path as a plain -I, which breaks the
    # -isystem ordering required by LLVM libc++ 20. Strip the include dirs;
    # zlib.h is already reachable via the existing -isystem path.
    set_target_properties(ZLIB::ZLIB PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "")
endif()
target_link_libraries(game_plegacy PRIVATE ZLIB::ZLIB)

# ---------------------------------------------------------
#  Main executable "plegacy"
# -----------------------------------------------------
add_executable(plegacy
    ${ENGINE_PLEGACY_SOURCES}
)

# Windows-specific: embed the application icon + version info and set the output name
if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/Pizza Legacy.rc.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/Pizza Legacy.rc"
        @ONLY
    )
    target_sources(plegacy PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/assets/Pizza Legacy.rc")
    set_target_properties(plegacy PROPERTIES OUTPUT_NAME "Pizza Legacy")
endif()

# Build as a proper macOS .app bundle when on Apple platforms
if(APPLE)
    set(MACOSX_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/Pizza Legacy.icns")
    set_source_files_properties("${MACOSX_ICON}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(plegacy PRIVATE "${MACOSX_ICON}")
    set_target_properties(plegacy PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "Pizza Legacy"
        MACOSX_BUNDLE_BUNDLE_NAME "Pizza Legacy"
        MACOSX_BUNDLE_GUI_IDENTIFIER "nl.pizzalegacy"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright 2010-2026 Cowomaly. Licensed under GPL v3."
        MACOSX_BUNDLE_ICON_FILE "Pizza Legacy.icns"
    )
endif()

target_compile_definitions(plegacy PRIVATE $<$<CONFIG:Debug>:DEBUG>)

# add HAS_STD_FORMAT macro (1/0)
if(HAS_STD_FORMAT)
    target_compile_definitions(plegacy PRIVATE HAS_STD_FORMAT=1)
else()
    target_compile_definitions(plegacy PRIVATE HAS_STD_FORMAT=0)
endif()

target_link_directories(plegacy PRIVATE ${SDL2_LIB_DIRS})
target_link_libraries(plegacy PRIVATE game_plegacy ${SDL2_LIBS} ${PNG_LIBS})

# ---------------------------------------------------------
# Link libADLMIDI to plegacy (AudioManager lives in ENGINE_PLEGACY_SOURCES)
# -----------------------------------------------------
if(PLEGACY_ENABLE_MUSIC)
  if(_ADLMIDI_VENDORED)
    target_link_libraries(plegacy PRIVATE ADLMIDI)
    target_include_directories(plegacy PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libadlmidi/include)
  elseif(WIN32)
    target_link_libraries(plegacy PRIVATE libADLMIDI)
  else()
    target_link_libraries(plegacy PRIVATE ${ADLMIDI_LIBRARIES})
    target_include_directories(plegacy PRIVATE ${ADLMIDI_INCLUDE_DIRS})
    target_link_directories(plegacy PRIVATE ${ADLMIDI_LIBRARY_DIRS})
    if(APPLE AND ADLMIDI_LIBRARY_DIRS)
      set_target_properties(plegacy PROPERTIES
          BUILD_RPATH "${ADLMIDI_LIBRARY_DIRS}")
    endif()
  endif()
endif()

target_compile_options(plegacy PRIVATE ${SDL2_CFLAGS} ${PNG_CFLAGS})

# ---------------------------------------------------------
#  Developer convenience targets: "format" and "tags"
# -----------------------------------------------------
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE PLEGACY_FORMAT_FILES CONFIGURE_DEPENDS
        "src/*.cpp" "src/*.inl" "src/*.h" "!src/third_party/*")
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${PLEGACY_FORMAT_FILES}
        COMMENT "Formatting source tree with clang-format")
endif()

find_program(CTAGS ctags)
if(CTAGS)
    add_custom_target(tags
        COMMAND ${CTAGS} -R --extra=+f ${CMAKE_CURRENT_SOURCE_DIR}/src
        COMMENT "Generating ctags for src/")
endif()


set(PLEGACY_DIST_SOURCES
   ${GAME_PLEGACY_SOURCES}
   ${ENGINE_PLEGACY_SOURCES}
   ${PLEGACY_HEADERS}
   ${PLEGACY_DIST_EXTRA}
)

target_include_directories(plegacy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(plegacy SYSTEM PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_include_directories(game_plegacy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(game_plegacy SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# ---------------------------------------------------------
#  Unit tests (Catch2) - disabled by default
#  Enable with -DPLEGACY_BUILD_TESTS=ON
# -----------------------------------------------------
option(PLEGACY_BUILD_TESTS "Build unit tests (requires Catch2 3)" OFF)

if(PLEGACY_BUILD_TESTS)
    find_package(Catch2 3 REQUIRED)
    enable_testing()

    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "tests/*.cpp")
    if(TEST_SOURCES)
        add_executable(test_plegacy ${TEST_SOURCES}
            tests/stubs/test_stubs.cpp)
        target_link_libraries(test_plegacy PRIVATE game_plegacy Catch2::Catch2WithMain)
        target_include_directories(test_plegacy PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_include_directories(test_plegacy SYSTEM PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        )
        include(CTest)
        include(Catch)
        catch_discover_tests(test_plegacy)
    endif()
endif()


# ---------------------------------------------------------
#  Release packaging targets
#
#  Usage (from project root):
#    cmake --build --preset release --target release_linux
#    cmake --build --preset release --target release_macos
#    cmake --build --preset release --target release_windows
# ---------------------------------------------------------

set(PLEGACY_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools")

if(UNIX AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    add_custom_target(release_linux
        DEPENDS plegacy
        COMMAND "${PLEGACY_TOOLS_DIR}/package_linux_appimage.sh"
                "${CMAKE_BINARY_DIR}"
                "${PROJECT_VERSION}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Packaging Linux AppImage v${PROJECT_VERSION}"
        VERBATIM
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    add_custom_target(release_haiku
        DEPENDS plegacy
        COMMAND "${PLEGACY_TOOLS_DIR}/package_haiku_zip.sh"
                "${CMAKE_BINARY_DIR}"
                "${PROJECT_VERSION}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Packaging Haiku ZIP v${PROJECT_VERSION}"
        VERBATIM
    )
endif()

if(APPLE)
    add_custom_target(release_macos
        DEPENDS plegacy
        COMMAND "${PLEGACY_TOOLS_DIR}/package_macos_app.sh"
                "${CMAKE_BINARY_DIR}"
                "${PROJECT_VERSION}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Packaging macOS app bundle v${PROJECT_VERSION}"
        VERBATIM
    )
endif()

if(WIN32)
    add_custom_target(release_windows
        DEPENDS plegacy
        COMMAND powershell -ExecutionPolicy Bypass -File
                "${PLEGACY_TOOLS_DIR}/package_windows_zip.ps1"
                -BuildDir "${CMAKE_BINARY_DIR}"
                -Version "${PROJECT_VERSION}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Packaging Windows ZIP v${PROJECT_VERSION}"
        VERBATIM
    )
endif()
