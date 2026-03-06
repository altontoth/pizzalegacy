/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "engine/DebugLabel.h"
#include "views/base/View.h"
#include "views/ui/widgets/MenuItem.h"

class Menu : public View, public debug::AutoLabel<Menu> {
public:
  Menu(GameContext& context, GameState& state, ViewPoint origin,
       int line_spacing);
  void tick() override;
  void render(const RenderContext& ctx) override;

protected:
  void update_interactions();
  void handle_menu_selection(MenuItem* item);
  void update_selection(MenuItem* item);
  void add_menu_item(std::unique_ptr<MenuItem> item);
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

  std::vector<std::unique_ptr<MenuItem>> menu_items_;
  ViewPoint origin_in_view_;
  int line_spacing_;
  MenuItem* keyboard_receiving_item_ = nullptr;
  std::optional<ViewRect> currently_hovered_rect_;
  std::vector<MenuEntry> layout_;
  bool ui_layout_dirty_{false};
};
