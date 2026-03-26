/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include "core/food/CustomerTasteProfiles.h"
#include "core/food/Pizza.h"
#include "core/food/PlayerRecipes.h"
#include "core/food/Recipe.h"
#include "core/food/Topping.h"

class GameState;
class GameContext;

namespace save {

using json = nlohmann::json;

json topping_to_json(const core::food::Topping& topping);
std::unique_ptr<core::food::Topping> json_to_topping(const json& j);

json pizza_to_json(const core::food::Pizza& pizza);
core::food::Pizza
json_to_pizza(const json& j,
              const core::food::CustomerTasteProfiles& taste_profiles);

json recipe_to_json(const core::food::Recipe& recipe);
core::food::Recipe
json_to_recipe(const json& j,
               const core::food::CustomerTasteProfiles& taste_profiles);

} // namespace save
