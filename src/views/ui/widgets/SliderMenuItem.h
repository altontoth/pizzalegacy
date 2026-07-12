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
// value column. Subclasses can map the track position (float between 0 and 1)
// to their value type.
class SliderMenuItemBase : public MenuItem {
public:
  SliderMenuItemBase(GameContext& context, const std::string& text,
                     FontFace font_face);

  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  Size measure() const override;

  bool handles_mouse_events() override { return true; }
  bool on_adjust(AdjustAxis axis, int delta) override;
  bool adjust_repeats() const override { return true; }
  bool on_interaction(views::ui::InteractionEvent ie) override;
  std::optional<views::ui::MouseButton> drag_button() const override {
    return views::ui::MouseButton::Left;
  }

protected:
  // Current value as a 0..1 track position.
  virtual float fraction() const = 0;
  virtual void set_fraction(float frac) = 0;
  // Keyboard adjust; delta is -1 or +1.
  virtual void step(int delta) = 0;
  // Text shown to the right of the track.
  virtual std::string value_text() const = 0;
  // Widest possible value text, so we can keep the the row width stable.
  virtual std::string widest_value_text() const = 0;

private:
  static bool point_on_track(int local_x);
  // Set the value from a row-local pointer x, clamped to 0..1.
  void seek_to(int local_x);

  bool dragging_ = false;
  // A press landed on the track and may still turn into a drag.
  bool armed_ = false;
};

// Continuous slider for a float (between 0 and 1), shown as a percentage.
class SliderMenuItem : public SliderMenuItemBase {
public:
  SliderMenuItem(GameContext& context, const std::string& text, float& value,
                 std::function<void(float)> on_change,
                 FontFace font_face = FontFace::PIZ_NUMB);

protected:
  float fraction() const override;
  void set_fraction(float frac) override;
  void step(int delta) override;
  std::string value_text() const override;
  std::string widest_value_text() const override;

private:
  float& value_;
  std::function<void(float)> on_change_;
};
