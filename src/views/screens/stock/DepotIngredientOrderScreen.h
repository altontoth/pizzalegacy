/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/finance/Money.h"
#include "core/food/IngredientCategoryType.h"
#include "core/state/UIState.h"
#include "core/supply/IngredientBatch.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/MainPanelView.h"
#include "views/ui/status/StatusController.h"

#include <optional>
#include <vector>

class FoodSupplier;

class DepotIngredientOrderScreen : public MainPanelView,
                                   public input::InputListener {
public:
  explicit DepotIngredientOrderScreen(GameContext& context, GameState& state,
                                      const DepotIngredientOrderState& ds);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  void draw_icon(const RenderContext& ctx);
  void draw_page_header(const RenderContext& ctx);
  void draw_ingredient_list(const RenderContext& ctx);
  void register_ingredient_row(ViewRect row_rect, size_t i);
  void set_up_input_panel();
  void place_order();
  void charge_exit();

  Money
  calculate_ingredient_price(core::food::IngredientType ingredient_type) const;
  Money calculate_total_cost() const;
  bool check_depot_quality(const IngredientBatch& batch) const;

  void handle_button_press(size_t ingredient_index, bool is_left_button);
  void handle_button_release(bool is_left_button);
  void update_held_button_repeat();
  // Step size for a click or held button, from the modifiers currently down
  int step_for_modifiers(bool dual_button) const;
  void handle_scroll(size_t ingredient_index, int delta, bool fast);
  int max_increment_for(size_t ingredient_index) const;

  StatusController status_controller_;
  IngredientCategory category_;
  bool is_sell_;
  FoodSupplier* supplier_{nullptr};
  std::vector<IngredientBatch> ingredients_;
  input::ListenerToken listener_;

  std::optional<size_t> hovered_index_;
  std::optional<size_t> button_held_index_;
  bool left_button_held_{false};
  bool right_button_held_{false};
  enum class HoldDirection { Increment, Decrement };
  HoldDirection hold_direction_{HoldDirection::Increment};
  int button_held_frames_{0};
  static constexpr int REPEAT_DELAY = 6;
  static constexpr int REPEAT_RATE_SINGLE = 2;
  static constexpr int NORMAL_STEP = 1;
  // Shift, or both mouse buttons at once
  static constexpr int FAST_STEP = 5;
  // Ctrl
  static constexpr int FASTER_STEP = 50;
  // Shift + ctrl, or a sideways wheel notch
  static constexpr int FASTEST_STEP = 100;
};
