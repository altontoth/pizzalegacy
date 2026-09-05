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
#include "core/city/CityNames.h"
#include "core/city/Property.h"
#include "core/common/Ids.h"
#include "core/state/GameState.h"
#include "core/util/Constants.h"
#include "engine/DisplayConstants.h"
#include "engine/FormatCompat.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/Blit.h"
#include "engine/gfx/Draw.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/FontManager.h"
#include "views/base/ViewDefs.h"

constexpr uint8_t dark_text_color = 20;
constexpr uint8_t box_bg_color = 24;

// Indexed by core::CityId. Our render coordinates are 2x these.
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
    : View(context, state), map_mode_(mode), category_(category) {

  // Where we want the white city popup box and how big it is:
  big_popup_box_.x = 140;
  big_popup_box_.y = 120;
  big_popup_box_.w = 320;
  big_popup_box_.h = 175;
  small_popup_box_.x = 208;
  small_popup_box_.y = 230;
  small_popup_box_.w = 242;
  small_popup_box_.h = 66;

  interactions_.create_persistent_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "euromap_fallback", [this](views::ui::InteractionEvent ev) {
        if (ev.type == views::ui::InteractionEvent::Type::ButtonDown) {
          map_state_ = MapState::NO_ACTION;
          last_clicked_city_.reset();
          mark_dirty();
        }
      });
  for (int i = 0; i < NUMBER_OF_CITIES; ++i) {
    core::CityId city{i};
    auto r = dest_box(ViewPoint{city_coords[i].x * 2, city_coords[i].y * 2});
    interactions_.create_persistent_item(
        r, compat::format("euromap.{}", core::city_name(city)),
        [this, city](views::ui::InteractionEvent ev) {
          switch (ev.type) {
          case views::ui::InteractionEvent::Type::ButtonDown:
            on_city_click(city, ev.button);
            break;
          case views::ui::InteractionEvent::Type::Enter:
            on_city_hover(city);
            break;
          case views::ui::InteractionEvent::Type::Leave:
            on_city_leave(city);
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
    const int center_x = popup_surface.rect().center().x;
    auto target = *last_clicked_city_;

    if (map_mode_ == MapMode::DEPOT) {
      // No depot in target city
      context_.font->render_text_onto(
          popup_surface, "You do not have", ViewPoint{center_x, 30},
          FontFace::ASTANDAR, dark_text_color, TextAnchor::Middle);
      context_.font->render_text_onto(
          popup_surface, "a central depot", ViewPoint{center_x, 50},
          FontFace::ASTANDAR, dark_text_color, TextAnchor::Middle);
      context_.font->render_text_onto(
          popup_surface, compat::format("in {}", core::city_name(target)),
          ViewPoint{center_x, 70}, FontFace::ASTANDAR, dark_text_color,
          TextAnchor::Middle);
    }
    else {
      context_.font->render_text_onto(
          popup_surface, core::city_name(target), ViewPoint{center_x, 30},
          FontFace::ASTANDAR, dark_text_color, TextAnchor::Middle);
      context_.font->render_text_onto(
          popup_surface, "market leader", ViewPoint{center_x, 50},
          FontFace::ASTANDAR, dark_text_color, TextAnchor::Middle);
      std::string leader_name = "- nobody -";
      if (state_.is_city_loaded(target)) {
        auto leader = state_.city(target)->control_map().market_leader();
        if (leader) {
          auto* player = state_.player(*leader);
          assert(player);
          leader_name = player->name();
        }
      }
      context_.font->render_text_onto(
          popup_surface, leader_name, ViewPoint{center_x, 70},
          FontFace::ASTANDAR, dark_text_color, TextAnchor::Middle);
    }

    ctx.surface->blit_raw(popup_surface,
                          ViewPoint{big_popup_box_.x, big_popup_box_.y});
  }
  else if (map_state_ == MapState::CITY_CONFIRMED) {
    auto popup_surface =
        create_popup(ctx, small_popup_box_.w, small_popup_box_.h);
    const int center_x = popup_surface.rect().center().x;
    auto target = *last_clicked_city_;
    Money cost = calculate_cost(state_.active_city_id(), target);

    if (map_mode_ == MapMode::DEPOT) {
      auto* dest_city = state_.city(target);
      if (state_.options().fix_legacy_bugs && state_.is_city_loaded(target) &&
          dest_city->trade_embargo_counter(*category_)) {
        context_.font->render_text_onto(
            popup_surface, compat::format("{} is", core::city_name(target)),
            ViewPoint{center_x, 12}, FontFace::ASTANDAR, dark_text_color,
            TextAnchor::Middle);
        context_.font->render_text_onto(
            popup_surface, "under embargo!", ViewPoint{center_x, 36},
            FontFace::ASTANDAR, dark_text_color, TextAnchor::Middle);
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
              last_clicked_city_.reset();
              mark_dirty();
            });
        return;
      }
      context_.font->render_text_onto(
          popup_surface, "Export will cost", ViewPoint{center_x, 12},
          FontFace::ASTANDAR, dark_text_color, TextAnchor::Middle);
      context_.font->render_text_onto(
          popup_surface, compat::format("{:w}.", cost), ViewPoint{center_x, 36},
          FontFace::ASTANDAR, dark_text_color, TextAnchor::Middle);
      interactions_.create_frame_item(
          ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
                   DisplayConstants::SCREEN_HEIGHT},
          "export_cost_box",
          [this, target, cost](views::ui::InteractionEvent ie) {
            if (ie.type != views::ui::InteractionEvent::Type::Click) {
              return;
            }
            state_.ui().replace_top<DepotExportState>(*category_, target, cost);
          });
    }
    else {
      bool can_afford = state_.active_player()->cash_balance() >= cost;
      if (can_afford) {
        context_.font->render_text_onto(
            popup_surface, "The flight costs", ViewPoint{center_x, 12},
            FontFace::ASTANDAR, dark_text_color, TextAnchor::Middle);
        context_.font->render_text_onto(
            popup_surface, compat::format("{:w}.", cost),
            ViewPoint{center_x, 36}, FontFace::ASTANDAR, dark_text_color,
            TextAnchor::Middle);
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
        context_.font->render_text_onto(
            popup_surface, "Not enough money!", ViewPoint{center_x, 24},
            FontFace::ASTANDAR, dark_text_color, TextAnchor::Middle);
        interactions_.create_frame_item(
            small_popup_box_, "flight_cost_box",
            [this](views::ui::InteractionEvent ie) {
              if (ie.type != views::ui::InteractionEvent::Type::Click) {
                return;
              }
              map_state_ = MapState::NO_ACTION;
              last_clicked_city_.reset();
              mark_dirty();
            });
      }
    }

    ctx.surface->blit_raw(popup_surface,
                          ViewPoint{small_popup_box_.x, small_popup_box_.y});
  }
  else if (hovered_city_) {
    constexpr int text_color = 255;
    const int center_x = ctx.surface->rect().center().x;
    context_.font->render_text_onto(
        *ctx.surface, core::city_name(*hovered_city_),
        ViewPoint{center_x - 40, 190}, FontFace::ASTANDAR, text_color,
        TextAnchor::Middle);
  }
}

