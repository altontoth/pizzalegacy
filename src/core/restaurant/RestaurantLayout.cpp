/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/RestaurantLayout.h"

#include <algorithm>
#include <cassert>

RestaurantLayout::RestaurantLayout(Size rest_size, Size kitchen_size,
                                   int entrance_offset,
                                   int kitchen_entry_offset)
    : restaurant_size_(rest_size), kitchen_size_(kitchen_size),
      entrance_offset_(entrance_offset),
      kitchen_entry_offset_(kitchen_entry_offset) {}

void RestaurantLayout::place_furniture(std::unique_ptr<PlacedFurniture> pf) {
  placed_furniture_.push_back(std::move(pf));
}

void RestaurantLayout::place_kitchen_furniture(
    std::unique_ptr<PlacedFurniture> pf) {
  placed_kitchen_furniture_.push_back(std::move(pf));
}

const std::vector<std::unique_ptr<PlacedFurniture>>&
RestaurantLayout::placed_furniture() const {
  return placed_furniture_;
}

const std::vector<std::unique_ptr<PlacedFurniture>>&
RestaurantLayout::placed_kitchen_furniture() const {
  return placed_kitchen_furniture_;
}

void RestaurantLayout::floor_pattern(FloorPattern floor_pattern) {
  floor_pattern_ = floor_pattern;
}

FloorPattern RestaurantLayout::floor_pattern() const { return floor_pattern_; }

std::unique_ptr<PlacedFurniture>
RestaurantLayout::remove_by_id(core::FurnitureId id, bool kitchen_furniture) {
  auto& f_list =
      kitchen_furniture ? placed_kitchen_furniture_ : placed_furniture_;

  auto it = std::ranges::find_if(
      f_list, [id](const std::unique_ptr<PlacedFurniture>& pf) {
        return pf->id() == id;
      });

  if (it != f_list.end()) {
    auto removed = std::move(*it); // move out the unique_ptr
    f_list.erase(it);              // erase from the right vector
    return removed;                // hand ownership to caller
  }
  return nullptr;
}

Money RestaurantLayout::furniture_value() const {
  Money value = 0_usd;
  for (const auto& pf : placed_furniture_) {
    value += pf->furniture()->price();
  }
  return value;
}

Money RestaurantLayout::oven_value() const {
  Money value = 0_usd;
  for (const auto& pf : placed_kitchen_furniture_) {
    value += pf->furniture()->price();
  }
  return value;
}

PlacedFurniture* RestaurantLayout::find_by_id(core::FurnitureId id,
                                              bool kitchen) {
  auto& vec = kitchen ? placed_kitchen_furniture_ : placed_furniture_;
  for (auto& pf : vec) {
    if (pf->id() == id) {
      return pf.get();
    }
  }
  return nullptr;
}

const PlacedFurniture* RestaurantLayout::find_by_id(core::FurnitureId id,
                                                    bool kitchen) const {
  const auto& vec = kitchen ? placed_kitchen_furniture_ : placed_furniture_;
  for (const auto& pf : vec) {
    if (pf->id() == id) {
      return pf.get();
    }
  }
  return nullptr;
}

std::optional<int> RestaurantLayout::max_furniture_id_value() const {
  int max_id = -1;
  auto scan = [&](const auto& vec) {
    for (const auto& up : vec) {
      max_id = std::max(max_id, up->id().value);
    }
  };
  scan(placed_furniture_);
  scan(placed_kitchen_furniture_);
  if (max_id == -1) {
    return std::nullopt;
  }
  return max_id;
}

core::FurnitureId RestaurantLayout::next_furniture_id() {
  auto max_id = max_furniture_id_value();
  next_furniture_id_ =
      max_id ? core::FurnitureId{max_id.value() + 1} : core::FurnitureId{0};
  return next_furniture_id_;
}

int RestaurantLayout::entrance_offset() const { return entrance_offset_; }

int RestaurantLayout::kitchen_entry_offset() const {
  return kitchen_entry_offset_;
}

void RestaurantLayout::clear() {
  placed_furniture_.clear();
  placed_kitchen_furniture_.clear();
}

int RestaurantLayout::linked_furniture_count(core::FurnitureId id) const {
  int count = 0;
  for (const auto& linked : placed_furniture_) {
    if (linked->linked_furniture_id() == id) {
      count++;
    }
  }
  return count;
}
