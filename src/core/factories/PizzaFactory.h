/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "core/food/CustomerTasteProfiles.h"
#include "core/food/Pizza.h"
#include "core/food/PlayerRecipes.h"
#include "core/food/Recipe.h"
#include "core/food/StandardPizza.h"
#include "resources/BobSizeFn.h"

class PizzaFactory {
public:
  // A recipe file's name and toppings, read without building the pizza.
  struct RecipeSummary {
    std::string name;
    std::vector<std::string> ingredients;
  };
  static std::optional<RecipeSummary> read_summary(const std::string& filename);
  static std::optional<RecipeSummary>
  read_summary(std::span<const uint8_t> bytes);

  enum class ImportResult { Imported, AlreadyListed, Unreadable };
  // Adds the recipe file to the list as a user recipe, under the name stored in
  // the file.
  static ImportResult
  import_recipe(const std::string& filename, core::food::PlayerRecipes& recipes,
                const BobSizeFn& get_size,
                const core::food::CustomerTasteProfiles& taste_profiles);

  // Load a recipe from a .pc file, taking its standard slot from the digits
  // in the filename.
  static std::unique_ptr<core::food::Pizza>
  create_pizza(const std::string& filename, const BobSizeFn& get_size,
               const core::food::CustomerTasteProfiles& taste_profiles);

  // Load a standard pizza's reference from its own file.
  static std::unique_ptr<core::food::Pizza>
  create_pizza(core::food::StandardPizza reference, const BobSizeFn& get_size,
               const core::food::CustomerTasteProfiles& taste_profiles);

  // A recipe's name and toppings, without the stats that depend on the
  // customers' tastes.
  static std::unique_ptr<core::food::Pizza>
  read_pizza(std::span<const uint8_t> bytes,
             std::optional<core::food::StandardPizza> reference,
             const BobSizeFn& get_size);
  static std::unique_ptr<core::food::Pizza>
  read_pizza(core::food::StandardPizza reference, const BobSizeFn& get_size);

  // Parse a recipe from an in-memory .pc-format buffer. A reference supplies
  // its name and marks the pizza standard; nullopt reads the name from the
  // buffer and leaves it a user recipe.
  static std::unique_ptr<core::food::Pizza>
  create_pizza(std::span<const uint8_t> bytes,
               std::optional<core::food::StandardPizza> reference,
               const BobSizeFn& get_size,
               const core::food::CustomerTasteProfiles& taste_profiles);

  // Load the reference recipe a competition judges against, from recipe file
  // r/r<recipe_number>.pc. These sit beyond the reference range.
  static std::unique_ptr<core::food::Pizza> create_competition_pizza(
      int recipe_number, const BobSizeFn& get_size,
      const core::food::CustomerTasteProfiles& taste_profiles);

  // Parse a recipe from an original .GAM save's recipe node. The node has its
  // own field layout; only the 4-byte ingredient placement records match a .pc
  // file. Standard status and the stored stats come from the node itself.
  static std::unique_ptr<core::food::Pizza> create_pizza_from_gam_node(
      std::span<const uint8_t> node, const BobSizeFn& get_size,
      const core::food::CustomerTasteProfiles& taste_profiles);

  // The recipe UID, used for save games and pizza charts. If it's less than
  // STANDARD_PIZZA_COUNT it's a standard pizza.
  static int gam_node_recipe_uid(std::span<const uint8_t> node);

  // The recipe's rolling daily-sold history, which scores it on the charts.
  static core::food::Recipe::DailySales
  gam_node_daily_sales(std::span<const uint8_t> node);
};
