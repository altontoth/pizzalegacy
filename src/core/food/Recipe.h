/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <numeric>
#include <string>
#include "core/common/Ids.h"
#include "core/food/Pizza.h"
#include "core/food/StandardPizza.h"

#include <optional>
#include <string_view>

namespace core::food {

// How many days of history do we look at for the pizza charts
inline constexpr int chart_history_days = 7;

class Recipe {
public:
  // For user recipes
  static Recipe user(RecipeId id, std::string display_name, Pizza designed) {
    assert(designed.is_finalized());
    return Recipe(id, Origin::User, std::nullopt, std::move(display_name),
                  std::move(designed));
  }

  // For standard recipes
  static Recipe standard(RecipeId id, StandardPizza reference,
                         std::string display_name, Pizza designed) {
    assert(designed.is_finalized());
    return Recipe(id, Origin::Standard, reference, std::move(display_name),
                  std::move(designed));
  }

  // non-copyable, move-only
  Recipe(const Recipe&) = delete;
  Recipe& operator=(const Recipe&) = delete;
  Recipe(Recipe&&) noexcept = default;
  Recipe& operator=(Recipe&&) noexcept = default;

  RecipeId id() const noexcept { return id_; }
  Origin origin() const noexcept { return origin_; }
  std::optional<StandardPizza> reference_id() const noexcept {
    return reference_id_;
  }

  std::string_view display_name() const noexcept { return display_name_; }
  void rename(std::string new_name) {
    display_name_ = std::move(new_name);
    pizza_.name(display_name_);
  }

  const Pizza& pizza() const noexcept { return pizza_; }
  Pizza& pizza_mut() noexcept { return pizza_; }

  void recreate(Pizza new_pizza) { pizza_ = std::move(new_pizza); }

  // Pizzas sold per day across for every restaurant selling it (the sum is what
  // is used for the pizza chart).
  using DailySales = std::array<int, chart_history_days>;
  const DailySales& daily_sales() const noexcept { return daily_sales_; }
  void daily_sales(const DailySales& sales) noexcept { daily_sales_ = sales; }

  int chart_score() const noexcept {
    return std::accumulate(daily_sales_.begin(), daily_sales_.end(), 0);
  }

  void record_daily_sales(int count) noexcept { daily_sales_.back() += count; }

  void rotate_daily_sales() noexcept {
    std::ranges::rotate(daily_sales_, daily_sales_.begin() + 1);
    daily_sales_.back() = 0;
  }

  // Identity-based equality
  friend bool operator==(const Recipe& a, const Recipe& b) noexcept {
    return a.id_ == b.id_;
  }
  friend bool operator!=(const Recipe& a, const Recipe& b) noexcept {
    return !(a == b);
  }

private:
  Recipe(RecipeId id, Origin origin, std::optional<StandardPizza> reference_id,
         std::string display_name, Pizza pizza)
      : id_(id), origin_(origin), reference_id_(reference_id),
        display_name_(std::move(display_name)), pizza_(std::move(pizza)) {
    pizza_.name(display_name_);
  }

  RecipeId id_{0};
  Origin origin_{Origin::User};
  std::optional<StandardPizza> reference_id_;
  std::string display_name_;
  Pizza pizza_;
  DailySales daily_sales_{};
};

} // namespace core::food
