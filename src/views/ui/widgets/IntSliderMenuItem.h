/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <string>

#include "views/ui/widgets/SliderMenuItem.h"

// Slider bound to an int
class IntSliderMenuItem : public SliderMenuItemBase {
public:
  IntSliderMenuItem(GameContext& context, const std::string& text, int& value,
                    int min_value, int max_value,
                    std::function<void(int)> on_change,
                    FontFace font_face = FontFace::PIZ_NUMB, int step = 1);

protected:
  float fraction() const override;
  void set_fraction(float frac) override;
  void step(int delta) override;
  std::string value_text() const override;
  std::string widest_value_text() const override;

private:
  void set_value(int v);

  int& value_;
  int min_value_;
  int max_value_;
  // Values land on multiples of this, counted from min_value.
  int step_;
  std::function<void(int)> on_change_;
};
