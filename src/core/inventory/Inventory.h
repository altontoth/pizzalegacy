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
  // read-only access
  int operator[](item::Item i) const noexcept;

  // Return how many units of this item the container holds.
  int count(item::Item id) const noexcept;

  // Try to add `amount` units. Returns the number of items actually
  // added (you can't go beyond 255):
  int add(item::Item id, int amount = 1) noexcept;

  // Try to remove `amount` units. Returns the number of items actually
  // removed (you can't go negative):
  int remove(item::Item id, int amount = 1) noexcept;

  int max_per_item(item::Item id) noexcept { return max_per_item_; }
};

inline std::ostream& operator<<(std::ostream& os,
                                const inventory::Inventory& inv) {
  os << "Inventory{";

  bool first = true;
  for (std::underlying_type_t<item::Item> i = 0; i < item::item_count; ++i) {
    const item::Item id = static_cast<item::Item>(i);
    const int qty = inv[id];
    if (qty == 0) {
      continue; // skip empty slots
    }

    if (!first) {
      os << ", ";
    }
    first = false;

    os << item::to_string(id) << ':' << qty;
  }

  if (first) {
    os << "empty"; // nothing was printed
  }
  os << '}';

  return os;
}

} // namespace core::inventory
