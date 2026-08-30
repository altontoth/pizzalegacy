/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cassert>
#include <ostream>
#include "core/item/Item.h"

/* Player inventory, what they walk around with; weapons and attack items */
namespace core::inventory {

struct Inventory {
  std::array<int, item::item_count> qty{};
  const int max_per_item_ = 255;

  int& operator[](item::Item i) noexcept;
  int operator[](item::Item i) const noexcept;

  int count(item::Item id) const noexcept;

  // Try to add `amount` units. Returns the number of items actually
  // added (given max_per_item_)
  int add(item::Item id, int amount = 1) noexcept;

  // Try to remove `amount` units. Returns the number of items actually
  // removed. Game actually doesn't allow you to sell 20 when you have 15 for
  // example, but if we want to allow that in the future, at least this can
  // handle it.
  int remove(item::Item id, int amount = 1) noexcept;

  int max_per_item(item::Item /*id*/) const noexcept { return max_per_item_; }
};

inline std::ostream& operator<<(std::ostream& os,
                                const inventory::Inventory& inv) {
  os << "Inventory{";
  bool first = true;
  for (std::underlying_type_t<item::Item> i = 0; i < item::item_count; ++i) {
    const auto id = static_cast<item::Item>(i);
    const int qty = inv[id];
    if (qty == 0) {
      continue;
    }
    if (!first) {
      os << ", ";
    }
    first = false;
    os << item::to_string(id) << ':' << qty;
  }
  if (first) {
    os << "empty";
  }
  os << '}';
  return os;
}

} // namespace core::inventory
