/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/restaurant/Furniture.h"
#include "core/restaurant/RestaurantTypes.h"
#include "core/supply/Supplier.h"

#include <memory>
#include <optional>
#include <unordered_map>

class FurnitureFactory {
public:
  static const Furniture* create_furniture(FurnitureType type);
  static std::vector<const Furniture*> all_furniture();
  static std::vector<const Furniture*> all_furniture_by_supplier(SupplierID);
  static std::optional<FurnitureType> get_furniture_type(const Furniture* item);

private:
  static std::unordered_map<FurnitureType, std::unique_ptr<const Furniture>>
      cache_;
};
