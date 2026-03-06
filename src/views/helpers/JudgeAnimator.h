/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include "core/food/Pizza.h"
#include "core/util/GameContext.h"
#include "engine/RenderContext.h"

class JudgeAnimator {
public:
  explicit JudgeAnimator(GameContext& context);

  // Initialize animation with pizza scores
  void start(const std::array<int, NUMBER_OF_JUDGES>& scores,
             bool is_canonical_comparison = false);
  bool tick();
  void render(const RenderContext& ctx);
  bool is_complete() const { return !animating_; }
  void skip_to_end();

private:
  GameContext& context_;
  std::array<int, NUMBER_OF_JUDGES> scores_;
  std::array<int, NUMBER_OF_JUDGES> box_current_y_positions_;
  std::array<int, NUMBER_OF_JUDGES> box_final_y_positions_;
  std::array<int, NUMBER_OF_JUDGES> box_heights_;
  std::array<int, NUMBER_OF_JUDGES> ticks_to_wait_after_finishing_;
  int current_box_index_{0};
  bool animating_{false};
  bool is_canonical_comparison_{false};

  static constexpr int table_offset_ = 214;
  static constexpr int score_offset_ = 5;
};
