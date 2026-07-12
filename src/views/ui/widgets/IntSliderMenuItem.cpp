/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/IntSliderMenuItem.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "engine/FormatCompat.h"

IntSliderMenuItem::IntSliderMenuItem(GameContext& context,
                                     const std::string& text, int& value,
                                     int min_value, int max_value,
                                     std::function<void(int)> on_change,
                                     FontFace font_face)
    : SliderMenuItemBase(context, text, font_face), value_(value),
      min_value_(min_value), max_value_(max_value),
      on_change_(std::move(on_change)) {}

float IntSliderMenuItem::fraction() const {
  const int v = std::clamp(value_, min_value_, max_value_);
  return static_cast<float>(v - min_value_) /
         static_cast<float>(max_value_ - min_value_);
}

void IntSliderMenuItem::set_fraction(float frac) {
  const float clamped = std::clamp(frac, 0.0f, 1.0f);
  set_value(min_value_ +
            static_cast<int>(std::lround(
                clamped * static_cast<float>(max_value_ - min_value_))));
}

void IntSliderMenuItem::step(int delta) {
  set_value(std::clamp(value_ + delta, min_value_, max_value_));
}

void IntSliderMenuItem::set_value(int v) {
  if (v == value_) {
    return;
  }
  value_ = v;
  if (on_change_) {
    on_change_(v);
  }
}

std::string IntSliderMenuItem::value_text() const {
  return compat::format("({})", value_);
}

std::string IntSliderMenuItem::widest_value_text() const {
  return compat::format("({})", max_value_);
}
