/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include "engine/FormatCompat.h"

namespace core::food {

enum class StandardPizza {
  PizzaNapoletana = 0,
  PizzaMargherita,
  QuattroStagioni,
  AndreaDoria,
  PizzaSalami,
  HamPizza,
  MincemeatPizza,
  ChickenPizza,
  PrawnPizza,
  FishPizza,
  MusselPizza,
  SalmonPizza,
  PizzaExocita,
  VegetablePizza,
  COUNT,
};

constexpr int to_index(StandardPizza id) { return static_cast<int>(id); }
constexpr int STANDARD_PIZZA_COUNT = static_cast<int>(StandardPizza::COUNT);

// The standard pizza at an index, or nullopt when the index is out of range.
constexpr std::optional<StandardPizza> standard_from_index(int index) {
  if (index < 0 || index >= STANDARD_PIZZA_COUNT) {
    return std::nullopt;
  }
  return static_cast<StandardPizza>(index);
}

inline std::string filename_for(StandardPizza id) {
  return compat::format("r/r{}.pc", to_index(id));
}

inline constexpr std::array<std::string_view, STANDARD_PIZZA_COUNT>
    standard_pizza_names{{
        "Pizza Napoletana",
        "Pizza Margherita",
        "Quattro Stagioni",
        "Andrea Doria",
        "Pizza Salami",
        "Ham Pizza",
        "Mincemeat Pizza",
        "Chicken Pizza",
        "Prawn Pizza",
        "Fish Pizza",
        "Mussel Pizza",
        "Salmon Pizza",
        "Pizza Exocita",
        "Vegetable Pizza",
    }};

static_assert(standard_pizza_names.size() == STANDARD_PIZZA_COUNT);

constexpr std::string_view to_string(StandardPizza id) {
  return standard_pizza_names[to_index(id)];
}
}; // namespace core::food
