/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/state/RealEstate.h"

#include <memory>
#include <string>
#include <utility>

#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/Property.h"
#include "core/finance/Ledger.h"
#include "core/finance/PlayerFinancials.h"
#include "core/restaurant/AIEconomy.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/RestaurantLayout.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"
#include "engine/Random.h"
#include "factories/RestaurantLayoutFactory.h"

namespace {

// Depot properties add storage capacity proportional to their footprint.
constexpr int depot_capacity_kg_per_tile = 100;

int depot_capacity_kg(const Property& property) {
  return property.size().w * property.size().h * depot_capacity_kg_per_tile;
}

} // namespace

bool RealEstate::buy_restaurant(Property& property, Player& buyer,
                                Money price) {
  GameDate today = gs_->time().current_date();

  // Property purchase is Capital, use transfer directly
  if (buyer.cash_balance() < price) {
    return false;
  }
  buyer.financials().transfer(today, Account::Cash, Account::Property, price,
                              AccountingScope::Capital);

  property.buy(buyer.id());
  // If the buyer was renting, a restaurant already exists
  if (!property.has_restaurant()) {
    create_restaurant(buyer, property);
    gs_->adjust_social_score(buyer, 50);
  }
  return true;
}

// When you're renting a property you pay the first month immediately,
// regardless of which day of the month it is. The lease is free
// (at least initial payment) if this is a quick start.
Restaurant* RealEstate::rent_restaurant(Property& property, Player& player,
                                        bool free) {
  Money price = property.get_monthly_rent();
  GameDate today = gs_->time().current_date();

  if (!free && player.cash_balance() < price) {
    return nullptr;
  }

  property.rent(player.id());
  auto* restaurant = create_restaurant(player, property);
  if (!restaurant) {
    return nullptr;
  }

  if (!free) {
    player.spend_cash(today, Account::Rents, price, restaurant->id());
    gs_->adjust_social_score(player, 50);
  }
  return restaurant;
}

bool RealEstate::buy_depot(Property& property, Player& buyer) {
  Money price = property.get_purchase_price();
  GameDate today = gs_->time().current_date();

  // Property purchase is Capital, use transfer directly
  if (buyer.cash_balance() < price) {
    return false;
  }
  buyer.financials().transfer(today, Account::Cash, Account::Property, price,
                              AccountingScope::Capital);

  property.buy(buyer.id());

  // Add depot capacity to player's central depot in this city
  auto& city = property.city();
  city.central_depot(buyer.id()).add_capacity(depot_capacity_kg(property));

  gs_->milestones().check_depot_milestones(buyer);
  gs_->adjust_social_score(buyer, 180);

  return true;
}

bool RealEstate::rent_depot(Property& property, Player& player, bool free) {
  Money price = property.get_monthly_rent();
  GameDate today = gs_->time().current_date();

  if (!free && !player.spend_cash(today, Account::Rents, price)) {
    return false;
  }
  property.rent(player.id());

  // Add depot capacity to player's central depot in this city
  auto& city = property.city();
  city.central_depot(player.id()).add_capacity(depot_capacity_kg(property));

  gs_->milestones().check_depot_milestones(player);

  return true;
}

std::vector<Property*> RealEstate::depots_for(core::PlayerId pid) const {
  std::vector<Property*> result;
  for (auto* city : gs_->initialized_cities()) {
    for (auto* prop : city->properties()) {
      if (prop->type() == PropertyType::DEPOT &&
          prop->owner_or_tenant() == pid) {
        result.push_back(prop);
      }
    }
  }
  return result;
}

Restaurant* RealEstate::create_restaurant(Player& player, Property& property) {
  // For now let's assume a random size between 12 and 20:
  int kitchen_w = engine::random::rand_max_n(8) + 12;
  int kitchen_h = engine::random::rand_max_n(8) + 12;
  auto restaurant_id = gs_->next_restaurant_id_++;
  core::CityId city_id = property.city().id();
  int in_city = gs_->restaurant_count_for_player_in_city(player.id(), city_id);
  auto restaurant_name = player.name() + " No. " + std::to_string(in_city + 1);
  property.restaurant_id(restaurant_id);

  auto restaurant = std::make_unique<Restaurant>(
      property, Size{kitchen_w, kitchen_h}, restaurant_id, restaurant_name);
  gs_->restaurants_.push_back(std::move(restaurant));
  auto* created = gs_->restaurants_.back().get();
  property.calculate_demographics(created);
  return created;
}

