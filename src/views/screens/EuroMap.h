/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <map>
#include <optional>
#include <vector>
#include "core/GameTypes.h"
#include "core/food/IngredientCategoryType.h"
#include "engine/DebugLabel.h"
#include "engine/geom/Spaces.h"
#include "views/base/View.h"

#define CLICKABLE_AREA_AROUND_CITY_IN_PX 30

// What state is the UI in:
enum class MapState {
  NO_ACTION,
  CITY_HOVERED,
  CITY_CLICKED,
  CITY_CONFIRMED,
  PRICE_AGREED,
};

enum class Destination {
  PARIS,
  BERLIN,
  BALTIMORE,
  VIENNA,
  CHICAGO,
  NEW_YORK,
  MADRID,
  ROME,
  STOCKHOLM,
  LONDON,
  NONE,
};

/// This class is called EuroMap despite also having a chunk of the US in there
/// because the original German version of the game only had Europe.
class EuroMap : public View, public debug::AutoLabel<EuroMap> {
public:
  EuroMap(GameContext& context, GameState& state, MapMode mode,
          std::optional<IngredientCategory> category = std::nullopt);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  void handle_mouse_click(ViewPoint p);
  void handle_mouse_hover(ViewPoint p);
  ViewRect dest_box(ViewPoint p);
  std::string destination_name(Destination id);
  Surface create_popup(const RenderContext& ctx, int, int);
  int calculate_cost(Destination from, Destination to);
  void on_city_click(Destination dest, views::ui::MouseButton btn);
  void on_city_hover(Destination dest);
  void on_city_leave(Destination dest);
  bool player_has_depot_in_city(Destination dest);
  void attempt_flight();

  MapState map_state_{MapState::NO_ACTION};
  ViewRect big_popup_box_;
  ViewRect small_popup_box_;
  Destination last_clicked_city_{Destination::NONE};
  MapMode map_mode_;
  Destination hovered_city_{Destination::NONE};
  std::map<Destination, ViewRect> destination_rects_;
  std::vector<Destination> destination_list_;
  std::optional<IngredientCategory> category_;
};
