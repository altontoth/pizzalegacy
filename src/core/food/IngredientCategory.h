/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <optional>
#include <string_view>
#include "core/character/Actor.h"
#include "core/food/Ingredient.h"
#include "core/food/IngredientCategoryType.h"
#include "resources/BlitmapManager.h"

namespace core::food {

struct CategoryMetadata {
  IngredientCategory category;
  std::string_view name;
  int start_id; // Inclusive
  int end_id;   // Exclusive (for use in loops: id < end_id)
  int count;
  ActorType supplier_type;
  Blitmap1Icon icon;
};

// Range adapter for range-based for loops
class CategoryIdRange {
  int start_, end_;

public:
  CategoryIdRange(int s, int e) : start_(s), end_(e) {}

  struct iterator {
    int value;
    IngredientType operator*() const {
      return static_cast<IngredientType>(value);
    }
    iterator& operator++() {
      ++value;
      return *this;
    }
    bool operator!=(const iterator& other) const {
      return value != other.value;
    }
  };

  iterator begin() const { return {start_}; }
  iterator end() const { return {end_}; }
};

class CategoryRegistry {
public:
  // Get metadata for a category
  static const CategoryMetadata& get(IngredientCategory category);

  // Convenience accessors
  static int start_id(IngredientCategory category);
  static int end_id(IngredientCategory category);
  static int count(IngredientCategory category);
  static std::string_view name(IngredientCategory category);
  static ActorType supplier_type(IngredientCategory category);
  static Blitmap1Icon icon(IngredientCategory category);

  // Range-based iteration support
  static CategoryIdRange range(IngredientCategory category);

  // Get start/end as pair
  static std::pair<int, int> id_range(IngredientCategory category);

  static IngredientCategory from_actor_type(ActorType type);

  // The category an ingredient belongs to, empty for the padding ids between
  // the categories.
  static std::optional<IngredientCategory> from_ingredient_id(int id);

private:
  static const std::array<CategoryMetadata, 4> metadata_;
  static int index_for(IngredientCategory category);
};

} // namespace core::food
