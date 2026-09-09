/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/factories/FurnitureFactory.h"

#include "core/Demographics.h"
#include "core/factories/DataFile.h"
#include "core/restaurant/Furniture.h"
#include "core/restaurant/RestaurantTypes.h"
#include "core/supply/Supplier.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::map<FurnitureType, FurnitureData> furniture_;

FurnitureData read_furniture(const data_file::json& entry, FurnitureType type) {
  return FurnitureData{
      .type = type,
      .u1 = entry.at("u1").get<int>(),
      .quality = entry.at("quality").get<int>(),
      .supplier = entry.at("supplier").get<int>(),
      .name = entry.at("name").get<std::string>(),
      .price = entry.at("price").get<int>(),
      .bobs = data_file::array_field<int, 4>(entry, "bobs"),
      .category = data_file::enum_field<FurnitureCategory>(entry, "category"),
      .u2 = entry.at("u2").get<int>(),
      .capacity = entry.at("capacity").get<int>(),
      .bake_time = entry.at("bake_time").get<int>(),
      .placement_layer =
          data_file::enum_field<PlacementLayer>(entry, "placement_layer"),
      .popularity = data_file::array_field<int, static_cast<std::size_t>(
                                                    ConsumerType::Count)>(
          entry, "popularity"),
      .role = data_file::enum_field<FurnitureRole>(entry, "role",
                                                   FurnitureRole::None),
  };
}

void ensure_loaded() {
  if (!furniture_.empty()) {
    return;
  }
  furniture_ = data_file::load<FurnitureData, FurnitureType>("furniture.json",
                                                             read_furniture);
}

} // namespace

std::unordered_map<FurnitureType, std::unique_ptr<const Furniture>>
    FurnitureFactory::cache_ = {};

std::vector<const Furniture*> FurnitureFactory::all_furniture() {
  ensure_loaded();
  std::vector<const Furniture*> list;
  list.reserve(furniture_.size());
  for (const auto& [type, data] : furniture_) {
    list.push_back(create_furniture(type));
  }
  return list;
}

std::vector<const Furniture*>
FurnitureFactory::all_furniture_by_supplier(SupplierID supplier) {

  auto suppliers = all_furniture();
  auto filtered =
      suppliers | std::ranges::views::filter([supplier](const Furniture* f) {
        return f && f->supplier() == static_cast<int>(supplier);
      });
  return std::vector<const Furniture*>(filtered.begin(), filtered.end());
}

const Furniture* FurnitureFactory::furniture_with_role(FurnitureRole role) {
  ensure_loaded();
  for (const auto& [type, data] : furniture_) {
    if (data.role == role) {
      return create_furniture(type);
    }
  }
  return nullptr;
}

std::optional<FurnitureType>
FurnitureFactory::get_furniture_type(const Furniture* item) {
  if (!item) {
    return std::nullopt;
  }
  return item->type();
}

const Furniture*
FurnitureFactory::create_furniture(FurnitureType furniture_type) {
  auto it = cache_.find(furniture_type);
  if (it != cache_.end()) {
    return it->second.get();
  }

  ensure_loaded();
  const auto found = furniture_.find(furniture_type);
  if (found == furniture_.end()) {
    return nullptr;
  }
  auto [pos, _] = cache_.emplace(furniture_type,
                                 std::make_unique<Furniture>(found->second));
  return pos->second.get();
}
