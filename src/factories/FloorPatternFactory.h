/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/restaurant/FloorPattern.h"
#include "core/supply/Supplier.h"

class FloorPatternFactory {
public:
  static const std::vector<FloorPattern>& create(SupplierID sid);

  static void preload_from_path(const std::string& kachinfo_path);

private:
  static void ensure_loaded();
  static void parse_and_index(std::span<const uint8_t> bytes);

  static std::unordered_map<SupplierID, std::vector<FloorPattern>> by_supplier_;
  static std::string kachinfo_path_;
};
