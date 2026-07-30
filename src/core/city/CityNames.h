/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <string_view>
#include "core/common/Ids.h"
#include "core/util/Constants.h"

namespace core {

// City ids index the original data files: c0 is Paris, c9 is London. The
// English names here replace the German ones stored in those files.
inline constexpr std::array<std::string_view, NUMBER_OF_CITIES> city_names = {
    "Paris",    "Berlin", "Baltimore", "Vienna",    "Chicago",
    "New York", "Madrid", "Rome",      "Stockholm", "London",
};

inline std::string_view city_name(CityId id) {
  if (id.value < 0 || id.value >= NUMBER_OF_CITIES) {
    return "Unknown";
  }
  return city_names[static_cast<std::size_t>(id.value)];
}

} // namespace core
