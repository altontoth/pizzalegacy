/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/layers/GameInfoLayer.h"

#include <string>
#include <string_view>

#include "core/state/GameState.h"
#include "core/state/TurnOrchestrator.h"
#include "engine/FormatCompat.h"
#include "engine/Geometry.h"
#include "views/ui/ScreenText.h"

namespace {
constexpr int normal_time_scale = 1;
} // namespace

GameInfoLayer::GameInfoLayer(GameContext& context, GameState& state)
    : View(context, state) {}

void GameInfoLayer::render(const RenderContext& ctx) {
  if (!activity_.empty()) {
    views::ui::draw_screen_text(*context_.font, *ctx.surface, activity_,
                                HAlign::Left, VAlign::Bottom);
  }
  if (!time_scale_.empty()) {
    views::ui::draw_screen_text(*context_.font, *ctx.surface, time_scale_,
                                HAlign::Left, VAlign::Top);
  }
}

void GameInfoLayer::tick() {
  std::string_view activity = state_.turn().is_pipeline_blocking_input()
                                  ? state_.turn().pipeline_activity()
                                  : std::string_view{};
  const int scale = state_.time_scale();
  std::string time_scale =
      scale == normal_time_scale ? std::string{} : compat::format("{}x", scale);

  if (activity != activity_ || time_scale != time_scale_) {
    activity_ = activity;
    time_scale_ = std::move(time_scale);
    mark_dirty();
    request_recompose();
  }
}
