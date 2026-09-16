/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <string>
#include "core/common/Ids.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/AnimatedStatsScreen.h"
#include "engine/geom/Spaces.h"
#include "resources/Bob.h"
#include "views/base/RestaurantSubView.h"

class AnimatedStats : public RestaurantSubView {
public:
  AnimatedStats(GameContext& context, GameState& state,
                core::RestaurantId restaurant_id,
                AnimatedStatsScreen screen = AnimatedStatsScreen::Production);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  void create_buttons();
  void render_reputation(const RenderContext& ctx);
  void render_pizza_quality(const RenderContext& ctx);
  void render_capacity(const RenderContext& ctx);
  void render_popularity(const RenderContext& ctx);
  void render_turnover(const RenderContext& ctx);
  void render_production(const RenderContext& ctx);
  void reset_animation();
  void navigate_to_restaurant(core::RestaurantId new_id) override;

  // Shared thermometer + arrow rendering helpers, ported from the original.
  void draw_thermometer(const RenderContext& ctx) const;
  void draw_thermometer_fill(const RenderContext& ctx, int fill_level) const;
  void draw_component_arrow(const RenderContext& ctx, ViewPoint arrow_pos,
                            bool is_left) const;

  void draw_decorated_background(const RenderContext& ctx,
                                 uint8_t bg_color) const;

  void add_tooltip(ViewRect area, std::string id, std::string text);

  Restaurant* restaurant_{nullptr};
  AnimatedStatsScreen current_screen_{AnimatedStatsScreen::Production};
  AnimatedStatsScreen previous_screen_{AnimatedStatsScreen::None};
  Bob& lebbo_bob_;

  // Unified animation state - each screen uses what it needs
  int anim_progress_{
      0}; // Fill progress in units (thermometer level per component)
  int anim_phase_{0}; // For multi-phase animations (capacity screen)
  int anim_sub_progress_{
      0}; // Which component is animating (e.g., which demographic)
  int anim_tick_{
      0}; // Raw tick counter (for smooth chevron animation + speed control)

  // Tick handlers for each animated screen
  void tick_production();
  void tick_capacity();
  void tick_popularity();
  void tick_reputation();
  void tick_pizza_quality();

  // Production screen constants
  static constexpr int PRODUCTION_TOTAL_CYCLES = 4;
  static constexpr int PRODUCTION_FRAMES_PER_ARROW = 4;
  static constexpr int PRODUCTION_TOTAL_FRAMES =
      PRODUCTION_TOTAL_CYCLES * 4 * PRODUCTION_FRAMES_PER_ARROW;

  static constexpr int CAPACITY_DEMAND_BAR_SPEED = 4; // Pixels per tick
  static constexpr int CAPACITY_RED_FILL_SPEED = 1;   // Rows per tick
  static constexpr int CAPACITY_BAR_SPEED = 4;        // Pixels per tick
  static constexpr int CAPACITY_BAR_HEIGHT = 18;      // Rows per category bar
  static constexpr int CAPACITY_NUM_CATEGORIES = 5;
  static constexpr int CAPACITY_MAX_BAR_WIDTH = 140; // Max pixels
  // Capacity phases: 0=DemandBar, 1=RedFill, 2=CapacityBar, 3+=done

  static constexpr int POPULARITY_FILL_SPEED = 2; // Rows per tick
  static constexpr int POPULARITY_NUM_DEMOGRAPHICS = 6;
  static constexpr int POPULARITY_BAR_FILL_START = 278 - 32;
  static constexpr int POPULARITY_BAR_FILL_MIN = 132 - 32;

  // Returns the target Y position for a bar with the given strength value.
  // Higher value = bar filled higher = lower Y coordinate.
  static int popularity_target_y(int value) {
    return std::clamp(POPULARITY_BAR_FILL_START - (value / 2),
                      POPULARITY_BAR_FILL_MIN, POPULARITY_BAR_FILL_START);
  }

  static constexpr int ANIM_TICKS_PER_UNIT = 4;
  static constexpr int REPUTATION_NUM_COMPONENTS = 8;
  static constexpr int PIZZA_QUALITY_NUM_COMPONENTS = 4;

  // Seating, Stock, Chef, Oven, Waiter
  std::array<int, 5> capacity_category_values_{0, 0, 0, 0, 0};
  int capacity_final_value_{0};

  // Caches (calculated once per screen switch)
  int capacity_demand_value_{0};
  std::array<int, 4> pizza_quality_values_{};
  ProductionScreenData production_data_{};

  void calculate_capacity_stats();
};