bool RealEstate::sell_property(Property& property, Player& seller,
                               Money sale_price) {
  if (!property.is_owned()) {
    return false;
  }

  auto owner = property.owner_or_tenant();
  if (!owner || *owner != seller.id()) {
    return false;
  }

  GameDate today = gs_->time().current_date();

  seller.financials().transfer(today, Account::Property, Account::Cash,
                               sale_price, AccountingScope::Capital);

  if (property.type() == PropertyType::DEPOT) {
    gs_->adjust_social_score(seller, -170);
  }
  else {
    gs_->adjust_social_score(seller, -50);
  }

  // If this is a depot, remove capacity from central depot
  if (property.type() == PropertyType::DEPOT) {
    auto& city = property.city();
    city.central_depot(seller.id())
        .remove_capacity(depot_capacity_kg(property));
  }

  if (property.has_restaurant()) {
    gs_->remove_restaurant(*property.restaurant_id());
  }

  property.clear_owner();
  property.status(PremiseStatus::NOT_AVAILABLE);
  return true;
}

bool RealEstate::end_property_lease(Property& property, Player& player) {
  if (!property.is_rented()) {
    return false;
  }

  auto owner = property.owner_or_tenant();
  if (!owner || *owner != player.id()) {
    return false;
  }

  // If this is a depot, remove capacity from central depot
  if (property.type() == PropertyType::DEPOT) {
    auto& city = property.city();
    city.central_depot(player.id())
        .remove_capacity(depot_capacity_kg(property));
  }

  if (property.has_restaurant()) {
    gs_->adjust_social_score(player, -50);
    gs_->remove_restaurant(*property.restaurant_id());
  }

  property.clear_owner();
  property.status(PremiseStatus::NOT_AVAILABLE);
  return true;
}

Restaurant* RealEstate::create_ai_starting_restaurant(Player& player,
                                                      City& city) {
  constexpr int max_attempts = 8;
  const uint8_t courage = player.character().courage();

  for (int i = 0; i < max_attempts; ++i) {
    if (!engine::random::rand_percentage_chance(courage)) {
      continue;
    }
    // The AI furnishes from its own prebuilt restaurants, which carry their
    // own size.
    const int layout_index =
        engine::random::rand_max_n(RestaurantLayoutFactory::ai_layout_count) +
        1;
    auto* rest = create_restaurant_with_layout(
        player, city, /*layout_id=*/0, /*is_big=*/false,
        RestaurantLayoutFactory::create_ai_layout(layout_index,
                                                  ai::opening_furniture_count));
    if (rest) {
      rest->ai_layout(layout_index, ai::opening_furniture_count);
      rest->open_for_business(*gs_);
    }
    return rest;
  }
  return nullptr;
}

Restaurant* RealEstate::create_restaurant_with_layout(
    Player& player, City& city, int layout_id, bool is_big,
    std::unique_ptr<RestaurantLayout> layout) {
  if (!layout) {
    layout = RestaurantLayoutFactory::create_layout(
        compat::format("q/f{}.pc", layout_id));
  }

  if (!layout) {
    return nullptr;
  }

  // Get all business premises in the city
  auto properties = city.business_premises();
  if (properties.empty()) {
    return nullptr;
  }

  // Try to find an available property (not already a restaurant) up to 10 times
  Property* available_property = nullptr;
  const int max_attempts = 10;
  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    auto try_index = engine::random::rand_max_n(properties.size());
    auto* candidate = properties.at(try_index);

    // Check if this property is available (no owner/tenant or no restaurant)
    if (!candidate->has_restaurant()) {
      available_property = candidate;
      break;
    }
  }

  // If we couldn't find an available property after 10 attempts, give up
  if (!available_property) {
    return nullptr;
  }

  available_property->size(layout->restaurant_size());

  // Rent the property (first month free for quickstart)
  auto* rest = rent_restaurant(*available_property, player,
                               /*free=*/true);

  if (rest) {
    if (player.is_human_controlled()) {
      // Apply financial transactions for furniture and ovens
      player.financials().transfer(
          gs_->time().current_date(), Account::Capital, Account::Furniture,
          layout->furniture_value() / 2, AccountingScope::Capital);
      player.financials().transfer(gs_->time().current_date(), Account::Capital,
                                   Account::Machines, layout->oven_value() / 2,
                                   AccountingScope::Capital);
    }

    rest->kitchen_size(layout->kitchen_size());
    rest->layout(std::move(layout));

    // AI restaurants need some members initialized from their character stats
    if (!player.is_human_controlled()) {
      rest->init_ai_economy(player);
    }
  }

  return rest;
}
