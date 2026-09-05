/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <vector>
#include "core/restaurant/FloorPattern.h"
#include "core/restaurant/PlacedFurniture.h"
#include "engine/Size.h"

class RestaurantLayout {
public:
  RestaurantLayout(Size rest_size, Size kitchen_size, int entrance_offset = 1,
                   int kitchen_entry_offset = 1);
  void place_furniture(std::unique_ptr<PlacedFurniture>);
  void place_kitchen_furniture(std::unique_ptr<PlacedFurniture>);
  const std::vector<std::unique_ptr<PlacedFurniture>>& placed_furniture() const;
  const std::vector<std::unique_ptr<PlacedFurniture>>&
  placed_kitchen_furniture() const;
  Size restaurant_size() const { return restaurant_size_; };
  Size kitchen_size() const { return kitchen_size_; };
  void floor_pattern(FloorPattern);
  FloorPattern floor_pattern() const;
  std::unique_ptr<PlacedFurniture> remove_by_id(core::FurnitureId id,
                                                bool kitchen_furniture = false);
  Money furniture_value() const;
  Money oven_value() const;
  core::FurnitureId next_furniture_id();
  PlacedFurniture* find_by_id(core::FurnitureId id, bool kitchen);
  const PlacedFurniture* find_by_id(core::FurnitureId id, bool kitchen) const;
  std::optional<int> max_furniture_id_value() const;
  int entrance_offset() const;
  int kitchen_entry_offset() const;
  void clear();
  // Count of items linked to this one
  int linked_furniture_count(core::FurnitureId id) const;
  // Chairs standing at a table; one guest each.
  int seated_chair_count() const;

private:
  Size restaurant_size_;
  Size kitchen_size_;
  std::vector<std::unique_ptr<PlacedFurniture>> placed_furniture_;
  std::vector<std::unique_ptr<PlacedFurniture>> placed_kitchen_furniture_;
  FloorPattern floor_pattern_{.vendor_id = 12,
                              .tile_indices = {4, 5, 6, 7},
                              // Default popularity for the 'no floor' floor
                              .popularity = {-10, -8, -20, -20, -30, -25},
                              .popularity_factor = 0};
  core::FurnitureId next_furniture_id_;
  int entrance_offset_{1};
  int kitchen_entry_offset_{1};
};
