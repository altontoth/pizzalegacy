/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "views/ui/popups/BasePopup.h"

namespace audio {
class AudioManager;
}
class Restaurant;

class JukeboxPopup : public BasePopup {
public:
  JukeboxPopup(const GameContext& ctx, Restaurant* restaurant);
  void add_interactions(views::ui::InteractionContext ictx,
                        ViewPoint origin) override;
  void render(Surface& surface, ViewPoint origin) override;

private:
  static ViewRect row_rect(int i);

  Restaurant* restaurant_;
  audio::AudioManager* audio_;
  int selected_music_; // 0 = OFF, 1-6 = music track index
  int hovered_row_{-1};
};
