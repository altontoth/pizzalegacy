/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/economy/WeaponMarket.h"
#include "engine/Random.h"

#include <cassert>

using core::item::Item;
using core::item::item_count; // compile-time constant

namespace {

// Price before the per-city modifier is applied.
constexpr std::array<Money, item_count> base_price = [] {
  std::array<Money, item_count> f{};
  for (std::size_t i = 0; i < item_count; ++i) {
    const int64_t dollars = (i < 4) ? (static_cast<int64_t>(i) + 1) * 400
                                    : (static_cast<int64_t>(i) - 2) * 1800;
    f[i] = Money{dollars * 100};
  }
  return f;
}();

// Each city has four price modifiers, and they drift from day to day. Adding
// this constant to one of these modifiers gives us the percentage of base
// price the item sells at. If the modifier were 0 we would sell at 50 percent
// of base, but since the modifiers stay between 5 and 124, our prices run from
// 5+50=55 percent to 124+50=174 percent.
constexpr int modifier_offset = 50;

// The direction reverses after a step has taken the walk past one of these,
// so the modifier can sit up to one step outside them.
constexpr int turn_around_below = 10;
constexpr int turn_around_above = 120;

// Chance per day that a trend reverses
constexpr int spontaneous_reversal_chance = 4;

constexpr int max_step = 5;

} // namespace

WeaponMarket::WeaponMarket() { init(); }

std::size_t WeaponMarket::index(core::CityId city, int column) {
  assert(city.value >= 0 && city.value < NUMBER_OF_CITIES);
  assert(column >= 0 && column < columns_per_city);
  return (static_cast<std::size_t>(city.value) * columns_per_city) +
         static_cast<std::size_t>(column);
}

Money WeaponMarket::purchase_price(core::CityId city, Item id) const noexcept {
  const int64_t base = base_price[static_cast<std::size_t>(id)].whole();
  int column = static_cast<int>(id) % columns_per_city;

  // Truncated to whole dollars here, so that buying a quantity charges a
  // multiple of the quoted price.
  auto dollars =
      base * (slots_[index(city, column)].modifier + modifier_offset) / 100;

  return Money{dollars * 100};
}

Money WeaponMarket::sell_price(core::CityId city, Item id) const noexcept {
  // 90 to 99% of the purchase price. Rolled once per sale, so every piece in
  // one transaction goes at the same price.
  int percent = 90 + (engine::random::ansi_rand15() % 10);

  int64_t dollars = purchase_price(city, id).whole() * percent / 100;

  return Money{dollars * 100};
}

int WeaponMarket::modifier(core::CityId city, int column) const {
  return slots_[index(city, column)].modifier;
}

bool WeaponMarket::rising(core::CityId city, int column) const {
  return slots_[index(city, column)].rising;
}

void WeaponMarket::set_slot(core::CityId city, int column, int modifier,
                            bool rising) {
  auto& s = slots_[index(city, column)];
  s.modifier = modifier;
  s.rising = rising;
}

void WeaponMarket::init() {
  for (auto& s : slots_) {
    s.rising = engine::random::rand_percentage_chance(50);
    s.modifier = (engine::random::ansi_rand15() % 80) + 40;
  }
}

void WeaponMarket::daily_update() {
  for (auto& s : slots_) {
    if (s.rising) {
      s.modifier += engine::random::rand_max_n(max_step);
      if (s.modifier > turn_around_above ||
          engine::random::rand_percentage_chance(spontaneous_reversal_chance)) {
        s.rising = false;
      }
    }
    else {
      s.modifier -= engine::random::rand_max_n(max_step) + 1;
      if (s.modifier < turn_around_below ||
          engine::random::rand_percentage_chance(spontaneous_reversal_chance)) {
        s.rising = true;
      }
    }
  }
}
