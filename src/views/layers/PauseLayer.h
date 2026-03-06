/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/InputManager.h"
#include "engine/input/ListenerToken.h"
#include "views/base/OverlayBehavior.h"
#include "views/base/View.h"

class PauseLayer : public View,
                   public OverlayBehavior<PauseLayer>,
                   public input::InputListener,
                   public debug::AutoLabel<PauseLayer> {
public:
  PauseLayer(GameContext& context, GameState& state);
  void render(const RenderContext& ctx) override;
  void tick() override;
  bool is_paused() const { return is_paused_; }
  bool blocks() const override { return is_paused_; }
  bool on_action(const input::ActionEvent& ev) override;

private:
  bool is_paused_{false};
  input::ListenerToken listener_;
};
