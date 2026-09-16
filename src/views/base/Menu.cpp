/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/base/Menu.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include "core/state/GameState.h"
#include "engine/DisplayConstants.h"
#include "engine/Geometry.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/Blit.h"
#include "engine/gfx/Draw.h"
#include "engine/gfx/Surface.h"
#include "engine/ui/InteractionManager.h"
#include "engine/ui/TooltipRequest.h"
#include "resources/FontManager.h"
#include "views/ui/PopupManager.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"

using DisplayConstants::SCREEN_HEIGHT;
using DisplayConstants::SCREEN_WIDTH;

namespace {
constexpr uint8_t line_gray = 198;
constexpr int line_from_x = 20;
constexpr int line_to_x = 620;
// The line and the help text, measured down from the last item.
constexpr int help_line_gap = 14;
constexpr int help_text_gap = 30;
constexpr int help_text_width = 400;
// Where the thumb of a scrolling menu runs, at the right of its items.
constexpr int scroll_track_x = 606;
constexpr int scroll_track_w = 14;
} // namespace

Menu::Menu(GameContext& context, GameState& state, ViewPoint origin,
           int line_spacing)
    : View(context, state), menu_bobs(context_.bob->get("bob/menubobs.bob")),
      origin_in_view_(origin), line_spacing_(line_spacing),
      listener_(context.input->register_listener(*this)) {

  update_selection(nullptr);
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

    const bool scrolling = scrolls();
    const std::size_t visible_end = scrolling ? fit_end(scroll_first_) : 0;
    if (scrolling) {
      const ViewRect area{origin_in_view_.x, origin_in_view_.y,
                          scroll_track_x - origin_in_view_.x, scroll_height_};
      scroll_track_ = ViewRect{scroll_track_x, origin_in_view_.y,
                               scroll_track_w, scroll_height_};
      // Created first so the items on top of it take precedence.
      interactions_.create_persistent_item(
          area, "menu_scroll_area", [this](views::ui::InteractionEvent ie) {
            if (ie.type == views::ui::InteractionEvent::Type::Scroll) {
              scroll_by(ie.scroll_delta);
            }
          });
    }
    else {
      scroll_track_ = {};
    }

    for (std::size_t index = 0; index < menu_items_.size(); ++index) {
      auto* menu_item = menu_items_[index].get();
      if (scrolling && index < *pinned_from_ &&
          (index < scroll_first_ || index >= visible_end)) {
        continue;
      }
      if (scrolling && index == *pinned_from_) {
        y = origin_in_view_.y + scroll_height_;
      }
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
        continue;
      }
      views::ui::TooltipCb ttcb;
      if (state_.options().qol_improvements) {
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
          r, "menu_item_" + std::to_string(index),
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
            if (ie.type == views::ui::InteractionEvent::Type::Scroll &&
                scrolls()) {
              scroll_by(ie.scroll_delta);
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
            // A rebuilt item under a resting pointer also gets Enter.
            else if (ie.type == views::ui::InteractionEvent::Type::Enter &&
                     last_hover_pos_ != ie.pos) {
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
    }

    if (scrolling) {
      interactions_.create_persistent_item(
          scroll_track_, "menu_scroll_bar",
          [this](views::ui::InteractionEvent ie) {
            int first = static_cast<int>(scroll_first_);
            if (scroll_bar_.on_interaction(ie, ie.pos, scroll_track_,
                                           scroll_extent(), first)) {
              scroll_to(first);
              mark_dirty();
            }
          },
          {.drag_button = views::ui::MouseButton::Left});
    }
  }
}

void Menu::end_scrolling_items(int max_height) {
  pinned_from_ = menu_items_.size();
  scroll_height_ = max_height;
}

std::size_t Menu::scrolling_item_count() const {
  return pinned_from_.value_or(0);
}

std::size_t Menu::fit_end(std::size_t first) const {
  int height = 0;
  std::size_t end = first;
  for (; end < scrolling_item_count(); ++end) {
    const int item_height = menu_items_[end]->measure().h;
    if (height + item_height > scroll_height_ && end > first) {
      break;
    }
    height += item_height + line_spacing_;
  }
  return end;
}

bool Menu::scrolls() const {
  return pinned_from_ && fit_end(0) < scrolling_item_count();
}

std::size_t Menu::max_scroll() const {
  const std::size_t count = scrolling_item_count();
  std::size_t first = count - 1;
  while (first > 0 && fit_end(first - 1) == count) {
    --first;
  }
  return first;
}

ScrollBar::Extent Menu::scroll_extent() const {
  const auto count = static_cast<int>(scrolling_item_count());
  return {.offset = static_cast<int>(scroll_first_),
          .visible = count - static_cast<int>(max_scroll()),
          .total = count};
}

void Menu::scroll_to(int first) {
  const auto target = static_cast<std::size_t>(
      std::clamp(first, 0, static_cast<int>(max_scroll())));
  if (target != scroll_first_) {
    scroll_first_ = target;
    ui_layout_dirty_ = true;
    mark_dirty();
  }
}

void Menu::scroll_by(int wheel_delta) {
  scroll_to(static_cast<int>(scroll_first_) - (wheel_delta > 0 ? 1 : -1));
}

void Menu::keep_visible(const MenuItem* item) {
  if (!scrolls()) {
    return;
  }
  const std::size_t count = scrolling_item_count();
  for (std::size_t index = 0; index < count; ++index) {
    if (menu_items_[index].get() != item) {
      continue;
    }
    std::size_t first = std::min(scroll_first_, index);
    while (fit_end(first) <= index) {
      ++first;
    }
    scroll_to(static_cast<int>(first));
    return;
  }
}

bool Menu::step_focused_item(AdjustAxis axis, int delta, bool repeat) {
  if (!selected_option_) {
    return false;
  }
  if (repeat && (!hold_adjusting_ || !selected_option_->adjust_repeats())) {
    return false;
  }
  if (!selected_option_->on_adjust(axis, delta)) {
    return false;
  }
  mark_dirty();
  return true;
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
      const bool adjusted =
          step_focused_item(AdjustAxis::Vertical, delta, ev.state.repeat);
      if (!ev.state.repeat) {
        hold_adjusting_ = adjusted;
      }
      // A repeat stays with the item once it reaches the end of its range;
      // moving on to the next item takes a fresh press.
      if (adjusted || (ev.state.repeat && hold_adjusting_)) {
        return true;
      }
      move_focus(delta);
      return true;
    }
    case input::Action::MoveLeft:
    case input::Action::MoveRight: {
      const int delta = ev.action == input::Action::MoveRight ? +1 : -1;
      const bool adjusted =
          step_focused_item(AdjustAxis::Horizontal, delta, ev.state.repeat);
      if (!ev.state.repeat) {
        hold_adjusting_ = adjusted;
      }
      return adjusted;
    }
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
  if (scroll_track_.h > 0) {
    scroll_bar_.draw(*ctx.surface, scroll_track_, scroll_extent());
  }
  render_help_text(*ctx.surface);
}

