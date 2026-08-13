/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <nlohmann/json.hpp>

#include "core/time/GameDate.h"

namespace save {

using json = nlohmann::json;

inline json date_to_json(const GameDate& d) {
  return {{"year", d.year()}, {"month", d.month()}, {"day", d.day()}};
}

inline GameDate json_to_date(const json& j) {
  return GameDate{j["year"].get<int>(), j["month"].get<int>(),
                  j["day"].get<int>()};
}

} // namespace save
