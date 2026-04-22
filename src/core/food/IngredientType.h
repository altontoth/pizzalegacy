/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include "core/food/IngredientCategoryType.h"

// Original game has 4 x 24 ingredient slots
// and even though we don't use them all this is nice and (time) efficient,
// we can just access by ingredient id.
constexpr int max_ingredient_count = 96;

namespace core::food {

enum class IngredientType {
  // Vegetables
  TOMATOES = 0,
  OLIVES = 1,
  PEPPERS = 2,
  ASPARAGUS = 3,
  COURGETTES = 4,
  CARROTS = 5,
  CUCUMBERS = 6,
  RADISHES = 7,
  PEAS = 8,
  BROCCOLI = 9,
  POTATOES = 10,
  CHILLIES = 11,
  SPINACH = 12,
  SALAD = 13,
  LEEKS = 14,
  ONIONS = 15,
  GARLIC = 16,
  SWEET_CORN = 17,
  CAULIFLOWER = 18,
  MUSHROOMS = 19,
  CHANTERELLES = 20,
  W_MUSHROOMS = 21,
  // Meat
  SALAMI = 24,
  BACON = 25,
  HAM = 26,
  BEEF = 27,
  PORK = 28,
  CHICKEN = 29,
  MINCEMEAT = 30,
  SAUSAGE = 31,
  SCAMPI = 32,
  SHRIMPS = 33,
  SQUID = 34,
  ANCHOVY = 35,
  SALMON = 36,
  LOBSTER = 37,
  CAVIAR = 38,
  COCKLES = 39,
  MUSSELS = 40,
  // Fruit
  APPLES = 48,
  PEARS = 49,
  MAN_ORANGES = 50,
  PLUMS = 51,
  CHERRIES = 52,
  BANANAS = 53,
  LEMONS = 54,
  PINEAPPLE = 55,
  KIWI = 56,
  STRAWBERRIES = 57,
  RASPBERRIES = 58,
  BLACKBERRIES = 59,
  GOOSEBERRIES = 60,
  // Cheese
  EGG = 72,
  MOZZARELLA = 73,
  PROVOLONE = 74,
  BEL_PAESE = 75,
  GORGONZOLA = 76,
  PAMERINO = 77,
  PARMESAN = 78,
  GOATS_CHEESE = 79,
  PARSLEY = 80,
  LEMON_BALM = 81,
  SAGE = 82,
  GINGER = 83,
  OREGANO = 84,
};

struct IngredientRange {
  IngredientType first;
  IngredientType last; // inclusive
};

constexpr std::array<IngredientRange, 4> ingredient_ranges = {{
    {.first = IngredientType::TOMATOES, .last = IngredientType::W_MUSHROOMS},
    {.first = IngredientType::SALAMI, .last = IngredientType::MUSSELS},
    {.first = IngredientType::APPLES, .last = IngredientType::GOOSEBERRIES},
    {.first = IngredientType::EGG, .last = IngredientType::OREGANO},
}};

constexpr IngredientRange
ingredient_range_for_category(IngredientCategory category) {
  return ingredient_ranges[static_cast<int>(category)];
}

constexpr bool is_valid_ingredient_id(uint8_t id) {
  using UT = std::underlying_type_t<IngredientType>;

  return std::ranges::any_of(ingredient_ranges,
                             [id](const IngredientRange& range) {
                               const auto first = static_cast<UT>(range.first);
                               const auto last = static_cast<UT>(range.last);

                               return id >= first && id <= last;
                             });
}

constexpr std::string_view to_string(IngredientType type) {
  switch (type) {
  case IngredientType::TOMATOES:
    return "Tomatoes";
  case IngredientType::OLIVES:
    return "Olives";
  case IngredientType::PEPPERS:
    return "Peppers";
  case IngredientType::ASPARAGUS:
    return "Asparagus";
  case IngredientType::COURGETTES:
    return "Courgettes";
  case IngredientType::CARROTS:
    return "Carrots";
  case IngredientType::CUCUMBERS:
    return "Cucumbers";
  case IngredientType::RADISHES:
    return "Radishes";
  case IngredientType::PEAS:
    return "Peas";
  case IngredientType::BROCCOLI:
    return "Broccoli";
  case IngredientType::POTATOES:
    return "Potatoes";
  case IngredientType::CHILLIES:
    return "Chillies";
  case IngredientType::SPINACH:
    return "Spinach";
  case IngredientType::SALAD:
    return "Salad";
  case IngredientType::LEEKS:
    return "Leeks";
  case IngredientType::ONIONS:
    return "Onions";
  case IngredientType::GARLIC:
    return "Garlic";
  case IngredientType::SWEET_CORN:
    return "Sweet Corn";
  case IngredientType::CAULIFLOWER:
    return "Cauliflower";
  case IngredientType::MUSHROOMS:
    return "Mushrooms";
  case IngredientType::CHANTERELLES:
    return "Chanterelles";
  case IngredientType::W_MUSHROOMS:
    return "W. Mushrooms";
  case IngredientType::SALAMI:
    return "Salami";
  case IngredientType::BACON:
    return "Bacon";
  case IngredientType::HAM:
    return "Ham";
  case IngredientType::BEEF:
    return "Beef";
  case IngredientType::PORK:
    return "Pork";
  case IngredientType::CHICKEN:
    return "Chicken";
  case IngredientType::MINCEMEAT:
    return "Mincemeat";
  case IngredientType::SAUSAGE:
    return "Sausage";
  case IngredientType::SCAMPI:
    return "Scampi";
  case IngredientType::SHRIMPS:
    return "Shrimps";
  case IngredientType::SQUID:
    return "Squid";
  case IngredientType::ANCHOVY:
    return "Anchovy";
  case IngredientType::SALMON:
    return "Salmon";
  case IngredientType::LOBSTER:
    return "Lobster";
  case IngredientType::CAVIAR:
    return "Caviar";
  case IngredientType::COCKLES:
    return "Cockles";
  case IngredientType::MUSSELS:
    return "Mussels";
  case IngredientType::APPLES:
    return "Apples";
  case IngredientType::PEARS:
    return "Pears";
  case IngredientType::MAN_ORANGES:
    return "Man Oranges";
  case IngredientType::PLUMS:
    return "Plums";
  case IngredientType::CHERRIES:
    return "Cherries";
  case IngredientType::BANANAS:
    return "Bananas";
  case IngredientType::LEMONS:
    return "Lemons";
  case IngredientType::PINEAPPLE:
    return "Pineapple";
  case IngredientType::KIWI:
    return "Kiwi";
  case IngredientType::STRAWBERRIES:
    return "Strawberries";
  case IngredientType::RASPBERRIES:
    return "Raspberries";
  case IngredientType::BLACKBERRIES:
    return "Blackberries";
  case IngredientType::GOOSEBERRIES:
    return "Gooseberries";
  case IngredientType::EGG:
    return "Egg";
  case IngredientType::MOZZARELLA:
    return "Mozzarella";
  case IngredientType::PROVOLONE:
    return "Provolone";
  case IngredientType::BEL_PAESE:
    return "Bel Paese";
  case IngredientType::GORGONZOLA:
    return "Gorgonzola";
  case IngredientType::PAMERINO:
    return "Pamerino";
  case IngredientType::PARMESAN:
    return "Parmesan";
  case IngredientType::GOATS_CHEESE:
    return "Goats Cheese";
  case IngredientType::PARSLEY:
    return "Parsley";
  case IngredientType::LEMON_BALM:
    return "Lemon Balm";
  case IngredientType::SAGE:
    return "Sage";
  case IngredientType::GINGER:
    return "Ginger";
  case IngredientType::OREGANO:
    return "Oregano";
  }
};

} // namespace core::food
