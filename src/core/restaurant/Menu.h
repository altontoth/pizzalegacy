/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <unordered_map>
#include <vector>
#include "core/Demographics.h"
#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/food/PlayerRecipes.h"

namespace core::restaurant {

struct MenuEntry {
  core::RecipeId recipe_id;
  Money price{0};

  // Local tuning (per-restaurant factors). Start with neutral values:
  // Multiplies base popularity from the Pizza (1.0 = unchanged)
  std::array<float, static_cast<size_t>(ConsumerType::Count)>
      popularity_multiplier{};

  MenuEntry() { popularity_multiplier.fill(1.0f); }
};

class Menu {
public:
  // Maximum pizzas allowed on menu (from original game)
  static constexpr int MAXIMUM_PIZZAS = 24;

  // Price constraints enforced on all menu entries
  static constexpr Money min_price = 1_usd;
  static constexpr Money max_price = 50_usd;

  // Adds or overwrites a slot with a recipe + price; returns index.
  // Price is clamped to [min_price, max_price].
  int add_or_set(core::RecipeId id, Money price) {
    price = std::clamp(price, min_price, max_price);
    if (auto it = by_id_.find(id); it != by_id_.end()) {
      auto idx = it->second;
      auto& e = entries_[idx];
      e.recipe_id = id;
      e.price = price;
      return static_cast<int>(idx);
    }
    MenuEntry e;
    e.recipe_id = id;
    e.price = price;
    entries_.push_back(e);
    by_id_[id] = entries_.size() - 1;
    return static_cast<int>(entries_.size() - 1);
  }

  bool remove(core::RecipeId id) {
    auto it = by_id_.find(id);
    if (it == by_id_.end()) {
      return false;
    }
    auto idx = it->second;
    entries_.erase(entries_.begin() + idx);
    by_id_.erase(it);

    // Update indices in map for entries after the removed one
    for (auto& [rid, ridx] : by_id_) {
      if (ridx > idx) {
        ridx--;
      }
    }
    return true;
  }

  bool set_price(core::RecipeId id, Money price) {
    if (auto* e = find_mut(id)) {
      e->price = price;
      return true;
    }
    return false;
  }

  // Lookup
  const MenuEntry* find(core::RecipeId id) const {
    auto it = by_id_.find(id);
    return (it == by_id_.end()) ? nullptr : &entries_[it->second];
  }
  MenuEntry* find_mut(core::RecipeId id) {
    auto it = by_id_.find(id);
    return (it == by_id_.end()) ? nullptr : &entries_[it->second];
  }

  const std::vector<MenuEntry>& entries() const { return entries_; }
  std::vector<MenuEntry>& entries() { return entries_; }

  // Get total count of all pizzas on menu (canonical + user)
  size_t count_total() const { return entries_.size(); }

  // Get count of canonical pizzas only (slot < 14)
  size_t count_canonical(const core::food::PlayerRecipes& recipes) const {
    int count = 0;
    for (const auto& entry : entries_) {
      const auto* recipe = recipes.find(entry.recipe_id);
      if (recipe && recipe->canonical_id().has_value()) {
        count++;
      }
    }
    return count;
  }

private:
  std::vector<MenuEntry> entries_;
  std::unordered_map<core::RecipeId, size_t, core::RecipeId::Hasher> by_id_;
};

} // namespace core::restaurant
