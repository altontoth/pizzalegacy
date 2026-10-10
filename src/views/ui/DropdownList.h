/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionLayers.h"
#include "resources/FontManager.h"
#include "views/ui/ClickResult.h"

class DropdownList {
public:
  DropdownList(std::string_view name, ViewRect header, ViewPoint origin,
               std::optional<std::function<void()>> on_activate_cb);
  void render(Surface& target, FontManager* font);
  bool tick();
  bool handle_hover(ViewPoint position);
  ClickResult handle_click(ViewPoint position);
  void add_entry(std::string_view, std::function<void()>,
                 std::optional<uint8_t> swatch_color = std::nullopt);
  bool is_expanded() const;
  void add_interactions(views::ui::InteractionContext&,
                        std::function<void()> on_pick);
  ViewRect header() const { return header_rect_; }
  void expanded(bool);

private:
  struct DropdownEntry {
    std::string label;
    std::function<void()> on_click;
    // Drawn as a small square before the label
    std::optional<uint8_t> swatch_color;
  };
  std::optional<int> entry_index_at(ViewPoint p) const;

  std::string name_;
  ViewRect header_rect_;
  bool expanded_ = false;
  std::vector<DropdownEntry> entries_;
  std::optional<int> hovered_index_;
  static constexpr int entry_height_ = 12;
  Surface header_bar_;
  static constexpr int bar_height_ = 20;
  static constexpr int bar_width_ = 192;
  // How much padding before the first line
  static constexpr int line_padding_ = 4;
  // How much of the area where we render text is visible:
  int current_visible_height_ = 0;
  // Full height is just the text entries again
  int full_height_ = 0;
  bool animating_open_ = false;
  static constexpr int dropdown_anim_speed_ = 40;
  // Where to start drawing the dropdown
  ViewPoint origin_;
  std::optional<std::function<void()>> on_activate_cb_;
};
