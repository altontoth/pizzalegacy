/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include "core/factories/PizzaFactory.h"
#include "engine/DebugLabel.h"
#include "engine/Size.h"
#include "engine/geom/Spaces.h"
#include "views/ui/widgets/MenuItem.h"

// Shows a recipe file's name and toppings, with the text starting at
// text_left to leave room for a picture of the pizza.
class RecipeFileInfoBox : public MenuItem,
                          public debug::AutoLabel<RecipeFileInfoBox> {
public:
  RecipeFileInfoBox(GameContext& gc, int text_left)
      : MenuItem(gc), text_left_(text_left) {}

  void set_file(const std::string& filename);

  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override {}
  Size measure() const override;
  bool focusable() const override { return false; }

private:
  int text_left_;
  std::string filename_;
  std::optional<PizzaFactory::RecipeSummary> summary_;
};
