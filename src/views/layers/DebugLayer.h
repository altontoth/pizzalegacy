/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/OverlayBehavior.h"
#include "views/base/View.h"

class DebugLayer : public View,
                   public OverlayBehavior<DebugLayer>,
                   public input::InputListener,
                   public debug::AutoLabel<DebugLayer> {
public:
  DebugLayer(GameContext& context, GameState& state);
  void render(const RenderContext& ctx) override;
  void tick() override;
  bool is_dirty() const override { return View::is_dirty() || changed(); }
  bool needs_force_recompose() const override {
    return pull_recompose_flag() || changed();
  }
  bool on_text(const input::TextEvent& ev) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  bool changed() const;

  uint64_t rendered_revision_{0};
  input::ListenerToken listener_;
};
