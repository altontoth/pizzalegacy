/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/base/Menu.h"

#include "core/state/GameState.h"
#include "engine/DisplayConstants.h"
#include "engine/Geometry.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/TooltipRequest.h"
#include "gfx/Blit.h"
#include "gfx/Draw.h"
#include "gfx/Surface.h"
#include "resources/FontManager.h"
#include "views/ui/PopupManager.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"

using DisplayConstants::SCREEN_HEIGHT;
using DisplayConstants::SCREEN_WIDTH;

namespace {
constexpr int ADJUST_REPEAT_DELAY_TICKS = 12;
constexpr int ADJUST_REPEAT_INTERVAL_TICKS = 3;
} // namespace

Menu::Menu(GameContext& context, GameState& state, ViewPoint origin,
           int line_spacing)
    : View(context, state), menu_bobs(context_.bob->get("bob/menubobs.bob")),
      origin_in_view_(origin), line_spacing_(line_spacing),
      listener_(context.input->register_listener(*this)) {

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
      ViewPoint origin =
          align_in_rect(ViewRect{{x, y}, {SCREEN_WIDTH, SCREEN_HEIGHT}}, size,
                        menu_item->alignment(), VAlign::Top);
      ViewPoint new_pos{origin.x, y};
      ViewRect r{new_pos, size}; // override_x, y, size.w, size.h};
      layout_.push_back(MenuEntry{
          .item = menu_item,
          .pos = new_pos,
          .rect = r,
          .selected = is_selected,
      });
      if (!menu_item->interactive()) {
        y += size.h + line_spacing_;
        ++i;
        continue;
      }
      views::ui::TooltipCb ttcb;
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
            // Hover fires every frame while the pointer rests on an item;
            // forward it only when the pointer actually moved, so it can't
            // override keyboard navigation.
            if (ie.type == views::ui::InteractionEvent::Type::Hover) {
              if (last_hover_pos_.has_value() && *last_hover_pos_ == ie.pos) {
                return;
              }
              last_hover_pos_ = ie.pos;
            }
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
            }
          },
          {.tooltip = std::move(ttcb),
           .drag_button = menu_item->drag_button()});
      y += size.h + line_spacing_;
      ++i;
    }
  }
}

void Menu::tick_adjust_repeat() {
  int h_dir = 0;
  if (context_.input->is_pressed(input::Action::MoveLeft)) {
    h_dir -= 1;
  }
  if (context_.input->is_pressed(input::Action::MoveRight)) {
    h_dir += 1;
  }
  int v_dir = 0;
  if (context_.input->is_pressed(input::Action::MoveUp)) {
    v_dir -= 1;
  }
  if (context_.input->is_pressed(input::Action::MoveDown)) {
    v_dir += 1;
  }
  if ((h_dir == 0 && v_dir == 0) || keyboard_receiving_item_ != nullptr ||
      context_.popup->freezes_view() || !selected_option_ ||
      !selected_option_->adjust_repeats()) {
    adjust_hold_ticks_ = 0;
    return;
  }
  // The initial press is handled by on_action; this only handles repeats. A
  // repeat that hits the end of the item's range does not escape to focus
  // movement; that requires a fresh key press so it has to be conscious action
  // by the user.
  ++adjust_hold_ticks_;
  if (adjust_hold_ticks_ >= ADJUST_REPEAT_DELAY_TICKS &&
      (adjust_hold_ticks_ - ADJUST_REPEAT_DELAY_TICKS) %
              ADJUST_REPEAT_INTERVAL_TICKS ==
          0) {
    bool changed = false;
    if (h_dir != 0) {
      changed |= selected_option_->on_adjust(AdjustAxis::Horizontal, h_dir);
    }
    if (v_dir != 0) {
      changed |= selected_option_->on_adjust(AdjustAxis::Vertical, v_dir);
    }
    if (changed) {
      mark_dirty();
    }
  }
}

void Menu::tick() {
  tick_adjust_repeat();

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
      // Repaint so the item can show its text-entry state right away.
      mark_dirty();
    }
  }

  // If the active item stopped wanting keyboard, close it.
  if (keyboard_receiving_item_ &&
      !keyboard_receiving_item_->handles_keyboard_events()) {
    keyboard_receiving_item_ = nullptr;
  }
}

void Menu::move_focus(int dir) {
  if (menu_items_.empty()) {
    return;
  }
  const auto n = static_cast<int>(menu_items_.size());
  // Start from the focused item, or just off either end when nothing is
  // focused yet so the first step lands on the first/last focusable item.
  int i = static_cast<int>(index_of_selected().value_or(
      dir > 0 ? static_cast<std::size_t>(n) - 1 : 0));

  for (int step = 1; step <= n; ++step) {
    i += dir;
    // Wrap around either direction
    if (i < 0) {
      i = n - 1;
    }
    else if (i >= n) {
      i = 0;
    }
    if (menu_items_[i]->focusable()) {
      update_selection(menu_items_[i].get());
      return;
    }
  }
}

bool Menu::on_action(const input::ActionEvent& ev) {
  if (keyboard_receiving_item_ != nullptr || context_.popup->freezes_view()) {
    return false;
  }
  if (ev.state.pressed) {
    switch (ev.action) {
    // Up/Down goes to the focused item first (a vertical list moves its
    // cursor); only an unconsumed step moves menu focus.
    case input::Action::MoveUp:
    case input::Action::MoveDown: {
      const int delta = ev.action == input::Action::MoveDown ? +1 : -1;
      if (selected_option_ &&
          selected_option_->on_adjust(AdjustAxis::Vertical, delta)) {
        mark_dirty();
        return true;
      }
      move_focus(delta);
      return true;
    }
    case input::Action::MoveLeft:
    case input::Action::MoveRight:
      if (selected_option_ &&
          selected_option_->on_adjust(
              AdjustAxis::Horizontal,
              ev.action == input::Action::MoveRight ? +1 : -1)) {
        mark_dirty();
        return true;
      }
      return false;
    case input::Action::NextMenuItem:
      move_focus(+1);
      return true;
    case input::Action::PrevMenuItem:
      move_focus(-1);
      return true;
    default:
      return false;
    }
  }
  // Fire on release, so that when the action opens a text session (e.g. a
  // rename), the release of the triggering key does not reach the session.
  if (ev.state.released &&
      (ev.action == input::Action::Fire ||
       ev.action == input::Action::SubmitInput) &&
      selected_option_) {
    handle_menu_selection(selected_option_);
    return true;
  }
  return false;
}

void Menu::render(const RenderContext& ctx) {
  update_interactions();
  auto bg = background();
  ctx.surface->fill(15);
  gfx::blit::blit_with_palette(*ctx.surface, bg);

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
  constexpr int text_color = 1;
  context_.font->render_text_onto(background, title, ViewPoint{280, 76},
                                  FontFace::MENU, text_color);
  return background;
}

void Menu::update_selection(MenuItem* item) {
  if (item != selected_option_) {
    selected_option_ = item;
  }
  mark_dirty();
}
