/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/geom/Spaces.h"
#include "views/ui/widgets/MenuItem.h"

class ToggleMenuItem : public MenuItem {
public:
  ToggleMenuItem(GameContext& context, const std::string& text, bool& value,
                 std::function<void()> action,
                 FontFace font_face = FontFace::PIZ_NUMB);

  ToggleMenuItem(GameContext& context, const std::string& text, bool& value,
                 const std::string& false_label, const std::string& true_label,
                 std::function<void()> action,
                 FontFace font_face = FontFace::PIZ_NUMB);

  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  Size measure() const override;

private:
  bool& value_;
  std::function<void()> action_;
  std::string false_label_;
  std::string true_label_;
};
