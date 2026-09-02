/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>
#include "core/food/IngredientCategoryType.h"
#include "core/food/IngredientType.h"

namespace core::food {

struct IngredientData {
  IngredientType type;
  int weight_large;
  int weight_medium;
  int weight_small;
  int calories;
  int intensity;
  int fat;
  int sweet;
  int salty;
  int hot;
  IngredientCategory category;
  std::array<int, 12> bobs;
};

class Ingredient {
public:
  Ingredient(IngredientType type, std::string_view name, int weight_large,
             int weight_medium, int weight_small, int calories, int intensity,
             int fat, int sweet, int salty, int hot,
             IngredientCategory category, std::vector<int> bobs);
  explicit Ingredient(const IngredientData& data);
  IngredientType type() const { return type_; }
  std::string name() const;
  bool can_be_chopped() const;
  bool can_be_minced() const;
  int intensity() const { return intensity_; }
  int fat() const { return fat_; }
  int sweet() const { return sweet_; }
  int salty() const { return salty_; }
  int hot() const { return hot_; }
  int weight_large() const { return weight_large_; }
  int weight_medium() const { return weight_medium_; }
  int weight_small() const { return weight_small_; }
  int calories_per_100g() const { return calories_; }
  int bob_state(int state) const;
  int state() const { return state_; };
  const std::vector<int>& bobs() const { return bobs_; };
  IngredientCategory category() const { return category_; };

private:
  IngredientType type_;
  std::string name_;
  // Weights in grams
  int weight_large_{0};
  int weight_medium_{0};
  int weight_small_{0};
  int calories_{0};
  int intensity_{0};
  int fat_{0};
  int sweet_{0};
  int salty_{0};
  int hot_{0};
  int popularity_{0};
  IngredientCategory category_;
  int count_whole_{0};
  int count_chopped_{0};
  int count_minced_{0};
  int state_{0};
  std::vector<int> bobs_; // A vector of bobs (4 orientations * 3 sizes)
};

} // namespace core::food
