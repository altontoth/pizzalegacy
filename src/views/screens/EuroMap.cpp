/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/EuroMap.h"

#include <cmath>
#include <cstdlib>

#include "core/city/City.h"
#include "core/city/Property.h"
#include "core/common/Ids.h"
#include "core/state/GameState.h"
#include "engine/DisplayConstants.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionEvent.h"
#include "gfx/Blit.h"
#include "gfx/Draw.h"
#include "magic_enum/magic_enum.hpp"
#include "resources/FontManager.h"
#include "views/base/ViewDefs.h"

constexpr uint8_t dark_text_color = 20;
constexpr uint8_t box_bg_color = 24;

EuroMap::EuroMap(GameContext& context, GameState& state, MapMode mode,
                 std::optional<IngredientCategory> category)
    : View(context, state), map_state_(MapState::NO_ACTION),
      last_clicked_city_(Destination::NONE), map_mode_(mode),
      destination_rects_(
          {{Destination::PARIS, dest_box(ViewPoint{477, 188})},
           {Destination::BERLIN, dest_box(ViewPoint{582, 175})},
           {Destination::BALTIMORE, dest_box(ViewPoint{136, 161})},
           {Destination::VIENNA, dest_box(ViewPoint{568, 222})},
           {Destination::CHICAGO, dest_box(ViewPoint{15, 110})},
           {Destination::NEW_YORK, dest_box(ViewPoint{162, 123})},
           {Destination::MADRID, dest_box(ViewPoint{352, 325})},
           {Destination::ROME, dest_box(ViewPoint{555, 301})},
           {Destination::STOCKHOLM, dest_box(ViewPoint{605, 82})},
           {Destination::LONDON, dest_box(ViewPoint{449, 134})}}),
      destination_list_({Destination::PARIS, Destination::BERLIN,
                         Destination::BALTIMORE, Destination::VIENNA,
                         Destination::CHICAGO, Destination::NEW_YORK,
                         Destination::MADRID, Destination::ROME,
                         Destination::STOCKHOLM, Destination::LONDON}),
      category_(category) {

  // Where we want the white city popup box and how big it is:
  big_popup_box_.x = 140;
  big_popup_box_.y = 120;
  big_popup_box_.w = 320;
  big_popup_box_.h = 175;
  small_popup_box_.x = 208;
  small_popup_box_.y = 230;
  small_popup_box_.w = 242;
  small_popup_box_.h = 66;

  state_.pause_time_advancement();

  interactions_.create_persistent_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "euromap_fallback", [this](views::ui::InteractionEvent ev) {
        auto dest = Destination::NONE;
        if (ev.type == views::ui::InteractionEvent::Type::ButtonDown) {
          map_state_ = MapState::NO_ACTION;
          last_clicked_city_ = Destination::NONE;
          mark_dirty();
        }
      });
  for (auto dest : destination_list_) {
    auto r = destination_rects_[dest];
    interactions_.create_persistent_item(
        r, compat::format("euromap.{}", magic_enum::enum_name(dest)),
        [this, dest](views::ui::InteractionEvent ev) {
          switch (ev.type) {
          case views::ui::InteractionEvent::Type::ButtonDown:
            on_city_click(dest, ev.button);
            break;
          case views::ui::InteractionEvent::Type::Enter:
            on_city_hover(dest);
            break;
          case views::ui::InteractionEvent::Type::Leave:
            on_city_leave(dest);
            break;
          default:
            break;
          }
        });
  }
}

ViewRect EuroMap::dest_box(ViewPoint p) {
  ViewRect r;
  r.x = p.x - (CLICKABLE_AREA_AROUND_CITY_IN_PX / 2);
  r.y = p.y - (CLICKABLE_AREA_AROUND_CITY_IN_PX / 2);
  r.w = CLICKABLE_AREA_AROUND_CITY_IN_PX;
  r.h = CLICKABLE_AREA_AROUND_CITY_IN_PX;
  return r;
}

void EuroMap::tick() {}

