/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/AmigaBobLoader.h"

#include <cstddef>
#include <fstream>

#include "engine/Fatal.h"
#include "gfx/Surface.h"
#include "resources/AssetMetadata.h"
#include "resources/BitplaneDecoder.h"

std::unique_ptr<Bob> AmigaBobLoader::load(const std::string& filename) {
  Logger::info("AmigaBobLoader: Loading file '{}'", filename);

  std::ifstream is = fileutils::open(filename);
  if (!is.is_open()) {
    fatal("AmigaBobLoader: Could not open BOB file: " + filename);
  }

  is.seekg(0, std::ios::end);
  std::streamsize length = is.tellg();
  is.seekg(0, std::ios::beg);

  Logger::debug("AmigaBobLoader: File size: {} bytes", length);

  std::vector<uint8_t> data(static_cast<std::size_t>(length));

  if (!is.read(reinterpret_cast<char*>(data.data()), length)) {
    fatal("AmigaBobLoader: Failed to read BOB file data: " + filename);
  }

  // Frame count is at offset 0x00, big-endian uint16
  uint16_t frame_count = (static_cast<uint16_t>(data[0]) << 8) | data[1];

  Logger::debug(
      "AmigaBobLoader: File header: frame_count={}, unknown_bytes=[{:02X} "
      "{:02X} {:02X} {:02X}]",
      frame_count, data[2], data[3], data[4], data[5]);

  // Frame headers are fixed 14 bytes each, starting after 6-byte file header
  constexpr std::size_t FILE_HEADER_SIZE = 6;
  constexpr std::size_t FRAME_HEADER_SIZE = 14;

  struct FrameHdr {
    int width;
    int height;
    int num_bitplanes;
    int header_words;       // words-per-line as stored in header (bytes 12..13)
    std::size_t header_off; // header file offset (for debugging)
  };

  std::vector<FrameHdr> frames;
  frames.reserve(frame_count);

  // Parse all headers and collect header_words if present
  for (int i = 0; i < frame_count; ++i) {
    std::size_t header_offset =
        FILE_HEADER_SIZE + static_cast<std::size_t>(i) * FRAME_HEADER_SIZE;
    if (header_offset + FRAME_HEADER_SIZE > data.size()) {
      Logger::warn(
          "Header for frame {} would read past EOF; aborting header parse", i);
      break;
    }

    int width = static_cast<int>(data[header_offset + 8]);
    int height = static_cast<int>(data[header_offset + 9]);
    int num_bitplanes = static_cast<int>(data[header_offset + 10]);
    int header_words = (static_cast<int>(data[header_offset + 12]) << 8) |
                       static_cast<int>(data[header_offset + 13]);

    // If header_words == 0, fall back to calculated words
    if (header_words == 0) {
      header_words = (width + 15) / 16;
    }

    frames.push_back(
        FrameHdr{width, height, num_bitplanes, header_words, header_offset});
    Logger::debug("Parsed header {}: w={} h={} bp={} header_words={}", i, width,
                  height, num_bitplanes, header_words);
  }

  // Compute header end (first data byte)
  std::size_t header_end =
      FILE_HEADER_SIZE +
      static_cast<std::size_t>(frame_count) * FRAME_HEADER_SIZE;
  std::size_t base_data_start = header_end;

  // Not sure why this is needed but it seems to work
  const int extra_magic_shift = 8;

  // Build frame offsets using calculated words_per_line from width
  struct FrameMeta {
    int width;
    int height;
    int bp;
    std::size_t offset;
    std::size_t frame_size;
    int words_per_line;
    int bytes_per_row;
  };
  std::vector<FrameMeta> metas;
  metas.reserve(frame_count);

  std::size_t cursor =
      base_data_start + static_cast<std::size_t>(extra_magic_shift);

  for (int i = 0; i < frame_count; ++i) {
    int w = frames[i].width;
    int h = frames[i].height;
    int bp = frames[i].num_bitplanes;

    // Amiga alignment calculation
    int words_per_line = (w + 15) / 16;     // number of 16-pixel words
    int bytes_per_row = words_per_line * 2; // 2 bytes per word (16 bits)
    std::size_t bytes_per_plane =
        static_cast<std::size_t>(bytes_per_row) * static_cast<std::size_t>(h);
    std::size_t frame_size = bytes_per_plane * static_cast<std::size_t>(bp);

    // align per-frame to 2 bytes (word) or 4 bytes (long) if needed; start with
    // 2-byte.
    std::size_t aligned_frame_size =
        (frame_size + 1) & ~static_cast<std::size_t>(1);

    metas.push_back(FrameMeta{.width = w,
                              .height = h,
                              .bp = bp,
                              .offset = cursor,
                              .frame_size = frame_size,
                              .words_per_line = words_per_line,
                              .bytes_per_row = bytes_per_row});

    cursor += aligned_frame_size;
  }

  std::size_t file_size = data.size();

  Logger::info("AmigaBobLoader: Successfully parsed {} frames", metas.size());

  // Get palette for this file
  auto palette_opt = resources::AssetMetadataRegistry::get_palette(filename);
  uint8_t palette_id = palette_opt.value_or(0);

  // Now convert bitplane data to chunky format for each frame
  std::vector<uint8_t> converted_data;
  std::vector<FrameInfo> converted_frames;
  std::size_t converted_offset = 0;

  for (std::size_t i = 0; i < metas.size(); ++i) {
    const auto& fm = metas[i];

    // bounds check
    if (fm.offset + fm.frame_size > file_size) {
      Logger::warn(
          "Frame {} data extends past EOF; offset 0x{:X} size {} file_size {}",
          i, fm.offset, fm.frame_size, file_size);
      fatal("AmigaBobLoader: Frame data extends past EOF");
    }

    // slice bitplane bytes
    std::vector<uint8_t> bitplane_data(
        data.begin() + fm.offset, data.begin() + fm.offset + fm.frame_size);

    Logger::debug(
        "AmigaBobLoader: Decoding frame {} ({}x{}, {} planes, {} bytes)", i,
        fm.width, fm.height, fm.bp, fm.frame_size);

    // Decode using bytes_per_row (fm.bytes_per_row); layout Planar
    // Use scale=1 to get unscaled pixels (Bob format expects unscaled data)
    gfx::Surface decoded =
        BitplaneDecoder::decode(bitplane_data, fm.width, fm.height, fm.bp,
                                fm.bytes_per_row, palette_id, /*scale=*/1);

    // Extract pixel data from decoded surface
    const auto& pixels = decoded.pixels();

    Logger::debug("AmigaBobLoader: Frame {} decoded to {} pixels", i,
                  pixels.size());

    // Append to converted data
    converted_data.insert(converted_data.end(), pixels.begin(), pixels.end());

    // Create FrameInfo for converted data
    FrameInfo converted_frame;
    converted_frame.width = fm.width;
    converted_frame.height = fm.height;
    converted_frame.data_offset = converted_offset;
    converted_frame.num_bitplanes = fm.bp;
    converted_frames.push_back(converted_frame);

    converted_offset += pixels.size();
  }

  Logger::info("AmigaBobLoader: Converted {} bytes of bitplane data to {} "
               "bytes of chunky data",
               data.size(), converted_data.size());

  return std::make_unique<Bob>(filename, std::move(converted_data),
                               std::move(converted_frames));
}
