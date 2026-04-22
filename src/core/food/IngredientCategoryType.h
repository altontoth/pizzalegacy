/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string_view>
enum class IngredientCategory {
  VEGETABLES,
  MEAT,
  FRUIT,
  CHEESE,
};

inline std::string_view to_string(IngredientCategory cat) {
  switch (cat) {
  case IngredientCategory::VEGETABLES:
    return "vegetables";
  case IngredientCategory::MEAT:
    return "meat";
  case IngredientCategory::FRUIT:
    return "fruit";
  case IngredientCategory::CHEESE:
    return "cheese";
  }
}
