/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/VgaWriter.h"

#include <algorithm>
#include <cstddef>

#include "engine/Fatal.h"
#include "engine/geom/Spaces.h"
#include "resources/Vga.h"

namespace {

// Control bytes at or below this are a literal run of (byte + 1) pixels;
// above it, a run of (byte - threshold) copies of the pixel that follows.
constexpr int rle_threshold = 128;
constexpr int max_literal_run = rle_threshold + 1;
constexpr int max_repeat_run = 255 - rle_threshold;

void push_u16le(std::vector<uint8_t>& out, std::size_t value) {
  out.push_back(static_cast<uint8_t>(value & 0xff));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

int run_length_at(const std::vector<uint8_t>& pixels, std::size_t at) {
  int run = 1;
  while (at + run < pixels.size() && pixels[at + run] == pixels[at] &&
         run < max_repeat_run) {
    ++run;
  }
  return run;
}

std::vector<uint8_t> encode(const std::vector<uint8_t>& pixels) {
  std::vector<uint8_t> out;
  std::size_t at = 0;
  while (at < pixels.size()) {
    const int repeat = run_length_at(pixels, at);
    if (repeat > 1) {
      out.push_back(static_cast<uint8_t>(rle_threshold + repeat));
      out.push_back(pixels[at]);
      at += repeat;
      continue;
    }
    // Gather single pixels until one of them starts a run worth encoding.
    const std::size_t start = at;
    while (at < pixels.size() &&
           static_cast<int>(at - start) < max_literal_run &&
           run_length_at(pixels, at) == 1) {
      ++at;
    }
    const std::size_t count = at - start;
    out.push_back(static_cast<uint8_t>(count - 1));
    out.insert(out.end(), pixels.begin() + static_cast<std::ptrdiff_t>(start),
               pixels.begin() + static_cast<std::ptrdiff_t>(at));
  }
  return out;
}

} // namespace

std::vector<uint8_t> VgaWriter::to_bytes(const gfx::Surface& surface,
                                         int scale) {
  if (scale < 1) {
    fatal("VgaWriter: scale must be at least 1");
  }
  const int width = surface.width() / scale;
  const int height = surface.height() / scale;

  std::vector<uint8_t> pixels;
  pixels.reserve(static_cast<std::size_t>(width) *
                 static_cast<std::size_t>(height));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      pixels.push_back(surface.get(ViewPoint{x * scale, y * scale}));
    }
  }

  const std::vector<uint8_t> body = encode(pixels);

  std::vector<uint8_t> out;
  out.reserve(VGA_HEADER_SIZE + body.size() + 1);
  push_u16le(out, static_cast<std::size_t>(width));
  push_u16le(out, static_cast<std::size_t>(height));
  // The data size counts the threshold byte after the body.
  push_u16le(out, body.size() + 1);
  push_u16le(out, 0);
  out.insert(out.end(), body.begin(), body.end());
  out.push_back(static_cast<uint8_t>(rle_threshold + 1));
  return out;
}
