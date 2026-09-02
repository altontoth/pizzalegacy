/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/Demographics.h"
#include "core/finance/Money.h"
#include "core/restaurant/RestaurantTypes.h"

enum class FurnitureCategory {
  OTHER = 0,
  TABLE,
  CHAIR,
  DECOR,
  ENTERTAINMENT,
  GAMES,
  SLOT_MACHINES,
  UNKNOWN,
  KITCHENWARE,
  UNKNOWN3,
};

constexpr std::string_view to_string(FurnitureCategory category) {
  switch (category) {
  case FurnitureCategory::OTHER:
    return "OTHER";
  case FurnitureCategory::TABLE:
    return "TABLE";
  case FurnitureCategory::CHAIR:
    return "CHAIR";
  case FurnitureCategory::DECOR:
    return "DECOR";
  case FurnitureCategory::ENTERTAINMENT:
    return "ENTERTAINMENT";
  case FurnitureCategory::GAMES:
    return "GAMES";
  case FurnitureCategory::SLOT_MACHINES:
    return "SLOT_MACHINES";
  case FurnitureCategory::UNKNOWN:
    return "UNKNOWN";
  case FurnitureCategory::KITCHENWARE:
    return "KITCHENWARE";
  case FurnitureCategory::UNKNOWN3:
    return "UNKNOWN3";
  }
  return "INVALID_FurnitureCategory";
}

enum class PlacementLayer : uint8_t {
  Seating = 1,
  Tables = 2,
  Top = 3,
};

struct FurnitureData {
  FurnitureType type;
  int u1; // unknown
  int quality;
  int supplier;
  std::string_view name;
  int price;
  std::array<int, 4> bobs;
  FurnitureCategory category;
  int u2; // unknown
  int capacity;
  int bake_time;
  PlacementLayer placement_layer;
  std::array<int, static_cast<std::size_t>(ConsumerType::Count)> popularity;
};

class Furniture {
public:
  explicit Furniture(const FurnitureData& data);

  FurnitureType type() const { return type_; }
  int u1() const { return u1_; }
  int quality() const { return quality_; }
  int supplier() const { return supplier_; }
  const std::string& name() const { return name_; }
  Money price() const { return Money::from_dollars(price_); }
  const std::vector<int>& bobs() const { return bobs_; }
  FurnitureCategory category() const { return category_; }
  int u2() const { return u2_; }
  int capacity() const { return capacity_; }
  int bake_time() const { return bake_time_; }
  PlacementLayer placement_layer() const { return placement_layer_; }
  int popularity(ConsumerType consumer) const;

  void type(FurnitureType type) { type_ = type; }
  void u1(int u1) { u1_ = u1; }
  void quality(int quality) { quality_ = quality; }
  void supplier(int supplier) { supplier_ = supplier; }
  void name(const std::string& name) { name_ = name; }
  void price(int price) { price_ = price; }
  void bobs(const std::vector<int>& bobs) { bobs_ = bobs; }
  void category(FurnitureCategory category) { category_ = category; }
  void u2(int u2) { u2_ = u2; }
  void capacity(int capacity) { capacity_ = capacity; }
  void bake_time(int bake_time) { bake_time_ = bake_time; }
  void placement_layer(PlacementLayer placement_layer) {
    placement_layer_ = placement_layer;
  }
  void popularity(ConsumerType consumer, int value);

private:
  FurnitureType type_;
  int u1_;
  int quality_;
  int supplier_;
  std::string name_;
  int price_;
  std::vector<int> bobs_; // IDs for graphics
  FurnitureCategory category_;
  int u2_;
  int capacity_;
  int bake_time_;
  PlacementLayer placement_layer_;
  std::array<int, static_cast<std::size_t>(ConsumerType::Count)>
      popularity_; // o1 to o6
};