Surface EuroMap::create_popup(const RenderContext& ctx, int w, int h) {
  auto box = Surface(w, h, PaletteHandle{2}, box_bg_color);
  gfx::draw::beveled_box(box, {0, 0, w, h}, 23, 32, 4);

  return box;
}

Money EuroMap::calculate_cost(core::CityId from, core::CityId to) {
  int dx = std::abs(city_coords[from.value].x - city_coords[to.value].x);
  int dy = std::abs(city_coords[from.value].y - city_coords[to.value].y);
  int manhattan = dx + dy;

  // Original used difficulty 1-5 with multiplier (difficulty + 1).
  return Money::from_dollars(manhattan * (state_.difficulty_level() + 1));
}

void EuroMap::on_city_click(core::CityId city, views::ui::MouseButton btn) {
  if (btn != views::ui::MouseButton::Left) {
    return;
  }

  if (map_mode_ == MapMode::DEPOT) {
    if (city == state_.active_city_id()) {
      state_.ui().pop();
      return;
    }
    last_clicked_city_ = city;
    if (player_has_depot_in_city(city)) {
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
  if (state_.options().qol_improvements &&
      map_state_ == MapState::CITY_CONFIRMED && last_clicked_city_ == city) {
    attempt_flight();
    return;
  }

  if (map_state_ == MapState::CITY_CLICKED && last_clicked_city_ == city) {
    // If they are picking a location for the first time
    if (map_mode_ == MapMode::GAME_INIT) {
      state_.initializer().city_id = city;
      state_.finalize_initialization(context_);
      return;
    }
    // or they clicked the city they were already in:
    if (city == state_.active_city_id()) {
      state_.ui().push<CityViewState>(city);
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
  last_clicked_city_ = city;
  mark_dirty();
}

void EuroMap::on_city_hover(core::CityId city) {
  if (map_state_ == MapState::NO_ACTION ||
      map_state_ == MapState::CITY_HOVERED) {
    map_state_ = MapState::CITY_HOVERED;
    mark_dirty();
  }
  hovered_city_ = city;
}

void EuroMap::on_city_leave(core::CityId city) {
  if (map_state_ == MapState::CITY_HOVERED && hovered_city_ == city) {
    map_state_ = MapState::NO_ACTION;
  }
  hovered_city_.reset();
  mark_dirty();
}

void EuroMap::attempt_flight() {
  auto target = *last_clicked_city_;
  Money cost = calculate_cost(state_.active_city_id(), target);
  if (!state_.spend_cash(Account::Others, cost)) {
    return;
  }

  if (auto* player = state_.active_player()) {
    player->on_city_travel();
  }

  // Flying to another city costs money instead of time:
  state_.pending_screen_charge(0);
  state_.ui().push<CityViewState>(target);
}

bool EuroMap::player_has_depot_in_city(core::CityId city_id) {
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
