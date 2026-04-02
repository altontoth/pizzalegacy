/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "views/ui/popups/BasePopup.h"

class Restaurant;

class TvPopup : public BasePopup {
public:
  TvPopup(const GameContext& ctx, Restaurant* restaurant);
  void add_interactions(views::ui::InteractionContext ictx,
                        ViewPoint origin) override;
  void render(Surface& surface, ViewPoint origin) override;

private:
  static ViewRect row_rect(int i);

  Restaurant* restaurant_;
  int selected_channel_; // 0 = OFF, 1-6 = targets demographic
  int hovered_row_{-1};
};