void EuroMap::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  gfx::blit::blit_raw_with_palette(*ctx.surface, context_.vga->get("euromap"));

  if (map_state_ == MapState::CITY_CLICKED) {
    auto popup_surface = create_popup(ctx, big_popup_box_.w, big_popup_box_.h);

    if (map_mode_ == MapMode::DEPOT) {
      // No depot in target city
      context_.font->render_text_onto(popup_surface, "You do not have",
                                      ViewPoint{0, 30}, FontFace::ASTANDAR,
                                      dark_text_color, AlignX::Center);
      context_.font->render_text_onto(popup_surface, "a central depot",
                                      ViewPoint{0, 50}, FontFace::ASTANDAR,
                                      dark_text_color, AlignX::Center);
      context_.font->render_text_onto(
          popup_surface, "in " + destination_name(last_clicked_city_),
          ViewPoint{0, 70}, FontFace::ASTANDAR, dark_text_color,
          AlignX::Center);
    }
    else {
      context_.font->render_text_onto(
          popup_surface, destination_name(last_clicked_city_), ViewPoint{0, 30},
          FontFace::ASTANDAR, dark_text_color, AlignX::Center);
      context_.font->render_text_onto(popup_surface, "market leader",
                                      ViewPoint{0, 50}, FontFace::ASTANDAR,
                                      dark_text_color, AlignX::Center);
      auto city_id = core::CityId{static_cast<int>(last_clicked_city_)};
      std::string leader_name = "- nobody -";
      if (state_.is_city_loaded(city_id)) {
        auto leader = state_.city(city_id)->control_map().market_leader();
        if (leader) {
          auto* player = state_.player(*leader);
          if (player) {
            leader_name = player->name();
          }
        }
      }
      context_.font->render_text_onto(popup_surface, leader_name,
                                      ViewPoint{0, 70}, FontFace::ASTANDAR,
                                      dark_text_color, AlignX::Center);
    }

    ctx.surface->blit_raw(popup_surface,
                          ViewPoint{big_popup_box_.x, big_popup_box_.y});
  }
  else if (map_state_ == MapState::CITY_CONFIRMED) {
    auto popup_surface =
        create_popup(ctx, small_popup_box_.w, small_popup_box_.h);
    auto current_city = static_cast<Destination>(state_.active_city_id().value);
    int cost = calculate_cost(current_city, last_clicked_city_);

    if (map_mode_ == MapMode::DEPOT) {
      int city_id = static_cast<int>(last_clicked_city_);
      core::CityId id{city_id};
      auto* dest_city = state_.city(id);
      if (state_.options().fix_legacy_bugs && state_.is_city_loaded(id) &&
          dest_city->trade_embargo_counter(*category_)) {
        auto city_name = destination_name(last_clicked_city_);
        context_.font->render_text_onto(popup_surface, city_name + " is",
                                        ViewPoint{0, 12}, FontFace::ASTANDAR,
                                        dark_text_color, AlignX::Center);
        context_.font->render_text_onto(popup_surface, "under embargo!",
                                        ViewPoint{0, 36}, FontFace::ASTANDAR,
                                        dark_text_color, AlignX::Center);
        ctx.surface->blit_raw(
            popup_surface, ViewPoint{small_popup_box_.x, small_popup_box_.y});
        interactions_.create_frame_item(
            ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
                     DisplayConstants::SCREEN_HEIGHT},
            "embargo_box", [this](views::ui::InteractionEvent ie) {
              if (ie.type != views::ui::InteractionEvent::Type::Click) {
                return;
              }
              map_state_ = MapState::NO_ACTION;
              last_clicked_city_ = Destination::NONE;
              mark_dirty();
            });
        return;
      }
      context_.font->render_text_onto(popup_surface, "Export will cost",
                                      ViewPoint{0, 12}, FontFace::ASTANDAR,
                                      dark_text_color, AlignX::Center);
      context_.font->render_text_onto(
          popup_surface, "$" + std::to_string(cost) + ".", ViewPoint{0, 36},
          FontFace::ASTANDAR, dark_text_color, AlignX::Center);
      interactions_.create_frame_item(
          ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
                   DisplayConstants::SCREEN_HEIGHT},
          "export_cost_box", [this, cost](views::ui::InteractionEvent ie) {
            if (ie.type != views::ui::InteractionEvent::Type::Click) {
              return;
            }
            auto target_id = core::CityId{static_cast<int>(last_clicked_city_)};
            state_.ui().replace_top<DepotExportState>(*category_, target_id,
                                                      cost);
          });
    }
    else {
      context_.font->render_text_onto(popup_surface, "The flight costs",
                                      ViewPoint{0, 12}, FontFace::ASTANDAR,
                                      dark_text_color, AlignX::Center);
      context_.font->render_text_onto(
          popup_surface, "$" + std::to_string(cost) + ".", ViewPoint{0, 36},
          FontFace::ASTANDAR, dark_text_color, AlignX::Center);
      interactions_.create_frame_item(
          small_popup_box_, "flight_cost_box",
          [this](views::ui::InteractionEvent ie) {
            if (ie.type != views::ui::InteractionEvent::Type::Click) {
              return;
            }
            state_.ui().push<CityViewState>(
                core::CityId{static_cast<int>(last_clicked_city_)});
          });
    }

    ctx.surface->blit_raw(popup_surface,
                          ViewPoint{small_popup_box_.x, small_popup_box_.y});
  }
  else if (hovered_city_ != Destination::NONE) {
    context_.font->render_text_onto(
        *ctx.surface, destination_name(hovered_city_), ViewPoint{-40, 190},
        FontFace::ASTANDAR, WHITE, AlignX::Center);
  }
}

