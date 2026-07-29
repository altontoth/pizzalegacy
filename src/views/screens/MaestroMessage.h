/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <string>

#include "core/state/UIState.h"
#include "core/util/GameContext.h"
#include "views/base/View.h"

// Full-screen message from El Maestro
class MaestroMessage : public View {
public:
  MaestroMessage(GameContext& context, GameState& state,
                 MaestroMessageState ms);
  void tick() override {}
  void render(const RenderContext& ctx) override;

private:
  std::string text_;
  std::function<void()> on_dismiss_;
};
