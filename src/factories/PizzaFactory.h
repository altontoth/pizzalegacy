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
#include "core/food/CanonicalPizza.h"
#include "core/food/CustomerTasteProfiles.h"
#include "core/food/Pizza.h"
#include "engine/Size.h"

using BobSizeFn = std::function<Size(int ingredient_id, int state)>;

class PizzaFactory {
public:
  // Load a recipe from a .pc file, taking its canonical slot from the digits
  // in the filename.
  static std::unique_ptr<core::food::Pizza>
  create_pizza(const std::string& filename, const BobSizeFn& get_size,
               const core::food::CustomerTasteProfiles& taste_profiles);

  // Load a canonical recipe from its own file.
  static std::unique_ptr<core::food::Pizza>
  create_pizza(core::food::CanonicalPizza canonical, const BobSizeFn& get_size,
               const core::food::CustomerTasteProfiles& taste_profiles);

  // Parse a recipe from an in-memory .pc-format buffer. A canonical slot
  // supplies that slot's name and marks the pizza canonical; nullopt reads the
  // name from the buffer and leaves it a user recipe.
  static std::unique_ptr<core::food::Pizza>
  create_pizza(std::span<const uint8_t> bytes,
               std::optional<core::food::CanonicalPizza> canonical,
               const BobSizeFn& get_size,
               const core::food::CustomerTasteProfiles& taste_profiles);

  // Load the reference recipe a competition judges against, from recipe file
  // r/r<recipe_number>.pc. These sit beyond the canonical range.
  static std::unique_ptr<core::food::Pizza> create_competition_pizza(
      int recipe_number, const BobSizeFn& get_size,
      const core::food::CustomerTasteProfiles& taste_profiles);

  // Parse a recipe from an original .GAM save's recipe node. The node has its
  // own field layout; only the 4-byte ingredient placement records match a .pc
  // file. Canonical status and the stored stats come from the node itself.
  static std::unique_ptr<core::food::Pizza> create_pizza_from_gam_node(
      std::span<const uint8_t> node, const BobSizeFn& get_size,
      const core::food::CustomerTasteProfiles& taste_profiles);

  // The node's recipe UID, which the save's menu entries and pizza charts
  // reference. Below CANON_COUNT it names that canonical slot.
  static int gam_node_recipe_uid(std::span<const uint8_t> node);
};
