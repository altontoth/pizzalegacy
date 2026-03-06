/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <vector>

namespace gfx {
class Surface;
}

// Decoder for Amiga bitplane graphics format
// Used in the Amiga version of Pizza Connection. The graphics files in the
// Amiga version use bitplane encoding (interleaved color planes) instead of the
// RLE compression used in the DOS version.
class BitplaneDecoder {
public:
  // Decode Amiga bitplane format to indexed surface
  //
  // Args:
  //   raw_data: The complete file data
  //   width: Image width in pixels
  //   height: Image height in pixels
  //   num_bitplanes: Number of color planes
  //   bytes_per_line: Bytes per scanline per plane (including padding). If
  //                   negative, auto-calculates (badly)
  //   palette_index: Palette bank index to use (default 0)
  //   scale: Scaling factor for output surface (1 = no scaling, 2 = 2x)
  //   bitplane_padding: Extra bytes between each bitplane
  //   bitplane_size_extra: Extra bytes to add to each bitplane's size
  //
  //   (This is all very convoluted because I had to do lots of debugging
  //   to find working values)
  static gfx::Surface decode(const std::vector<uint8_t>& raw_data, int width,
                             int height, int num_bitplanes = 8,
                             int bytes_per_line = -1, uint8_t palette_index = 0,
                             int scale = 2, int bitplane_padding = 0,
                             int bitplane_size_extra = 0);
};
