/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// core/food/player_recipes.h
#pragma once
#include <array>
#include <variant>
#include <vector>
#include "core/common/Ids.h"
#include "core/food/CanonicalPizza.h"
#include "core/food/Recipe.h"

struct NewRowTag {};
inline NewRowTag new_row_tag{};
using RecipeKey =
    std::variant<NewRowTag, core::RecipeId, core::food::CanonicalPizza>;

namespace core::food {

struct CanonicalSlot {
  std::optional<food::Recipe> recipe; // empty = not created
  bool created() const noexcept { return recipe.has_value(); }
};

enum class RecipeOrigin { User, Canonical };

struct PlayerRecipes {
  std::array<CanonicalSlot, to_index(CanonicalPizza::COUNT)> canon{};
  std::vector<Recipe> user;

  core::RecipeId next_id{0};

  [[nodiscard]] size_t size() const { return canon.size() + user.size(); };
  [[nodiscard]] size_t recipe_count() const {
    const auto canon_count =
        std::count_if(canon.begin(), canon.end(),
                      [](auto const& slot) { return slot.created(); });
    return canon_count + user.size();
  }

  core::RecipeId create_user(std::string name, Pizza designed) {
    const core::RecipeId id = next_id++;
    user.emplace_back(Recipe::user(id, std::move(name), std::move(designed)));
    return id;
  }

  core::RecipeId recreate_user(core::RecipeId id, std::string name,
                               Pizza designed) {
    auto it = std::find_if(user.begin(), user.end(),
                           [&](const Recipe& r) { return r.id() == id; });
    if (it == user.end()) {
      return id;
    }
    // Redesigning a pizza keeps it on the charts at its current place.
    auto sales = it->daily_sales();
    *it = Recipe::user(id, std::move(name), std::move(designed));
    it->daily_sales(sales);
    return id;
  }

  core::RecipeId create_from_canonical(CanonicalPizza cp, Pizza designed) {
    // Check if this pizza was created before, and if so, re-use its id:
    auto& current = canon[static_cast<int>(cp)];
    const core::RecipeId id = current.recipe ? current.recipe->id() : next_id++;
    Recipe::DailySales sales =
        current.recipe ? current.recipe->daily_sales() : Recipe::DailySales{};
    // Get the name from the canonical pizza list:
    auto name = std::string(canonical_pizza_names[to_index(cp)]);
    // Store it in the right slot
    canon[static_cast<int>(cp)] = CanonicalSlot{
        Recipe::canonical(id, cp, std::move(name), std::move(designed))};
    canon[static_cast<int>(cp)].recipe->daily_sales(sales);
    return id;
  }

  std::vector<const Recipe*> all() const {
    std::vector<const Recipe*> out;
    out.reserve(recipe_count());
    for (const auto& slot : canon) {
      if (slot.created()) {
        out.push_back(&slot.recipe.value());
      }
    }
    for (const auto& recipe : user) {
      out.push_back(&recipe);
    }
    return out;
  }

  std::vector<Recipe*> all_mut() {
    std::vector<Recipe*> out;
    out.reserve(recipe_count());
    for (auto& slot : canon) {
      if (slot.created()) {
        out.push_back(&slot.recipe.value());
      }
    }
    for (auto& recipe : user) {
      out.push_back(&recipe);
    }
    return out;
  }

  const Recipe* find(core::RecipeId id) const {
    // Search user recipes
    auto user_it = std::find_if(user.begin(), user.end(),
                                [&](const Recipe& r) { return r.id() == id; });
    if (user_it != user.end()) {
      return &(*user_it);
    }
    // Search canonical recipes
    for (const auto& slot : canon) {
      if (slot.created() && slot.recipe->id() == id) {
        return &(slot.recipe.value());
      }
    }
    return nullptr;
  }

  Recipe* find_mut(core::RecipeId id) {
    // Search user recipes
    auto user_it = std::find_if(user.begin(), user.end(),
                                [&](const Recipe& r) { return r.id() == id; });
    if (user_it != user.end()) {
      return &(*user_it);
    }
    // Search canonical recipes
    for (auto& slot : canon) {
      if (slot.created() && slot.recipe->id() == id) {
        return &(slot.recipe.value());
      }
    }
    return nullptr;
  }

  static std::optional<core::RecipeId> id_of(const PlayerRecipes& pr,
                                             core::food::CanonicalPizza cp) {
    const auto& slot = pr.canon[to_index(cp)];
    if (!slot.recipe) {
      return std::nullopt;
    }
    return slot.recipe->id();
  }

  static std::optional<core::RecipeId> resolve_to_id(const PlayerRecipes& pr,
                                                     const RecipeKey& key) {
    if (const auto* id = std::get_if<core::RecipeId>(&key)) {
      return *id;
    }
    if (const auto* cp = std::get_if<core::food::CanonicalPizza>(&key)) {
      return id_of(pr, *cp);
    }
    return std::nullopt; // NewRowTag
  }
};

} // namespace core::food
