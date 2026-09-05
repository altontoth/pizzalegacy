/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <unordered_map>

#include "core/food/Ingredient.h"

class IngredientFactory {
public:
  static core::food::Ingredient* create_ingredient(int ingredient_id);

private:
  static std::unordered_map<int, std::unique_ptr<core::food::Ingredient>>
      cache_;
};
