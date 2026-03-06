/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Implementation based on defimp.c: This code is placed in the Public
 * Domain by its author, Stuart Caie
 */

#pragma once

#include <cstdint>
#include <vector>

/* ImploderDecompressor
 *
 * This is a C++ wrapper around the Imploder decompression library
 * (third_party/explosion). Handles decompression of IMP! compressed files used
 * in the original Pizza Tycoon on Amiga.
 */
class ImploderDecompressor {
public:
  static std::vector<uint8_t>
  decompress(const std::vector<uint8_t>& compressed);
  static bool is_imploder_magic(const std::vector<uint8_t>& data);

private:
  ImploderDecompressor() = delete;

  static constexpr uint32_t IMP_MAGIC = 0x494D5021; // "IMP!"
};
