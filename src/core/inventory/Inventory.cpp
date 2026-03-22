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

int Inventory::add_with_cost(item::Item id, int amount,
                             Money total_cost) noexcept {
  const int added = add(id, amount);
  if (added > 0 && amount > 0) {
    // If capped, only record cost for items that actually went in
    cost_basis[static_cast<std::size_t>(id)] += total_cost * added / amount;
  }
  return added;
}

int Inventory::remove(item::Item id, int amount) noexcept {
  assert(amount >= 0);

  int& slot = (*this)[id];
  // can't go negative
  const int to_remove = std::min(amount, slot);
  slot -= to_remove;

  return to_remove;
}

Money Inventory::remove_with_cost(item::Item id, int amount) noexcept {
  auto idx = static_cast<std::size_t>(id);
  const int before = qty[idx];
  const int removed = remove(id, amount);
  if (removed == 0 || before == 0) {
    return Money{0};
  }
  // Withdraw proportional average cost
  Money withdrawn = cost_basis[idx] * removed / before;
  cost_basis[idx] -= withdrawn;
  return withdrawn;
}
} // namespace core::inventory
