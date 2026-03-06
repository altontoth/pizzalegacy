/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <functional>
#include "core/food/Pizza.h"
#include "engine/DebugLabel.h"
#include "views/base/View.h"
#include "views/helpers/JudgeAnimator.h"

class PizzaJudges : public View, public debug::AutoLabel<PizzaJudges> {
public:
  PizzaJudges(GameContext& context, GameState& state,
              const std::array<int, NUMBER_OF_JUDGES>& scores,
              std::function<void()> on_complete,
              bool is_canonical_comparison = false);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  void register_interactions();

  JudgeAnimator animator_;
  std::function<void()> on_complete_;
};
