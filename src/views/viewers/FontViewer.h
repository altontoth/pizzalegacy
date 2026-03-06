/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <vector>
#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/InputManager.h"
#include "engine/input/ListenerToken.h"
#include "resources/FontTypes.h"
#include "views/base/View.h"

class FontViewer final : public View,
                         public input::InputListener,
                         public debug::AutoLabel<FontViewer> {
public:
  FontViewer(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  std::vector<FontFace> fonts_;
  int current_position_;
  input::ListenerToken listener_;
};
