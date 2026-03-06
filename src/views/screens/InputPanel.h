/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <list>
#include <memory>
#include "engine/geom/Spaces.h"
#include "views/base/PanelView.h"
#include "views/ui/widgets/Button.h"

class MainPanelView;

class InputPanel : public PanelView {
public:
  InputPanel(GameContext& context, GameState& state, ViewPoint p);
  void tick() override;
  void render(const RenderContext& ctx) override;
  void add_button(Icon icon, ButtonType type, ButtonPosition pos,
                  std::string tooltip, std::function<void()> on_click);
  void add_button(std::unique_ptr<Button>);
  void delete_buttons();

private:
  void handle_mouse_hover(ViewPoint p);
  void handle_mouse_click(ViewPoint p);
  void handle_mouse_up(ViewPoint p);
  ViewRect button_rect(ButtonType, ButtonPosition);
  void layout_buttons();
  static ViewRect calculate_button_rect(ButtonType type, ButtonPosition pos);

  std::list<std::unique_ptr<Button>> buttons_;
  // We use this to keep track on when we have to create new interaction items.
  bool buttons_changed_{false};
};
