/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include "engine/ui/InteractionEvent.h"
#include "resources/VgaManager.h"
#include "views/helpers/ButtonGeometry.h"
#include "views/ui/Icon.h"

#define BUTTON_WIDTH 32
#define BUTTON_HEIGHT 26

class Button {
public:
  Button(VgaManager* vga, Icon icon, ButtonType type, ViewRect pos,
         std::string tooltip, std::function<void()> on_click,
         std::function<void()> mark_dirty);
  // std::function<void(const std::string&)> on_tooltip);
  void draw(Surface&, const ViewPoint& origin = {0, 0});
  ViewRect location() const { return pos_; };
  const std::string& tooltip() const;
  void on_interaction(views::ui::InteractionEvent ie);

private:
  static Surface make_button_surface(VgaManager* vga, Icon icon,
                                     ButtonType type, ViewRect pos,
                                     bool for_pressed);
  ViewRect pos_;
  bool button_pressed_{false};
  std::string tooltip_;
  std::function<void()> on_click_;
  std::function<void()> mark_dirty_;
  const Surface image_;
  const Surface pressed_image_;
};
