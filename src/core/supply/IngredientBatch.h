/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/finance/Money.h"
#include "core/food/Ingredient.h"

struct IngredientBatch {
  const core::food::Ingredient* ingredient;
  core::food::IngredientType ingredient_type;
  int count{0};
  Money price_per_unit{0};
  int quality{0};
  int current_stock{0};
  int daily_usage{0};
  int need{0};
};
