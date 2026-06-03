/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/state/UIState.h"
#include "core/util/GameContext.h"
#include "views/base/View.h"

class CookbookReminder : public View {
public:
  CookbookReminder(GameContext& context, GameState& state,
                   CookbookReminderState crs);
  void tick() override {}
  void render(const RenderContext& ctx) override;

private:
  int page_;
};
