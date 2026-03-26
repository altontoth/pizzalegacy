/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/save/SaveGameFood.h"

#include "engine/geom/Spaces.h"

namespace save {

json topping_to_json(const core::food::Topping& topping) {
  json j;
  j["ingredient_id"] = static_cast<int>(topping.ingredient_type());
  j["state"] = topping.state_raw();
  j["origin"] = {{"x", topping.origin().x}, {"y", topping.origin().y}};
  return j;
}

std::unique_ptr<core::food::Topping> json_to_topping(const json& j) {
  int ingredient_id = j["ingredient_id"].get<int>();
  int state = j["state"].get<int>();
  WorldPoint origin{j["origin"]["x"].get<int>(), j["origin"]["y"].get<int>()};

  return std::make_unique<core::food::Topping>(ingredient_id, state, origin);
}

json pizza_to_json(const core::food::Pizza& pizza) {
  using namespace core::food;

  json j;
  j["name"] = pizza.name();
  j["finalized"] = pizza.is_finalized();
  j["origin"] = static_cast<int>(pizza.origin());

  // Save toppings
  json toppings_array = json::array();
  for (const auto& topping : pizza.toppings()) {
    toppings_array.push_back(topping_to_json(*topping));
  }
  j["toppings"] = toppings_array;

  // Save scores (5 judges)
  j["scores"] = pizza.scores();

  // Save popularity (6 demographic groups)
  json popularity_array = json::array();
  auto pop = pizza.popularity();
  for (size_t i = 0; i < static_cast<size_t>(ConsumerType::Count); ++i) {
    popularity_array.push_back(pop.values[i]);
  }
  j["popularity"] = popularity_array;

  // Save flavors
  auto flavors = pizza.flavors();
  j["flavors"] = {{"fat", flavors.fat},
                  {"sweet", flavors.sweet},
                  {"salty", flavors.salty},
                  {"hot", flavors.hot}};

  // Save stats
  j["calories"] = pizza.calories();
  j["appearance"] = pizza.appearance();
  j["refinement"] = pizza.refinement();
  j["normality"] = pizza.normality();
  j["imitation"] = pizza.imitation();

  return j;
}

core::food::Pizza
json_to_pizza(const json& j,
              const core::food::CustomerTasteProfiles& taste_profiles) {
  using namespace core::food;

  std::string name = j["name"].get<std::string>();
  auto pizza = Pizza(name);

  // Restore origin
  pizza.set_origin(static_cast<Origin>(j["origin"].get<int>()));

  // Restore toppings
  if (j.contains("toppings")) {
    for (const auto& topping_json : j["toppings"]) {
      pizza.add_topping(json_to_topping(topping_json));
    }
  }

  // Restore flavors and stats first (before finalize)
  if (j.contains("flavors")) {
    Flavors flavors;
    flavors.fat = j["flavors"]["fat"].get<int>();
    flavors.sweet = j["flavors"]["sweet"].get<int>();
    flavors.salty = j["flavors"]["salty"].get<int>();
    flavors.hot = j["flavors"]["hot"].get<int>();

    int appearance = j["appearance"].get<int>();
    int refinement = j["refinement"].get<int>();
    int normality = j["normality"].get<int>();

    pizza.set_disk_stats(flavors, appearance, refinement, normality,
                         taste_profiles);
  }

  // Finalize if it was finalized when saved
  if (j["finalized"].get<bool>()) {
    pizza.finalize(taste_profiles, name);
  }

  return pizza;
}

json recipe_to_json(const core::food::Recipe& recipe) {
  using namespace core::food;

  json j;
  j["id"] = recipe.id().value;
  j["origin"] = static_cast<int>(recipe.origin());
  j["display_name"] = recipe.display_name();

  // Save canonical_id if present
  if (recipe.canonical_id().has_value()) {
    j["canonical_id"] = static_cast<int>(*recipe.canonical_id());
  }
  else {
    j["canonical_id"] = nullptr;
  }

  // Save the pizza
  j["pizza"] = pizza_to_json(recipe.pizza());

  return j;
}

core::food::Recipe
json_to_recipe(const json& j,
               const core::food::CustomerTasteProfiles& taste_profiles) {
  using namespace core::food;

  core::RecipeId id{j["id"].get<int>()};
  Origin origin = static_cast<Origin>(j["origin"].get<int>());
  std::string display_name = j["display_name"].get<std::string>();
  Pizza pizza = json_to_pizza(j["pizza"], taste_profiles);

  // Check if this is a canonical recipe
  if (!j["canonical_id"].is_null()) {
    CanonicalPizza canonical_id =
        static_cast<CanonicalPizza>(j["canonical_id"].get<int>());
    return Recipe::canonical(id, canonical_id, std::move(display_name),
                             std::move(pizza));
  }
  return Recipe::user(id, std::move(display_name), std::move(pizza));
}

} // namespace save
