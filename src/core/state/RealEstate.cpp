/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/state/RealEstate.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "core/character/Player.h"
#include "core/character/Staff.h"
#include "core/city/City.h"
#include "core/city/JobMarket.h"
#include "core/city/MayorOffice.h"
#include "core/city/Property.h"
#include "core/factories/RestaurantLayoutFactory.h"
#include "core/finance/Ledger.h"
#include "core/finance/Money.h"
#include "core/finance/PlayerFinancials.h"
#include "core/restaurant/AIEconomy.h"
#include "core/restaurant/Furniture.h"
#include "core/restaurant/PlacedFurniture.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/RestaurantLayout.h"
#include "core/state/GameState.h"
#include "core/status/StatusHub.h"
#include "engine/FormatCompat.h"
#include "engine/Random.h"

namespace {

void add_investment(Property& property, const Player& player, Money amount) {
  if (player.is_human_controlled()) {
    property.city().mayor_office().add_investment_activity(player.id().value,
                                                           amount);
  }
}

// Depot properties add storage capacity proportional to their footprint.
constexpr int depot_capacity_kg_per_tile = 100;

int depot_capacity_kg(const Property& property) {
  return property.size().w * property.size().h * depot_capacity_kg_per_tile;
}

// A premise bigger than this counts as large, and uses a different set of
// layouts.
constexpr int large_premise_area = 240;

// A demographic pair has to draw more than this share of a premise's customers
// before its owner picks a layout to suit them.
constexpr int dominant_pair_percent = 40;

// An owner needs more intelligence than this to think of it at all.
constexpr int demographic_bias_intelligence = 40;

// Which demographic pair an AI furnishes for: 0 children and young people,
// 1 blue and white collar, 2 business people and seniors. The 18 AI restaurant
// layouts are three styles by these three pairs by two sizes, so the bias
// decides what the place looks like. Only the look: an AI's furniture appeal is
// decided by its owner's intelligence at creation.
int ai_demographic_bias(Property& property, int intelligence) {
  int bias = engine::random::rand_max_n(3);

  property.ensure_demographics();

  int total = 0;
  for (int count : property.potential_customers().data()) {
    total += count;
  }
  if (total == 0 || intelligence <= demographic_bias_intelligence) {
    return bias;
  }

  // When more than one pair matches, last one wins.
  const auto& counts = property.potential_customers().data();
  for (std::size_t pair = 0; pair < 3; ++pair) {
    const int share =
        ((counts[pair * 2] + counts[(pair * 2) + 1]) * 100) / total;
    if (share > dominant_pair_percent) {
      bias = static_cast<int>(pair);
    }
  }
  return bias;
}

// Which prebuilt AI restaurant a premise gets. This never comes to more than
// 18, so the last few layout files never come up.
int ai_layout_index(const Property& property, int bias) {
  const bool large = property.size().w * property.size().h > large_premise_area;
  return (engine::random::rand_max_n(3) * 6) + 1 + (bias * 2) + (large ? 1 : 0);
}

} // namespace

bool RealEstate::buy_restaurant(Property& property, Player& buyer, Money price,
                                bool free) {
  GameDate today = gs_->time().current_date();

  // Property purchase is Capital, use transfer directly
  if (!free) {
    if (buyer.cash_balance() < price) {
      return false;
    }
    buyer.financials().transfer(today, Account::Cash, Account::Property, price,
                                balance_sheet_only);
    add_investment(property, buyer, price);
  }

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
    // We already check player has enough money, so this can't be false.
    (void)player.spend_cash(today, MonthlyCategory::Rents, price,
                            restaurant->id());
    add_investment(property, player, price);
    gs_->adjust_social_score(player, 50);
  }
  return restaurant;
}

