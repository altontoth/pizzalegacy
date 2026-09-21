/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include "core/GameTypes.h"
#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/food/IngredientCategoryType.h"
#include "engine/DebugLabel.h"
#include "engine/geom/Spaces.h"
#include "views/base/View.h"

#define CLICKABLE_AREA_AROUND_CITY_IN_PX 30

// What state is the UI in:
enum class MapState {
  NO_ACTION,
  CITY_CLICKED,
  CITY_CONFIRMED,
  PRICE_AGREED,
};

/// This class is called EuroMap despite also having a chunk of the US in there
/// because the original German version of the game only had Europe.
class EuroMap : public View, public debug::AutoLabel<EuroMap> {
public:
  EuroMap(GameContext& context, GameState& state, MapMode mode,
          std::optional<IngredientCategory> category = std::nullopt);
  void tick() override;
  void render(const RenderContext& ctx) override;
  ClockRate clock_rate() const override { return ClockRate::Stopped; }

private:
  ViewRect dest_box(ViewPoint p);
  Surface create_popup(const RenderContext& ctx, int, int);
  Money calculate_cost(core::CityId from, core::CityId to);
  void on_city_click(core::CityId city, views::ui::MouseButton btn);
  bool player_has_depot_in_city(core::CityId city_id);
  void attempt_flight();

  MapState map_state_{MapState::NO_ACTION};
  ViewRect big_popup_box_;
  ViewRect small_popup_box_;
  std::optional<core::CityId> last_clicked_city_;
  MapMode map_mode_;
  std::optional<IngredientCategory> category_;
};
