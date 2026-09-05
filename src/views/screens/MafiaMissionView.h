/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>

#include "core/conversations/MafiaLine.h"
#include "views/base/MainPanelView.h"

class MafiaMissionView : public MainPanelView {
public:
  MafiaMissionView(GameContext& context, GameState& state, MafiaLine line);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  MafiaLine line_;
  std::string text_;
};