bool RealEstate::buy_depot(Property& property, Player& buyer, Money price) {
  GameDate today = gs_->time().current_date();

  // Property purchase is Capital, use transfer directly
  if (buyer.cash_balance() < price) {
    return false;
  }
  buyer.financials().transfer(today, Account::Cash, Account::Property, price,
                              balance_sheet_only);
  add_investment(property, buyer, price);

  // A tenant buying the depot they rent already has its capacity and score.
  const bool was_tenant =
      property.is_rented() && property.owner_or_tenant() == buyer.id();
  property.buy(buyer.id());

  if (!was_tenant) {
    // Add depot capacity to player's central depot in this city
    auto& city = property.city();
    city.central_depot(buyer.id()).add_capacity(depot_capacity_kg(property));
    gs_->adjust_social_score(buyer, 180);
  }

  gs_->milestones().check_depot_milestones(buyer);

  return true;
}

bool RealEstate::rent_depot(Property& property, Player& player, bool free) {
  Money price = property.get_monthly_rent();
  GameDate today = gs_->time().current_date();

  if (!free && !player.spend_cash(today, MonthlyCategory::Rents, price)) {
    return false;
  }
  if (!free) {
    add_investment(property, player, price);
  }
  property.rent(player.id());

  // Add depot capacity to player's central depot in this city
  auto& city = property.city();
  city.central_depot(player.id()).add_capacity(depot_capacity_kg(property));

  gs_->milestones().check_depot_milestones(player);
  gs_->adjust_social_score(player, 180);

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
  auto restaurant_id = gs_->next_restaurant_id_++;
  core::CityId city_id = property.city().id();
  int in_city = gs_->restaurant_count_for_player_in_city(player.id(), city_id);
  auto restaurant_name = player.name() + " No. " + std::to_string(in_city + 1);
  property.restaurant_id(restaurant_id);

  auto restaurant = std::make_unique<Restaurant>(
      property, property.kitchen_size(), restaurant_id, restaurant_name);
  gs_->restaurants_.push_back(std::move(restaurant));
  auto* created = gs_->restaurants_.back().get();
  property.calculate_demographics(created);
  gs_->milestones().check_restaurant_acquired(player);
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
                               sale_price, balance_sheet_only);

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
    close_restaurant(seller, property);
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
    close_restaurant(player, property);
  }

  property.clear_owner();
  property.status(PremiseStatus::NOT_AVAILABLE);
  return true;
}

void RealEstate::close_restaurant(Player& player, Property& property) {
  const auto restaurant_id = *property.restaurant_id();
  auto* restaurant = gs_->restaurant(restaurant_id);
  if (restaurant && player.is_human_controlled()) {
    // Closing the restaurant means firing the entire staff.
    auto& city = property.city();
    auto fire_staff = [&](const Staff& staff) {
      uint8_t char_id = staff.character().id();
      city.blacklist_staff(char_id, player.id());
      city.job_market().set_status(
          char_id,
          static_cast<core::character::MarketStatus>(staff.job_role()));
    };
    for (const auto& cook : restaurant->cooks()) {
      if (cook.has_value()) {
        fire_staff(*cook);
      }
    }
    for (const auto& waiter : restaurant->waiters()) {
      if (waiter.has_value()) {
        fire_staff(*waiter);
      }
    }
    if (restaurant->manager().has_value()) {
      fire_staff(*restaurant->manager());
    }

    player.cancel_restaurant_contracts(*gs_, restaurant_id);

    Money furniture_amount;
    Money oven_amount;
    for (const auto& item : restaurant->layout()->placed_furniture()) {
      furniture_amount += item->furniture()->price() / 2;
    }
    for (const auto& item : restaurant->layout()->placed_kitchen_furniture()) {
      oven_amount += item->furniture()->price() / 2;
    }

    if (furniture_amount > 0_usd || oven_amount > 0_usd) {
      gs_->status().post_direct({
          .payload = compat::format(
              "Articles to a total of {:w}, were left in your restaurant. "
              "The scrapdealer is happy now!!",
              furniture_amount + oven_amount),
          .scope = StatusScope::Global,
      });

      auto date = gs_->time().current_date();
      if (furniture_amount > 0_usd) {
        player.financials().transfer(date, Account::Furniture,
                                     Account::ProfitAndLoss, furniture_amount,
                                     balance_sheet_only);
      }
      if (oven_amount > 0_usd) {
        player.financials().transfer(date, Account::Machines,
                                     Account::ProfitAndLoss, oven_amount,
                                     balance_sheet_only);
      }
    }
  }
  gs_->remove_restaurant(property);
}

