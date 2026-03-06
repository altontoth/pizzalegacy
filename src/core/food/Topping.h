/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include "core/food/Ingredient.h"
#include "engine/BobRenderable.h"
#include "engine/geom/Spaces.h"

namespace core::food {

class Topping : public BobRenderable {
  friend std::ostream& operator<<(std::ostream& os, const Topping& topping);

public:
  Topping(IngredientType ingredient);
  Topping(int ingredient_id, int state, WorldPoint p);
  Topping(const Topping& other);
  void rotate();
  bool chop();
  void place(WorldPoint p);
  Ingredient* ingredient() const;
  void reload_image();
  WorldPoint origin() const override;
  float calories() const;
  int weight() const;
  const std::vector<int>& bobs() const override;
  int bob_state() const override;
  int chopped_state() const;
  int rotation_state() const;
  IngredientType ingredient_type() const { return ingredient_type_; }
  int state_raw() const { return state_; }
  Anchor anchor() const override { return Anchor::Center; }

private:
  Ingredient* ingredient_;
  WorldPoint origin_;
  IngredientType ingredient_type_;
  int state_;
};

} // namespace core::food
