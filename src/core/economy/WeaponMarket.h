/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/finance/Money.h"

#include <array>
#include "core/item/Item.h"

class WeaponMarket {
public:
  WeaponMarket();
  Money purchase_price(core::item::Item id) const noexcept;
  Money sell_price(core::item::Item id) const noexcept;
  void init();

private:
  struct PriceSlot {
    enum class PriceTrend {
      UPWARDS,
      DOWNWARDS,
    };
    PriceTrend trend;
    int value;
  };

  static constexpr std::size_t number_of_slots = 4;
  std::array<PriceSlot, number_of_slots> slots_{};
};
