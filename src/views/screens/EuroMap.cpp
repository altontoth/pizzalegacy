/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/EuroMap.h"

#include <cmath>
#include <cstdlib>

#include "core/character/Player.h"
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

// Destination enum value. Our render coordinates are 2x these.
struct CityCoord {
  int x, y;
};
constexpr CityCoord city_coords[] = {
    {.x = 237, .y = 95},  // Paris
    {.x = 289, .y = 88},  // Berlin
    {.x = 67, .y = 80},   // Baltimore
    {.x = 282, .y = 111}, // Vienna
    {.x = 7, .y = 53},    // Chicago
    {.x = 79, .y = 61},   // New York
    {.x = 175, .y = 162}, // Madrid
    {.x = 276, .y = 149}, // Rome
    {.x = 301, .y = 42},  // Stockholm
    {.x = 223, .y = 67},  // London
};

EuroMap::EuroMap(GameContext& context, GameState& state, MapMode mode,
                 std::optional<IngredientCategory> category)
    : View(context, state), map_state_(MapState::NO_ACTION),
      last_clicked_city_(Destination::NONE), map_mode_(mode),
      destination_list_({Destination::PARIS, Destination::BERLIN,
                         Destination::BALTIMORE, Destination::VIENNA,
                         Destination::CHICAGO, Destination::NEW_YORK,
                         Destination::MADRID, Destination::ROME,
                         Destination::STOCKHOLM, Destination::LONDON}),
      category_(category) {

  for (auto dest : destination_list_) {
    auto i = static_cast<int>(dest);
    destination_rects_[dest] =
        dest_box(ViewPoint{city_coords[i].x * 2, city_coords[i].y * 2});
  }

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
          assert(player);
          leader_name = player->name();
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
      Money amount{static_cast<int64_t>(cost) * 100};
      bool can_afford = state_.active_player()->cash_balance() >= amount;
      if (can_afford) {
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
              attempt_flight();
            });
      }
      else {
        context_.font->render_text_onto(popup_surface, "Not enough money!",
                                        ViewPoint{0, 24}, FontFace::ASTANDAR,
                                        dark_text_color, AlignX::Center);
        interactions_.create_frame_item(
            small_popup_box_, "flight_cost_box",
            [this](views::ui::InteractionEvent ie) {
              if (ie.type != views::ui::InteractionEvent::Type::Click) {
                return;
              }
              map_state_ = MapState::NO_ACTION;
              last_clicked_city_ = Destination::NONE;
              mark_dirty();
            });
      }
    }

    ctx.surface->blit_raw(popup_surface,
                          ViewPoint{small_popup_box_.x, small_popup_box_.y});
  }
  else if (hovered_city_ != Destination::NONE) {
    constexpr int text_color = 255;
    context_.font->render_text_onto(
        *ctx.surface, destination_name(hovered_city_), ViewPoint{-40, 190},
        FontFace::ASTANDAR, text_color, AlignX::Center);
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
  auto fi = static_cast<int>(from);
  auto ti = static_cast<int>(to);
  int dx = std::abs(city_coords[fi].x - city_coords[ti].x);
  int dy = std::abs(city_coords[fi].y - city_coords[ti].y);
  int manhattan = dx + dy;

  // Original used difficulty 1-5 with multiplier (difficulty + 1).
  return manhattan * (state_.difficulty_level() + 1);
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

  // QoL: clicking the city a third time (while the price box is shown)
  // acts as accepting the price, saving the user from having to figure out that
  // they have to click the white box with the price (which is not communicated
  // to them in any way).
  if (context_.config->options().qol_improvements &&
      map_state_ == MapState::CITY_CONFIRMED && last_clicked_city_ == dest) {
    attempt_flight();
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

void EuroMap::attempt_flight() {
  auto current_city = static_cast<Destination>(state_.active_city_id().value);
  int cost = calculate_cost(current_city, last_clicked_city_);
  Money amount{static_cast<int64_t>(cost) * 100};
  if (!state_.spend_cash(Account::Others, amount)) {
    return;
  }

  if (auto* player = state_.active_player()) {
    player->on_city_travel();
  }

  state_.ui().push<CityViewState>(
      core::CityId{static_cast<int>(last_clicked_city_)});
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