Restaurant* RealEstate::create_ai_starting_restaurant(Player& player,
                                                      City& city) {
  constexpr int max_attempts = 8;
  const uint8_t experience = player.character().experience();

  for (int i = 0; i < max_attempts; ++i) {
    if (!engine::random::rand_percentage_chance(experience)) {
      continue;
    }
    Property* property = find_available_premise(city);
    if (!property) {
      return nullptr;
    }
    auto* rest = create_ai_restaurant(player, *property, Tenure::Rent);
    if (rest) {
      rest->open_for_business(*gs_);
    }
    return rest;
  }
  return nullptr;
}

Restaurant* RealEstate::create_ai_restaurant(Player& player, Property& property,
                                             Tenure tenure) {
  const int bias =
      ai_demographic_bias(property, player.character().intelligence());
  const int layout_index = ai_layout_index(property, bias);

  auto* rest =
      furnish_new_restaurant(player, property,
                             RestaurantLayoutFactory::create_ai_layout(
                                 layout_index, ai::opening_furniture_count),
                             tenure);
  if (rest) {
    rest->ai_layout(layout_index, ai::opening_furniture_count);
    ai::grow_customers_per_day(*rest);
  }
  return rest;
}

Restaurant* RealEstate::create_restaurant_with_layout(Player& player,
                                                      City& city,
                                                      int layout_id) {
  auto layout = RestaurantLayoutFactory::create_layout(
      compat::format("q/f{}.pc", layout_id));
  if (!layout) {
    return nullptr;
  }

  Property* property = find_available_premise(city);
  if (!property) {
    return nullptr;
  }

  return furnish_new_restaurant(player, *property, std::move(layout),
                                Tenure::RentFirstMonthFree);
}

Property* RealEstate::find_available_premise(City& city) {
  auto properties = city.business_premises();
  if (properties.empty()) {
    return nullptr;
  }

  // Try to find an available property (not already a restaurant) up to 10 times
  const int max_attempts = 10;
  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    auto try_index =
        engine::random::rand_max_n(static_cast<int>(properties.size()));
    auto* candidate = properties.at(try_index);

    // Check if this property is available (no owner/tenant or no restaurant)
    if (!candidate->has_restaurant()) {
      return candidate;
    }
  }

  // If we couldn't find an available property after 10 attempts, give up
  return nullptr;
}

Restaurant*
RealEstate::furnish_new_restaurant(Player& player, Property& property,
                                   std::unique_ptr<RestaurantLayout> layout,
                                   Tenure tenure) {
  if (!layout) {
    return nullptr;
  }

  property.size(layout->restaurant_size());

  // An AI takes the premise whether or not it can afford it, and just doesn't
  // pay when it can't.
  Restaurant* rest = nullptr;
  if (tenure == Tenure::Buy) {
    const Money price = property.purchase_price();
    if (buy_restaurant(property, player, price,
                       /*free=*/player.cash_balance() < price) &&
        property.restaurant_id()) {
      rest = gs_->restaurant(*property.restaurant_id());
    }
  }
  else {
    const bool free = tenure == Tenure::RentFirstMonthFree ||
                      player.cash_balance() < property.get_monthly_rent();
    rest = rent_restaurant(property, player, free);
  }

  if (rest) {
    if (player.is_human_controlled()) {
      // Apply financial transactions for furniture and ovens
      player.financials().transfer(
          gs_->time().current_date(), Account::Capital, Account::Furniture,
          layout->furniture_value() / 2, balance_sheet_only);
      player.financials().transfer(gs_->time().current_date(), Account::Capital,
                                   Account::Machines, layout->oven_value() / 2,
                                   balance_sheet_only);
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
