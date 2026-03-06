/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "engine/DebugLabel.h"
#include "gfx/Blit.h"
#include "views/base/View.h"

class PanelContainerView : public View,
                           public debug::AutoLabel<PanelContainerView> {
public:
  PanelContainerView(GameContext& context, GameState& state)
      : View(context, state) {}

  void render(const RenderContext& ctx) override {
    // Draw the wooden border background
    gfx::blit::blit_raw_with_palette(*ctx.surface, context_.vga->get("gfx/bo"));
  }
  void tick() override {}
};
