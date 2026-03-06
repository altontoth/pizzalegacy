/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/inventory/Inventory.h"

namespace core::inventory {

int& Inventory::operator[](item::Item i) noexcept {
  auto idx = static_cast<std::size_t>(i);
  assert(idx < item::item_count);
  return qty[idx];
}

// read-only access
int Inventory::operator[](item::Item i) const noexcept {
  auto idx = static_cast<std::size_t>(i);
  assert(idx < item::item_count);
  return qty[idx];
}

int Inventory::count(item::Item id) const noexcept { return (*this)[id]; }

int Inventory::add(item::Item id, int amount) noexcept {
  assert(amount >= 0);

  int& slot = (*this)[id];
  const int free_space = max_per_item_ - slot;

  const int to_add = std::min(amount, free_space);
  slot += to_add;

  return to_add;
}

int Inventory::remove(item::Item id, int amount) noexcept {
  assert(amount >= 0);

  int& slot = (*this)[id];
  // can't go negative
  const int to_remove = std::min(amount, slot);
  slot -= to_remove;

  return to_remove;
}
} // namespace core::inventory
