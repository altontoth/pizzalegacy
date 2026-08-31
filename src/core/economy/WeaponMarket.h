/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/item/Item.h"
#include "core/util/Constants.h"

#include <array>
#include <cstddef>

// Underworld prices for every city at once, drifting daily. Cities are priced
// whether or not the player has ever entered them, so a price tip can quote a
// city sight unseen and buying low in one city to sell high in another is a
// viable trade.
class WeaponMarket {
public:
  // Items sharing a column move together: a sabotage good and the weapon three
  // slots above it rise and fall as one, differing only in base price.
  static constexpr int columns_per_city = 4;

  WeaponMarket();

  Money purchase_price(core::CityId city, core::item::Item id) const noexcept;
  Money sell_price(core::CityId city, core::item::Item id) const noexcept;

  void init();
  void daily_update();

  // Percentage offset applied to an item's base price, and the direction it is
  // currently walking in.
  int modifier(core::CityId city, int column) const;
  bool rising(core::CityId city, int column) const;
  void set_slot(core::CityId city, int column, int modifier, bool rising);

private:
  struct PriceSlot {
    bool rising;
    int modifier;
  };

  static constexpr std::size_t slot_count =
      static_cast<std::size_t>(NUMBER_OF_CITIES) * columns_per_city;

  static std::size_t index(core::CityId city, int column);

  std::array<PriceSlot, slot_count> slots_{};
};
