/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <string>
#include <string_view>
#include "engine/FormatCompat.h"
#include "resources/util/Platform.h"

namespace core::food {

enum class CanonicalPizza {
  PizzaNeapolitana = 0,
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

constexpr int to_index(CanonicalPizza id) { return static_cast<int>(id); }
constexpr int CANON_COUNT = static_cast<int>(CanonicalPizza::COUNT);

inline std::string filename_for(CanonicalPizza id) {
  return compat::format("r/r{}{}", to_index(id),
                        platform::is_amiga() ? "" : ".pc");
}

inline constexpr std::array<std::string_view, CANON_COUNT>
    canonical_pizza_names{{
        "Pizza Neapolitana",
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

static_assert(canonical_pizza_names.size() == CANON_COUNT);

constexpr std::string_view to_string(CanonicalPizza id) {
  return canonical_pizza_names[to_index(id)];
}
}; // namespace core::food
