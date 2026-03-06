/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Based on defimp.c: This code is placed in the Public Domain by its author,
 * Stuart Caie
 */

#include "resources/ImploderDecompressor.h"
#include "engine/Fatal.h"
#include "engine/Logger.h"

// Forward declare the C decompression function from third_party/explosion
extern "C" {
char* defimp(unsigned char* in, unsigned int in_len, unsigned char** out_ptr,
             unsigned int* out_len_ptr);
}

std::vector<uint8_t>
ImploderDecompressor::decompress(const std::vector<uint8_t>& compressed) {
  if (compressed.size() < 0x30) {
    fatal("Compressed data too short to be IMP! file");
  }

  unsigned char* out = nullptr;
  unsigned int out_len = 0;

  // Call C decompression function
  char* error =
      defimp(const_cast<unsigned char*>(compressed.data()),
             static_cast<unsigned int>(compressed.size()), &out, &out_len);

  if (error) {
    std::string error_msg = "Imploder decompression failed: ";
    error_msg += error;
    fatal(error_msg);
  }

  // Copy decompressed data into vector
  std::vector<uint8_t> result(out, out + out_len);

  // Free C-allocated memory
  free(out);

  Logger::debug("Decompressed IMP! file: {} bytes -> {} bytes",
                compressed.size(), out_len);

  return result;
}

bool ImploderDecompressor::is_imploder_magic(const std::vector<uint8_t>& data) {
  if (data.size() < 4) {
    return false;
  }

  uint32_t magic = (static_cast<uint32_t>(data[0]) << 24) |
                   (static_cast<uint32_t>(data[1]) << 16) |
                   (static_cast<uint32_t>(data[2]) << 8) |
                   static_cast<uint32_t>(data[3]);

  return magic == IMP_MAGIC;
}
