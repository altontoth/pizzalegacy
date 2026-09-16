/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/View.h"
#include "views/ui/widgets/MenuItem.h"
#include "views/ui/widgets/ScrollBar.h"

class Menu : public View,
             public input::InputListener,
             public debug::AutoLabel<Menu> {
public:
  Menu(GameContext& context, GameState& state, ViewPoint origin,
       int line_spacing);
  void tick() override;
  void render(const RenderContext& ctx) override;
  ClockRate clock_rate() const override { return ClockRate::Stopped; }
  bool on_action(const input::ActionEvent& ev) override;

protected:
  void update_interactions();
  void handle_menu_selection(MenuItem* item);
  void update_selection(MenuItem* item);
  MenuItem& add_menu_item(std::unique_ptr<MenuItem> item);
  // The items added so far scroll, a whole item at a time, once they are
  // taller than max_height; the items added after stay below them.
  void end_scrolling_items(int max_height);
  virtual Surface background() const = 0;
  Surface default_background(const std::string& title) const;
  Bob& menu_bobs;
  MenuItem* selected_option_ = nullptr;

private:
  struct MenuEntry {
    MenuItem* item;
    ViewPoint pos;
    ViewRect rect;
    bool selected;
  };

  [[nodiscard]] static ViewPoint local_to_view(ViewPoint p,
                                               ViewPoint origin) noexcept {
    return ViewPoint{origin.x + p.x, origin.y + p.y};
  }
  [[nodiscard]] static ViewRect local_to_view(ViewRect r,
                                              ViewPoint origin) noexcept {
    return ViewRect{origin.x + r.x, origin.y + r.y, r.w, r.h};
  }
  [[nodiscard]] static ViewPoint view_to_local(ViewPoint p,
                                               ViewPoint origin) noexcept {
    return ViewPoint{p.x - origin.x, p.y - origin.y};
  }
  [[nodiscard]] std::optional<std::size_t> index_of_selected() const;
  void render_help_text(Surface& surface);
  // Move keyboard focus to the next focusable item in insertion order
  // (dir is -1 or +1), wrapping at both ends.
  void move_focus(int dir);
  // Step the focused item, if it takes the step. A repeat reaches only an
  // item that opted into repeating and that the hold started on.
  bool step_focused_item(AdjustAxis axis, int delta, bool repeat);

  // Scrolling, see end_scrolling_items().
  std::size_t scrolling_item_count() const;
  // One past the last scrolling item that fits when the view starts at first.
  std::size_t fit_end(std::size_t first) const;
  bool scrolls() const;
  // The furthest down the first visible item can be.
  std::size_t max_scroll() const;
  ScrollBar::Extent scroll_extent() const;
  void scroll_to(int first);
  void scroll_by(int wheel_delta);
  void keep_visible(const MenuItem* item);

  std::vector<std::unique_ptr<MenuItem>> menu_items_;
  ViewPoint origin_in_view_;
  int line_spacing_;
  MenuItem* keyboard_receiving_item_ = nullptr;
  std::optional<ViewRect> currently_hovered_rect_;
  std::vector<MenuEntry> layout_;
  bool ui_layout_dirty_{false};
  input::ListenerToken listener_;
  // Whether the arrow key being held started out adjusting the focused item.
  bool hold_adjusting_ = false;
  // See the Hover handling in update_interactions().
  std::optional<ViewPoint> last_hover_pos_;

  std::optional<std::size_t> pinned_from_;
  int scroll_height_ = 0;
  std::size_t scroll_first_ = 0;
  ScrollBar scroll_bar_;
  ViewRect scroll_track_;
};
