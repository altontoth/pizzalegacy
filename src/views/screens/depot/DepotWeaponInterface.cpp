/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/depot/DepotWeaponInterface.h"

#include "core/city/Property.h"
#include "core/item/Item.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionEvent.h"
#include "gfx/Blit.h"
#include "resources/FontManager.h"
#include "views/ui/DropdownDecoration.h"

using namespace views::screens::depot;

DepotWeaponInterface::DepotWeaponInterface(GameContext& context,
                                           GameState& state,
                                           DepotWeaponInterfaceState dwis)
    : MainPanelView(context, state), status_controller_(state.status()),
      property_(dwis.property) {
  set_up_input_panel();
  status_controller_.show_city_default(
      {.state = &state_, .city = state_.active_city()});
}

void DepotWeaponInterface::tick() {}

void DepotWeaponInterface::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back14"));
  Surface text_box(252, 226, ctx.surface->palette());
  text_box.fill({13});
  views::ui::dropdowndecoration::draw_dropdown_decoration(text_box, 0);

  const int text_color = 240;
  const int right_edge = text_box.width();

  int y_position = 28;
  context_.font->render_text_onto(text_box, "category", {10, y_position},
                                  FontFace::PIZ_NUMB, text_color);
  context_.font->render_text_onto(
      text_box, "bag", {right_edge - 70, y_position}, FontFace::PIZ_NUMB,
      text_color, TextAnchor::End);
  context_.font->render_text_onto(
      text_box, "depot", {right_edge - 4, y_position}, FontFace::PIZ_NUMB,
      text_color, TextAnchor::End);

  auto* player = state_.active_player();
  auto* depot_weapons = property_->weapons();

  y_position += 32;
  constexpr ViewPoint text_box_offset{66, 68};

  for (auto weapon : core::item::weapon_list) {
    auto bag_count = player->inventory().count(weapon);
    auto depot_count = depot_weapons ? depot_weapons->count(weapon) : 0;

    ViewRect hover_box{6, y_position, 236, 18};

    if (hovered_weapon_ && *hovered_weapon_ == weapon) {
      text_box.fill_rect(hover_box, {15});
    }

    auto name = to_string(weapon);
    // Text positioned 1px down for vertical centering (1px + text + 1px)
    int text_y = y_position + 1;
    context_.font->render_text_onto(text_box, name, {10, text_y},
                                    FontFace::PIZ_NUMB, text_color);
    context_.font->render_text_onto(
        text_box, std::to_string(bag_count), {right_edge - 74, text_y},
        FontFace::PIZ_NUMB, text_color, TextAnchor::End);
    context_.font->render_text_onto(
        text_box, std::to_string(depot_count), {right_edge - 20, text_y},
        FontFace::PIZ_NUMB, text_color, TextAnchor::End);

    // Interaction box needs absolute coordinates
    ViewRect hover_box_absolute{hover_box.x + text_box_offset.x,
                                hover_box.y + text_box_offset.y, hover_box.w,
                                hover_box.h};

    interactions_.create_frame_item(
        hover_box_absolute, name,
        [this, weapon, depot_weapons](views::ui::InteractionEvent ev) {
          if (ev.type == ui::InteractionEvent::Type::Enter) {
            hovered_weapon_ = weapon;
            mark_dirty();
          }
          else if (ev.type == ui::InteractionEvent::Type::Leave) {
            hovered_weapon_ = std::nullopt;
            mark_dirty();
          }
          else if (ev.type == views::ui::InteractionEvent::Type::Click) {
            auto* player = state_.active_player();
            if (ev.button == views::ui::MouseButton::Left) {
              // Move from depot to bag (as long as depot > 0 and bag < 255)
              if (depot_weapons) {
                int depot_count = depot_weapons->count(weapon);
                int bag_count = player->inventory().count(weapon);
                if (depot_count > 0 && bag_count < 255) {
                  depot_weapons->remove(weapon, 1);
                  player->inventory().add(weapon, 1);
                  mark_dirty();
                }
              }
            }
            else if (ev.button == views::ui::MouseButton::Right) {
              // Move from bag to depot (as long as bag > 0--this has an
              // overflow risk that is also present in the original but given
              // you need 65K clicks to get there I don't care)
              int bag_count = player->inventory().count(weapon);
              if (bag_count > 0 && depot_weapons) {
                player->inventory().remove(weapon, 1);
                depot_weapons->add(weapon, 1);
                mark_dirty();
              }
            }
          }
        });

    y_position += 18;
  }

  views::ui::dropdowndecoration::draw_dropdown_decoration(
      text_box, text_box.height() - 20);
  ctx.surface->blit_raw(text_box, ViewPoint{66, 68});
}

void DepotWeaponInterface::set_up_input_panel() {
  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this]() { state_.ui().pop(); });
}
