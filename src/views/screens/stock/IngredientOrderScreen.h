/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/finance/Money.h"
#include "core/food/Ingredient.h"
#include "core/food/IngredientCategoryType.h"
#include "core/state/UIState.h"
#include "core/supply/IngredientBatch.h"
#include "views/screens/stock/StockMain.h"

#include <optional>
#include <vector>

class FoodSupplier;

class IngredientOrderScreen : public views::screens::stock::StockMain {
public:
  IngredientOrderScreen(
      GameContext& context, GameState& state, uint8_t supplier_id,
      IngredientCategory category,
      IngredientScreenMode mode = IngredientScreenMode::RESTAURANT,
      std::optional<std::vector<IngredientBatch>> saved_ingredients =
          std::nullopt);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

protected:
  void draw_icon(const RenderContext& ctx);
  void draw_page_header(const RenderContext& ctx);
  void draw_page_screen(const RenderContext& ctx);
  void draw_ingredient_list(const RenderContext& ctx);
  void draw_depot_ingredient_list(const RenderContext& ctx);
  void draw_restaurant_ingredient_list(const RenderContext& ctx);
  void register_ingredient_row(ViewRect row_rect, size_t i);
  void set_up_input_panel() override;
  void place_order();

private:
  Money
  calculate_ingredient_price(core::food::IngredientType ingredient_type) const;
  Money calculate_total_cost() const;
  bool check_depot_quality(const IngredientBatch& batch) const;

  // Button hold handling helpers
  void handle_button_press(size_t ingredient_index, bool is_left_button);
  void handle_button_release(bool is_left_button);
  void update_held_button_repeat();
  void handle_scroll(size_t ingredient_index, int delta);

  IngredientCategory category_;
  IngredientScreenMode mode_;
  const FoodSupplier* supplier_{nullptr};
  std::vector<IngredientBatch> ingredients_;

  // For hover and continuous clicking
  std::optional<size_t> hovered_index_;
  std::optional<size_t> button_held_index_;
  bool left_button_held_{false};
  bool right_button_held_{false};
  enum class HoldDirection { Increment, Decrement };
  HoldDirection hold_direction_{HoldDirection::Increment};
  int button_held_frames_{0};
  static constexpr int REPEAT_DELAY = 6;       // Frames before repeat starts
  static constexpr int REPEAT_RATE_SINGLE = 2; // Frames between repeats
  static constexpr int FAST_STEP = 5;          // Step when Shift or dual-button
};
