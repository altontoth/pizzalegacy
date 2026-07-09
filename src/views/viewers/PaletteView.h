/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>

#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/View.h"

class PaletteView : public View,
                    public input::InputListener,
                    public debug::AutoLabel<PaletteView> {
public:
  PaletteView(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  void render_rgb(const RenderContext& ctx);

  uint8_t palette_{0};
  bool show_numbers_{false};
  std::optional<int> hover_index_;
  std::optional<int> click_index_;
  input::ListenerToken listener_;
};
