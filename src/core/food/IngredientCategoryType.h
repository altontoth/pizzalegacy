/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstddef>
#include <string_view>
enum class IngredientCategory {
  VEGETABLES,
  MEAT,
  FRUIT,
  CHEESE,
};

inline constexpr std::size_t ingredient_category_count = 4;

// Type-safe array indexed by IngredientCategory.
template <typename T> struct IngredientCategoryArray {
  std::array<T, ingredient_category_count> values{};

  T& operator[](IngredientCategory c) {
    return values[static_cast<std::size_t>(c)];
  }
  const T& operator[](IngredientCategory c) const {
    return values[static_cast<std::size_t>(c)];
  }

  auto begin() { return values.begin(); }
  auto end() { return values.end(); }
  auto begin() const { return values.cbegin(); }
  auto end() const { return values.cend(); }

  std::array<T, ingredient_category_count>& data() { return values; }
  const std::array<T, ingredient_category_count>& data() const {
    return values;
  }
};

inline constexpr std::array<IngredientCategory, ingredient_category_count>
    ALL_INGREDIENT_CATEGORIES = {
        IngredientCategory::VEGETABLES, IngredientCategory::MEAT,
        IngredientCategory::FRUIT, IngredientCategory::CHEESE};

inline std::string_view to_string(IngredientCategory cat) {
  switch (cat) {
  case IngredientCategory::VEGETABLES:
    return "vegetables";
  case IngredientCategory::MEAT:
    return "meat";
  case IngredientCategory::FRUIT:
    return "fruit";
  case IngredientCategory::CHEESE:
    return "cheese";
  }
  return "unknown";
}
