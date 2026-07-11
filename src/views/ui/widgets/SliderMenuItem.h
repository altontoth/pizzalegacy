/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <optional>
#include <string>

#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionEvent.h"
#include "views/ui/widgets/MenuItem.h"

// A menu row with a label on the left and a draggable horizontal slider in the
// value column. The value is between 0 and 1.
class SliderMenuItem : public MenuItem {
public:
  SliderMenuItem(GameContext& context, const std::string& text, float& value,
                 std::function<void(float)> on_change,
                 FontFace font_face = FontFace::PIZ_NUMB);

  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  Size measure() const override;

  bool handles_mouse_events() override { return true; }
  bool on_interaction(views::ui::InteractionEvent ie) override;
  std::optional<views::ui::MouseButton> drag_button() const override {
    return views::ui::MouseButton::Left;
  }

private:
  static bool point_on_track(int local_x);
  // Set value_ from a row-local pointer x, clamped to 0..1.
  void seek_to(int local_x);

  float& value_;
  std::function<void(float)> on_change_;
  bool dragging_ = false;
  // A press landed on the track and may still turn into a drag.
  bool armed_ = false;
};
