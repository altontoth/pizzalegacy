/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/InputPanel.h"

#include "engine/geom/Spaces.h"
#include "views/helpers/ButtonGeometry.h"
#include "views/ui/widgets/Button.h"

InputPanel::InputPanel(GameContext& context, GameState& state, ViewPoint origin)
    : PanelView(context, state, origin) {}

void InputPanel::tick() {}

void InputPanel::render(const RenderContext& ctx) {
  // Fill with opaque background color to prevent old graphics from showing
  // through Use color 17 (yellow) for the input panel background
  ctx.surface->fill(17);

  if (buttons_changed_) {
    layout_buttons();
    buttons_changed_ = false;
  }

  for (auto& button : buttons_) {
    button->draw(*ctx.surface);
  }
}

void InputPanel::layout_buttons() {
  std::unordered_map<std::string, int> seen;
  for (auto& button : buttons_) {
    auto cb = [&button](views::ui::InteractionEvent ie) {
      views::ui::InteractionEvent local = ie;
      button->on_interaction(local);
    };
    const auto base = button->tooltip();
    int n = ++seen[base];
    auto ttcb = [tt_text = base](views::ui::InteractionEvent ie) {
      return TooltipRequest{
          .text = tt_text,
          .anchor = TooltipAnchor::InputPanel,
          .bg_color = gfx::TRANSPARENT,
          .text_color = 240, // black
          .padding = 0,
      };
    };

    // Make sure two "Turn page" buttons don't cause issues by deduplicating
    // the names:
    std::string dbg_name = (n == 1) ? base : base + " #" + std::to_string(n);
    interactions_.create_persistent_item(button->location(), dbg_name, cb,
                                         ttcb);
  }
}

void InputPanel::add_button(std::unique_ptr<Button> b) {
  buttons_.push_back(std::move(b));
  buttons_changed_ = true;
}

void InputPanel::add_button(Icon icon, ButtonType type, ButtonPosition pos,
                            std::string tooltip_str,
                            std::function<void()> on_click) {
  auto dirty_cb = [this] { mark_dirty(); };
  ViewRect rect_pos = ButtonGeometry::rect(type, pos);

  auto btn =
      std::make_unique<Button>(context_.vga, icon, type, rect_pos, tooltip_str,
                               std::move(on_click), std::move(dirty_cb));

  add_button(std::move(btn));
}

void InputPanel::delete_buttons() {
  buttons_.clear();
  interactions_.clear_persistent();
  mark_dirty();
}
