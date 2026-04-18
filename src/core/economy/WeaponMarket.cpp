/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/economy/WeaponMarket.h"
#include "engine/Random.h"

using core::item::Item;
using core::item::item_count; // compile-time constant

namespace {

// Pre-compute the factor table once at load time
constexpr std::array<int, item_count> base_factor = [] {
  std::array<int, item_count> f{};
  for (std::size_t i = 0; i < item_count; ++i) {
    f[i] = (i < 4) ? (static_cast<int>(i) + 1) * 400   // joke articles
                   : (static_cast<int>(i) - 3) * 1800; // weapons
  }
  return f;
}();

} // anonymous namespace

WeaponMarket::WeaponMarket() { init(); }

Money WeaponMarket::purchase_price(Item id) const noexcept {
  std::size_t idx = static_cast<std::size_t>(id);

  // pick the slot this item is priced from
  auto const& slot = slots_[idx % number_of_slots];

  int base = slot.value + 50;
  int price_raw = base * base_factor[idx];

  return Money{price_raw};
}

Money WeaponMarket::sell_price(Item id) const noexcept {
  Money buy = purchase_price(id);

  // 0 to 9:
  int rnd = engine::random::watcom_rand15() % 10;
  // 90 to 99% of buy price:
  return Money{buy * (90 + rnd) / 100}; // 90-99 % of buy price
}

// initialize this city's slots at game start
void WeaponMarket::init() {
  for (auto& s : slots_) {
    s.trend = engine::random::rand_percentage_chance(50)
                  ? PriceSlot::PriceTrend::UPWARDS
                  : PriceSlot::PriceTrend::DOWNWARDS;
    s.value = (engine::random::watcom_rand15() % 80) + 40;
  }
}