std::string EuroMap::destination_name(Destination id) {
  std::string name;
  switch (id) {
  case Destination::PARIS:
    name = "Paris";
    break;
  case Destination::BERLIN:
    name = "Berlin";
    break;
  case Destination::BALTIMORE:
    name = "Baltimore";
    break;
  case Destination::VIENNA:
    name = "Vienna";
    break;
  case Destination::CHICAGO:
    name = "Chicago";
    break;
  case Destination::NEW_YORK:
    name = "New York";
    break;
  case Destination::MADRID:
    name = "Madrid";
    break;
  case Destination::ROME:
    name = "Rome";
    break;
  case Destination::STOCKHOLM:
    name = "Stockholm";
    break;
  case Destination::LONDON:
    name = "London";
    break;
  default:
    name = "Unknown";
    break;
  }
  return name;
}

Surface EuroMap::create_popup(const RenderContext& ctx, int w, int h) {
  auto box = Surface(w, h, PaletteHandle{2}, box_bg_color);
  gfx::draw::beveled_box(box, {0, 0, w, h}, 23, 32, 4);

  return box;
}

int EuroMap::calculate_cost(Destination from, Destination to) {

  const auto& from_item = destination_rects_.at(from);
  const auto& to_item = destination_rects_.at(to);

  // Calculate Manhattan distance.
  int dx = std::abs(from_item.x - to_item.x);
  int dy = std::abs(from_item.y - to_item.y);
  int manhattan = dx + dy;

  // Get the difficulty level.
  // For our model, the multiplier is (difficulty + 1) / 2.
  // For difficulty 1: (1+1)/2 = 1, difficulty 2: (2+1)/2 = 1.5, etc.
  float difficulty = 1.0f + 0.5f * state_.difficulty_level();

  int cost = manhattan * (difficulty + 1) / 2;

  return cost;
}

void EuroMap::on_city_click(Destination dest, views::ui::MouseButton btn) {
  if (btn != views::ui::MouseButton::Left) {
    return;
  }

  if (map_mode_ == MapMode::DEPOT) {
    auto current_city = static_cast<Destination>(state_.active_city_id().value);
    if (dest == current_city) {
      state_.ui().pop();
      return;
    }
    last_clicked_city_ = dest;
    if (player_has_depot_in_city(dest)) {
      map_state_ = MapState::CITY_CONFIRMED;
    }
    else {
      map_state_ = MapState::CITY_CLICKED;
    }
    mark_dirty();
    return;
  }

  if (map_state_ == MapState::CITY_CLICKED && last_clicked_city_ == dest) {
    auto game_last_clicked_city_ =
        static_cast<Destination>(state_.active_city_id().value);

    // If they are picking a location for the first time
    if (map_mode_ == MapMode::GAME_INIT) {
      state_.initializer().city_id = core::CityId{static_cast<int>(dest)};
      state_.finalize_initialization(context_);
      return;
    }
    // or they clicked the city they were already in:
    if (game_last_clicked_city_ == dest) {
      state_.ui().push<CityViewState>(core::CityId{static_cast<int>(dest)});
      return;
    }
    map_state_ = MapState::CITY_CONFIRMED;
  }
  else if (map_state_ == MapState::CITY_CLICKED) {
    map_state_ = MapState::NO_ACTION;
  }
  else {
    map_state_ = MapState::CITY_CLICKED;
  }
  last_clicked_city_ = dest;
  mark_dirty();
}

void EuroMap::on_city_hover(Destination dest) {
  if (map_state_ == MapState::NO_ACTION ||
      map_state_ == MapState::CITY_HOVERED) {
    map_state_ = MapState::CITY_HOVERED;
    mark_dirty();
  }
  hovered_city_ = dest;
}

void EuroMap::on_city_leave(Destination dest) {
  if (map_state_ == MapState::CITY_HOVERED && hovered_city_ == dest) {
    hovered_city_ = Destination::NONE;
    map_state_ = MapState::NO_ACTION;
  }
  hovered_city_ = Destination::NONE;
  mark_dirty();
}

bool EuroMap::player_has_depot_in_city(Destination dest) {
  auto city_id = core::CityId{static_cast<int>(dest)};
  if (!state_.is_city_loaded(city_id)) {
    return false;
  }
  auto* city = state_.city(city_id);
  if (!city) {
    return false;
  }
  auto player_id = state_.active_player()->id();
  for (auto* property : city->properties()) {
    if (property->type() == PropertyType::DEPOT) {
      auto owner = property->owner_or_tenant();
      if (owner && *owner == player_id) {
        return true;
      }
    }
  }
  return false;
}
