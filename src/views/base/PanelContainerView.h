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
    // Draw the wooden border background; it has transparent areas so make sure
    // we have a black surface to work on:
    ctx.surface->fill(240);
    ctx.surface->blit_masked(context_.vga->get("gfx/bo"), {0, 0});
  }
  void tick() override {}
};
