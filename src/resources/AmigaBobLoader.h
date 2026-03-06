/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "resources/Bob.h"

// Loader for Amiga BOB files (bitplane format)
class AmigaBobLoader {
public:
  static std::unique_ptr<Bob> load(const std::string& filename);

private:
  static constexpr uint8_t MARKER_BYTE_1 = 0x05;
  static constexpr uint8_t MARKER_BYTE_2 = 0x03;

  static uint16_t parse_file_header(const std::vector<uint8_t>& data);
  static std::vector<std::size_t>
  find_frame_markers(const std::vector<uint8_t>& data);

  static std::tuple<FrameInfo, uint8_t, uint16_t>
  parse_frame_header(const std::vector<uint8_t>& data,
                     std::size_t header_offset,
                     std::size_t cumulative_data_offset, int index);
};
