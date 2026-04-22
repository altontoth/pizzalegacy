/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include "core/food/IngredientType.h"

namespace core::food {

// Type-safe array indexed by IngredientType
template <typename T> struct IngredientArray {
  std::array<T, max_ingredient_count> values{};

  T& operator[](IngredientType t) {
    return values[static_cast<std::size_t>(t)];
  }
  const T& operator[](IngredientType t) const {
    return values[static_cast<std::size_t>(t)];
  }

  T& operator[](int i) { return values[i]; }
  const T& operator[](int i) const { return values[i]; }

  auto begin() { return values.begin(); }
  auto end() { return values.end(); }
  auto begin() const { return values.cbegin(); }
  auto end() const { return values.cend(); }
  [[nodiscard]] constexpr std::size_t size() const { return values.size(); }
  void fill(const T& v) { values.fill(v); }

  void fill_range(IngredientType first, IngredientType last, const T& v) {
    for (auto i = static_cast<int>(first); i <= static_cast<int>(last); ++i) {
      values[i] = v;
    }
  }

  IngredientArray& operator+=(const IngredientArray& other) {
    for (std::size_t i = 0; i < values.size(); ++i) {
      values[i] += other.values[i];
    }
    return *this;
  }
};

using IngredientDemand = IngredientArray<int>;
using IngredientTrends = IngredientArray<int>;

} // namespace core::food
