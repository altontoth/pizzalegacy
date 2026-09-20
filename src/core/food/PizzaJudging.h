/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>

// How a pizza baked on a standard recipe is judged against the cook book.
// Original requires every topping to be in the right position,
// orientation and chop state, while Relaxed only cares about having the right
// ingredients in roughly the same amount.
enum class PizzaJudging { Original, Relaxed };

inline std::string to_string(PizzaJudging judging) {
  switch (judging) {
  case PizzaJudging::Original:
    return "Original";
  case PizzaJudging::Relaxed:
    return "Relaxed";
  }
  return "??";
}

inline std::string_view to_config_value(PizzaJudging judging) {
  switch (judging) {
  case PizzaJudging::Original:
    return "original";
  case PizzaJudging::Relaxed:
    return "relaxed";
  }
  return "original";
}

inline std::optional<PizzaJudging>
pizza_judging_from_config_value(std::string_view value) {
  if (value == "original") {
    return PizzaJudging::Original;
  }
  if (value == "relaxed") {
    return PizzaJudging::Relaxed;
  }
  return std::nullopt;
}
