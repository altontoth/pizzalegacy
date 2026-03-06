/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum class AudioAssetType : uint32_t { CMF = 1, VOC = 2 };

struct AudioAsset {
  AudioAssetType type;
  std::vector<uint8_t> data;
  std::string debug_name;
};

class MFileLoader {
public:
  static std::vector<AudioAsset> load(const std::string& filename);
};
