/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <cassert>
#include <string>
#include "core/common/Ids.h"
#include "core/food/CanonicalPizza.h"
#include "core/food/Pizza.h"

#include <optional>
#include <string_view>

namespace core::food {

class Recipe {
public:
  // For user recipes
  static Recipe user(RecipeId id, std::string display_name, Pizza designed) {
    assert(designed.is_finalized());
    return Recipe(id, Origin::User, std::nullopt, std::move(display_name),
                  std::move(designed));
  }

  // For canonical recipes
  static Recipe canonical(RecipeId id, CanonicalPizza cid,
                          std::string display_name, Pizza designed) {
    assert(designed.is_finalized());
    return Recipe(id, Origin::Canonical, cid, std::move(display_name),
                  std::move(designed));
  }

  // non-copyable, move-only
  Recipe(const Recipe&) = delete;
  Recipe& operator=(const Recipe&) = delete;
  Recipe(Recipe&&) noexcept = default;
  Recipe& operator=(Recipe&&) noexcept = default;

  RecipeId id() const noexcept { return id_; }
  Origin origin() const noexcept { return origin_; }
  std::optional<CanonicalPizza> canonical_id() const noexcept {
    return canonical_id_;
  }

  std::string_view display_name() const noexcept { return display_name_; }
  void rename(std::string new_name) { display_name_ = std::move(new_name); }

  const Pizza& pizza() const noexcept { return pizza_; }
  Pizza& pizza_mut() noexcept { return pizza_; }

  void recreate(Pizza new_pizza) { pizza_ = std::move(new_pizza); }

  // Identity-based equality
  friend bool operator==(const Recipe& a, const Recipe& b) noexcept {
    return a.id_ == b.id_;
  }
  friend bool operator!=(const Recipe& a, const Recipe& b) noexcept {
    return !(a == b);
  }

private:
  Recipe(RecipeId id, Origin origin, std::optional<CanonicalPizza> canonical_id,
         std::string display_name, Pizza pizza)
      : id_(id), origin_(origin), canonical_id_(canonical_id),
        display_name_(std::move(display_name)), pizza_(std::move(pizza)) {}

  RecipeId id_{0};
  Origin origin_{Origin::User};
  std::optional<CanonicalPizza> canonical_id_;
  std::string display_name_;
  Pizza pizza_;
};

} // namespace core::food
