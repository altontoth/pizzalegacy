/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <map>
#include <string>
#include <vector>

#include "engine/gfx/Surface.h"

using gfx::PaletteHandle;
using gfx::Surface;

struct FrameInfo {
  int width;
  int height;
  std::size_t data_offset; // Byte offset where pixel data begins for this frame
  int num_bitplanes =
      8; // Number of bitplanes (Amiga BOBs have variable depths)
};

class Bob {
public:
  Bob(std::string filename, std::vector<uint8_t> raw_data,
      std::vector<FrameInfo> frames);
  ~Bob() = default;

  Bob(Bob&&) noexcept = default;
  Bob& operator=(Bob&&) noexcept = default;

  // Delete copy constructor to avoid accidental copying
  Bob(const Bob&) = delete;
  Bob& operator=(const Bob&) = delete;

  int width(std::size_t frame_index) const;
  int height(std::size_t frame_index) const;
  Size size(std::size_t frame_index) const;
  Surface frame(std::size_t frame_index,
                std::optional<PaletteHandle> palette = std::nullopt) const;
  std::size_t frame_count() const;

private:
  std::string filename_;
  std::vector<unsigned char> raw_data_;
  std::vector<FrameInfo> frames_;
};
