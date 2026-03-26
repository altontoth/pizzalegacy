/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/base/Menu.h"

#include "core/state/GameState.h"
#include "engine/DisplayConstants.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/Alignment.h"
#include "engine/ui/TooltipRequest.h"
#include "gfx/Blit.h"
#include "gfx/Draw.h"
#include "gfx/Surface.h"
#include "resources/FontManager.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"

using namespace views::base;
using DisplayConstants::SCREEN_HEIGHT;
using DisplayConstants::SCREEN_WIDTH;

Menu::Menu(GameContext& context, GameState& state, ViewPoint origin,
           int line_spacing)
    : View(context, state), menu_bobs(context_.bob->get("bob/menubobs.bob")),
      origin_in_view_(origin), line_spacing_(line_spacing) {

  update_selection(nullptr);
  state.pause_time_advancement();
}

std::optional<std::size_t> Menu::index_of_selected() const {
  for (std::size_t i = 0; i < menu_items_.size(); ++i) {
    if (menu_items_[i].get() == selected_option_) {
      return i;
    }
  }
  return std::nullopt;
}

void Menu::update_interactions() {
  if (ui_layout_dirty_) {
    ui_layout_dirty_ = false;
    mark_dirty(); // Make sure we repaint
    layout_.clear();
    interactions_.clear_persistent();

    int x = origin_in_view_.x;
    int y = origin_in_view_.y;
    int i = 0;
    for (auto& menu_item_ptr : menu_items_) {
      auto* menu_item = menu_item_ptr.get();
      if (dynamic_cast<PositionMarkerMenuItem*>(menu_item) != nullptr) {
        // Encountered a column marker, move to the next column, use
        // width and height as x and y (yes, hacky):
        Size size = menu_item->measure();
        int new_x = size.w;
        int new_y = size.h;
        x = origin_in_view_.x + new_x;
        if (new_y >= 0) {
          y = origin_in_view_.y + new_y;
        }
        continue;
      }
      bool is_selected = false;

      Size size = menu_item->measure();
      ViewPoint origin = views::ui::align_in_rect(
          ViewRect{{x, y}, {SCREEN_WIDTH, SCREEN_HEIGHT}}, size,
          menu_item->alignment());
      ViewPoint new_pos{origin.x, y};
      ViewRect r{new_pos, size}; // override_x, y, size.w, size.h};
      layout_.push_back(MenuEntry{
          .item = menu_item,
          .pos = new_pos,
          .rect = r,
          .selected = is_selected,
      });
      views::ui::InteractionLayers::TooltipCb ttcb;
      if (context_.config->options().qol_improvements) {
        if (std::string tt = menu_item->tooltip_text(); !tt.empty()) {
          ttcb = [tt = std::move(tt)](const views::ui::InteractionEvent& ie) {
            return TooltipRequest{
                .text = tt,
                .anchor = TooltipAnchor::Mouse,
                .screenPos = ie.pos,
                .palette = 8,
                .bg_color = 29,
                .text_color = 240,
                .border_color = 240,
                .padding = 4,
            };
          };
        }
      }
      interactions_.create_persistent_item(
          r, "menu_item_" + std::to_string(i),
          [this, menu_item](views::ui::InteractionEvent ie) {
            // convert to the menu-item's local coordinates
            views::ui::InteractionEvent local = ie;
            local.pos = ie.local;
            if (menu_item->on_interaction(local)) {
              mark_dirty();
              return;
            }
            if (ie.type == views::ui::InteractionEvent::Type::ButtonDown &&
                ie.button == views::ui::MouseButton::Left) {

              selected_option_ = menu_item;
              if (menu_item->handles_keyboard_events()) {
                keyboard_receiving_item_ = menu_item;
              }
              else {
                handle_menu_selection(menu_item);
              }
            }
            else if (ie.type == views::ui::InteractionEvent::Type::Enter) {
              selected_option_ = menu_item;
              mark_dirty();
            }
            else if (ie.type == views::ui::InteractionEvent::Type::Leave) {
              mark_dirty();
              selected_option_ = nullptr;
            }
          },
          std::move(ttcb));
      y += size.h + line_spacing_;
      ++i;
    }
  }
}

void Menu::tick() {
  // Decide which item (if any) should receive keyboard this frame.
  MenuItem* wants = nullptr;

  // Prefer the currently selected item if it wants keyboard.
  if (selected_option_ && selected_option_->handles_keyboard_events()) {
    wants = selected_option_;
  }
  else {
    // Fallback: first item that requests keyboard
    for (auto& it : menu_items_) {
      if (it->handles_keyboard_events()) {
        wants = it.get();
        break;
      }
    }
  }

  // Transition: if the target changed, close previous (cancel) and open new.
  if (wants && wants != keyboard_receiving_item_) {
    if (keyboard_receiving_item_) {
      Logger::warn("Already have a keyboard item");
    }
    keyboard_receiving_item_ = wants;
    if (keyboard_receiving_item_) {
      keyboard_receiving_item_->begin_keyboard_session(
          context_.input, [this] { mark_dirty(); });
    }
  }

  // If the active item stopped wanting keyboard, close it.
  if (keyboard_receiving_item_ &&
      !keyboard_receiving_item_->handles_keyboard_events()) {
    keyboard_receiving_item_ = nullptr;
  }
}

void Menu::render(const RenderContext& ctx) {
  update_interactions();
  auto bg = background();
  ctx.surface->fill(15);
  gfx::blit::blit_with_palette(*ctx.surface, bg);
  // ctx.surface->palette({63});

  for (auto& e : layout_) {
    e.item->draw(*ctx.surface, e.pos, e.item == selected_option_);
  }
}

void Menu::add_menu_item(std::unique_ptr<MenuItem> item) {
  menu_items_.push_back(std::move(item));
  ui_layout_dirty_ = true;
}

void Menu::handle_menu_selection(MenuItem* selected_item) {
  if (!selected_item) {
    Logger::debug("No selected item!");
    return;
  }
  selected_item->handle_action();
}

Surface Menu::default_background(const std::string& title) const {
  auto bg_color_bob = menu_bobs.frame(1, PaletteHandle{38});
  uint8_t bg_index = bg_color_bob.get(ViewPoint{1, 1});

  auto background =
      Surface(SCREEN_WIDTH, SCREEN_HEIGHT, bg_color_bob.palette(), bg_index);
  background.blit_masked(bg_color_bob, ViewPoint{44, 0});

  constexpr int line_y = 110;
  const uint8_t gray_index = 198;

  struct LineSegment {
    int from_x;
    int to_x;
  };

  for (LineSegment seg : {LineSegment{.from_x = 20, .to_x = 130},
                          LineSegment{.from_x = 140, .to_x = 145},
                          LineSegment{.from_x = 158, .to_x = 620}}) {
    gfx::draw::line(background, ViewPoint{seg.from_x, line_y},
                    ViewPoint{seg.to_x, line_y}, gray_index);
  }
  context_.font->render_text_onto(background, title, ViewPoint{280, 76},
                                  FontFace::MENU, BLACK);
  return background;
}

void Menu::update_selection(MenuItem* item) {
  if (item != selected_option_) {
    selected_option_ = item;
  }
  mark_dirty();
}
