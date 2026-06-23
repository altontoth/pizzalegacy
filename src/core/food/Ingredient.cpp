/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/food/Ingredient.h"

#include <cassert>
#include <utility>
#include "engine/Logger.h"

namespace core::food {

Ingredient::Ingredient(IngredientType type, std::string_view name,
                       int weight_large, int weight_medium, int weight_small,
                       int calories, int intensity, int fat, int sweet,
                       int salty, int hot, IngredientCategory category,
                       std::vector<int> bobs)
    : type_(type), name_(name), weight_large_(weight_large),
      weight_medium_(weight_medium), weight_small_(weight_small),
      calories_(calories), intensity_(intensity), fat_(fat), sweet_(sweet),
      salty_(salty), hot_(hot), category_(category), bobs_(std::move(bobs)) {}

Ingredient::Ingredient(const IngredientData& d)
    : Ingredient(d.type, to_string(d.type), d.weight_large, d.weight_medium,
                 d.weight_small, d.calories, d.intensity, d.fat, d.sweet,
                 d.salty, d.hot, d.category,
                 std::vector<int>(d.bobs.begin(), d.bobs.end())) {}

std::string Ingredient::name() const { return name_; }

int Ingredient::bob_state(int state) const { return bobs_[state]; }

} // namespace core::food
