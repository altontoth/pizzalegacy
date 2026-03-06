/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/DebugLabel.h"
#include "engine/geom/Spaces.h"
#include "views/base/View.h"

class PanelView : public View, public debug::AutoLabel<PanelView> {
public:
  PanelView(GameContext& context, GameState& state, ViewPoint origin);
  void tick() override = 0;
  void render(const RenderContext& ctx) override = 0;

protected:
  ViewPoint panel_origin_;
};
