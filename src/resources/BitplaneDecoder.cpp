/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/BitplaneDecoder.h"

#include "gfx/Surface.h"

gfx::Surface BitplaneDecoder::decode(const std::vector<uint8_t>& raw_data,
                                     int width, int height, int num_bitplanes,
                                     int bytes_per_line, uint8_t palette_index,
                                     int scale, int bitplane_padding,
                                     int bitplane_size_extra) {
  std::vector<uint8_t> pixels(width * height, 0);

  // Auto-calculate bytes per line if not provided
  if (bytes_per_line < 0) {
    bytes_per_line = (width + 7) / 8;
  }

  Logger::info("Decoding bitplane: {}x{}, {} planes, {} bytes/line, scale={}, "
               "padding={}, size_extra={}",
               width, height, num_bitplanes, bytes_per_line, scale,
               bitplane_padding, bitplane_size_extra);

  // Planar format: all scanlines for plane0, then all for plane1, etc.
  // Layout: [Plane0: all lines][padding][Plane1: all lines][padding][Plane2:
  // all lines + extra] Pre-calculate start offset for each bitplane
  std::vector<int> bitplane_offsets;
  int offset = 0;

  for (int bp = 0; bp < num_bitplanes; ++bp) {
    bitplane_offsets.push_back(offset);
    int this_plane_size = bytes_per_line * height;
    // Only add extra bytes to the LAST bitplane
    if (bp == num_bitplanes - 1) {
      this_plane_size += bitplane_size_extra;
    }

    int end_offset = offset + this_plane_size - 1;

    offset += this_plane_size + bitplane_padding;
  }

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      uint8_t pixel = 0;

      // For each bitplane
      for (int bp = 0; bp < num_bitplanes; ++bp) {
        // Use pre-calculated offset for this bitplane
        int byte_offset = bitplane_offsets[bp] + y * bytes_per_line + x / 8;
        int bit_index = 7 - (x % 8); // Bit 7 is leftmost pixel in byte

        if (byte_offset < static_cast<int>(raw_data.size())) {
          uint8_t byte_val = raw_data[byte_offset];
          int bit_val = (byte_val >> bit_index) & 1;

          // Set the appropriate bit in the pixel value
          pixel |= (bit_val << bp);
        }
      }

      pixels[y * width + x] = pixel;
    }
  }

  return gfx::Surface::from_indexed(pixels, width, height,
                                    gfx::PaletteHandle{palette_index}, scale);
}
