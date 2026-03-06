/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/Bob.h"

#include <cassert>
#include <cstring>

#include "resources/AssetMetadata.h"

Bob::Bob(std::string filename, std::vector<uint8_t> raw_data,
         std::vector<FrameInfo> frames)
    : filename_(std::move(filename)), raw_data_(std::move(raw_data)),
      frames_(std::move(frames)) {}

int Bob::width(std::size_t frame_index) const {
  auto& f = frames_[frame_index];
  return f.width * 2;
}

int Bob::height(std::size_t frame_index) const {
  auto& f = frames_[frame_index];
  return f.height * 2;
}

Size Bob::size(std::size_t frame_index) const {
  auto& f = frames_[frame_index];
  return Size{f.width * 2, f.height * 2};
}

Surface Bob::frame(std::size_t frame_index,
                   std::optional<PaletteHandle> maybe_palette) const {
  assert(frame_index < frames_.size());
  auto& f = frames_[frame_index];
  int w = f.width;
  int h = f.height;
  std::size_t offset = f.data_offset;

  // Determine palette: use override if provided, otherwise lookup in metadata
  PaletteHandle palette;
  if (maybe_palette.has_value()) {
    palette = *maybe_palette;
  }
  else {
    auto palette_opt = resources::AssetMetadataRegistry::get_palette(filename_);
    uint8_t palette_id = palette_opt.value_or(0);
    palette = PaletteHandle{palette_id};
  }

  // raw_data_ is the entire file; we want w*h from offset
  std::vector<uint8_t> picture(
      static_cast<std::size_t>(w * h)); // Allocate w * h bytes
  memcpy(picture.data(), raw_data_.data() + offset,
         static_cast<std::size_t>(w * h));

  return Surface::from_indexed(picture, w, h, palette, 2);
}

std::size_t Bob::frame_count() const { return frames_.size(); }