void Menu::render_help_text(Surface& surface) {
  if (!state_.options().qol_improvements || layout_.empty()) {
    return;
  }
  const bool any_help = std::ranges::any_of(
      menu_items_, [](const auto& item) { return !item->help_text().empty(); });
  if (!any_help) {
    return;
  }

  // Below the scrolling items when there are any, else below the last item
  int bottom = origin_in_view_.y + scroll_height_;
  if (!pinned_from_) {
    const ViewRect& last = layout_.back().rect;
    bottom = last.y + last.h;
  }
  gfx::draw::line(surface, ViewPoint{line_from_x, bottom + help_line_gap},
                  ViewPoint{line_to_x, bottom + help_line_gap}, line_gray);

  if (!selected_option_) {
    return;
  }
  const std::string& help = selected_option_->help_text();
  if (help.empty()) {
    return;
  }
  context_.font->render_into_rect(
      surface, help,
      {origin_in_view_.x, bottom + help_text_gap, help_text_width, 0},
      FontFace::CSTANDAR, TextStyle{.color = 240});
}

MenuItem& Menu::add_menu_item(std::unique_ptr<MenuItem> item) {
  menu_items_.push_back(std::move(item));
  ui_layout_dirty_ = true;
  return *menu_items_.back();
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

  struct LineSegment {
    int from_x;
    int to_x;
  };

  for (LineSegment seg : {LineSegment{.from_x = line_from_x, .to_x = 130},
                          LineSegment{.from_x = 140, .to_x = 145},
                          LineSegment{.from_x = 158, .to_x = line_to_x}}) {
    gfx::draw::line(background, ViewPoint{seg.from_x, line_y},
                    ViewPoint{seg.to_x, line_y}, line_gray);
  }
  constexpr int text_color = 1;
  context_.font->render_text_onto(background, title, ViewPoint{280, 76},
                                  FontFace::MENU, text_color);
  return background;
}

void Menu::update_selection(MenuItem* item) {
  if (item != selected_option_) {
    selected_option_ = item;
    keep_visible(item);
  }
  mark_dirty();
}
