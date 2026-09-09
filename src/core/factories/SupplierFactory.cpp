/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/factories/SupplierFactory.h"

#include "core/factories/DataFile.h"
#include "core/supply/Supplier.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::map<SupplierID, SupplierData> suppliers_;

SupplierData read_supplier(const data_file::json& entry, SupplierID id) {
  return SupplierData{
      .id = id,
      .type = data_file::enum_field<SupplierType>(entry, "type"),
      .quality = entry.at("quality").get<uint8_t>(),
      .name = entry.at("name").get<std::string>(),
      .price_per_tile = entry.at("price_per_tile").get<uint16_t>(),
      .customer_ratings =
          data_file::array_field<int8_t, 6>(entry, "customer_ratings"),
      .introduction_year = entry.at("introduction_year").get<uint16_t>(),
      .withdrawal_year = entry.at("withdrawal_year").get<uint16_t>(),
  };
}

void ensure_loaded() {
  if (!suppliers_.empty()) {
    return;
  }
  suppliers_ = data_file::load<SupplierData, SupplierID>("suppliers.json",
                                                         read_supplier);
}

} // namespace

std::unordered_map<SupplierID, std::unique_ptr<const Supplier>>
    SupplierFactory::cache_ = {};

std::vector<const Supplier*> SupplierFactory::all_suppliers() {
  ensure_loaded();
  std::vector<const Supplier*> list;
  list.reserve(suppliers_.size());
  for (const auto& [id, data] : suppliers_) {
    list.push_back(create_supplier(id));
  }

  // Sort by group first and then by enum order within
  std::sort(list.begin(), list.end(), [](const Supplier* a, const Supplier* b) {
    int tier_a = a->quality() >> 4;
    int tier_b = b->quality() >> 4;
    if (tier_a != tier_b) {
      return tier_a < tier_b;
    }
    return static_cast<int>(a->id()) < static_cast<int>(b->id());
  });

  return list;
}

std::vector<const Supplier*>
SupplierFactory::all_suppliers_by_type(SupplierType type, int current_year) {
  auto list = all_suppliers();
  auto filtered = list | std::ranges::views::filter(
                             [type, current_year](const Supplier*& s) {
                               return s && s->type() == type &&
                                      current_year >= s->introduction_year() &&
                                      current_year <= s->withdrawal_year();
                             });
  return std::vector<const Supplier*>(filtered.begin(), filtered.end());
}

const Supplier* SupplierFactory::create_supplier(SupplierID supplier_id) {
  auto it = cache_.find(supplier_id);
  if (it != cache_.end()) {
    return it->second.get();
  }

  ensure_loaded();
  const auto found = suppliers_.find(supplier_id);
  if (found == suppliers_.end()) {
    return nullptr;
  }
  auto [pos, _] =
      cache_.emplace(supplier_id, std::make_unique<Supplier>(found->second));
  return pos->second.get();
}
