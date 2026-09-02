/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <optional>
#include <ostream>
#include "core/Demographics.h"
#include "core/city/CityTypes.h"
#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/inventory/Inventory.h"
#include "core/state/GameState.h"
#include "engine/Size.h"

class City;
class Property;
class Restaurant;
class District;

std::ostream& operator<<(std::ostream& os, const Property& premise);

// A warehouse costs five times what the same floor area would as a shop, in
// both purchase price and rent.
constexpr int depot_price_multiplier = 5;

class Property {
public:
  Property(int index, PropertyType type, Size size, core::ActorId realtor_id,
           PremiseStatus, City& city);
  Size size() const;
  void size(Size size);
  // The kitchen and its doors are fixed when the city is built.
  Size kitchen_size() const;
  void kitchen_size(Size size);
  int entrance_offset() const;
  void entrance_offset(int offset);
  int kitchen_entry_offset() const;
  void kitchen_entry_offset(int offset);
  core::ActorId realtor_id() const;
  void realtor_id(core::ActorId realtor_id);
  PremiseStatus status() const;
  void status(PremiseStatus);
  City& city();
  const City& city() const;
  int index() const;
  PropertyType type() const;
  std::optional<core::PlayerId> owner_or_tenant() const;
  void owner_or_tenant(core::PlayerId player);
  void clear_owner();
  bool is_for_sale() const;
  bool is_for_rent() const;
  bool is_owned() const;
  bool is_rented() const;
  void rent(core::PlayerId player_id);
  void buy(core::PlayerId player_id);
  // Fixed when the city is built, then it moves: a failed haggle, a purchase
  // and a sale each write the agreed figure back.
  Money purchase_price() const;
  void purchase_price(Money price);
  Money get_monthly_rent() const;
  void calculate_demographics(const Restaurant* restaurant = nullptr);
  // The original fills the pool once and leaves it until the weekly
  // settlement, so a reader recomputes only an empty pool.
  void ensure_demographics(const Restaurant* restaurant = nullptr);

  // Potential customers - calculated by scanning surrounding tiles
  CustomerCounts& potential_customers() { return potential_customers_; }
  const CustomerCounts& potential_customers() const {
    return potential_customers_;
  }

  std::optional<core::RestaurantId> restaurant_id() const;
  void restaurant_id(core::RestaurantId r_id);
  void clear_restaurant();
  bool has_restaurant() const { return restaurant_id_.has_value(); };

  // Only a depot can have weapons
  core::inventory::Inventory* weapons() {
    return weapons_ ? &*weapons_ : nullptr;
  }
  const core::inventory::Inventory* weapons() const {
    return weapons_ ? &*weapons_ : nullptr;
  }
  const District& district() const;

private:
  float calculate_tile_population(uint8_t tile_type, int demographic_type,
                                  float district_percentage);

  const int index_;
  PropertyType type_;
  Size size_;
  Size kitchen_size_{};
  int entrance_offset_{1};
  int kitchen_entry_offset_{1};
  core::ActorId realtor_id_;
  PremiseStatus status_;
  City& city_;
  std::optional<core::PlayerId> owner_tenant_;
  std::optional<core::RestaurantId> restaurant_id_;
  CustomerCounts potential_customers_{};
  int total_population_{0};
  Money rent_price_{0};
  Money purchase_price_{0};
  std::optional<core::inventory::Inventory> weapons_;
};
