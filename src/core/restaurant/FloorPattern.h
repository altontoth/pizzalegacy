/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstdint>
#include <ostream>

#include "core/Demographics.h"
#include "core/finance/Money.h"

class FloorPattern {
public:
  uint8_t vendor_id;
  // order: top-left, top-right, bottom-left, bottom-right
  std::array<uint8_t, 4> tile_indices;
  std::array<int, static_cast<std::size_t>(ConsumerType::Count)> popularity;
  uint8_t popularity_factor;
  uint8_t quality = 0;
  Money price_per_tile;

  int popularity_for(ConsumerType type) const {
    return popularity[static_cast<std::size_t>(type)] * popularity_factor / 4;
  };
};

inline std::ostream& operator<<(std::ostream& os, const FloorPattern& fp) {
  os << "FloorPattern {";
  os << " vendor_id=" << static_cast<int>(fp.vendor_id);

  os << ", tile_indices=[";
  for (std::size_t i = 0; i < fp.tile_indices.size(); ++i) {
    os << static_cast<int>(fp.tile_indices[i]);
    if (i + 1 < fp.tile_indices.size()) {
      os << ", ";
    }
  }
  os << "]";

  os << ", popularity=[";
  for (std::size_t i = 0; i < fp.popularity.size(); ++i) {
    os << static_cast<int>(fp.popularity[i]);
    if (i + 1 < fp.popularity.size()) {
      os << ", ";
    }
  }
  os << "]";

  os << ", price_per_tile=" << fp.price_per_tile.cents
     << ", quality=" << static_cast<int>(fp.quality) << " }";

  return os;
}
