/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "CoreTypes.h"
#include "core/marketing/Commercial.h"
#include "resources/FontManager.h"
#include "resources/VgaManager.h"

#define COMMERCIAL_WIDTH 148
#define COMMERCIAL_HEIGHT 100

class TVPlaybackEngine {
  enum class PlaybackState {
    INACTIVE,
    PRE_TITLE_STATIC,
    TITLE_PAGE,
    INTRO, // the "adverts" thing
    AD,
  };

public:
  TVPlaybackEngine(FontManager* font, VgaManager* vga);

  bool tick();
  void start_playback();
  void play_commercial(int id);
  void next_commercial();
  void previous_commercial();
  // This just returns a pointer to the current_frame_,
  // so it's only valid until the next tick() call.
  Surface& next_frame();
  void reset() {
    playback_state_ = PlaybackState::INACTIVE;
    state_timer_ = 0;
    paused_ = false;
  }
  void init() {
    playback_state_ = PlaybackState::PRE_TITLE_STATIC;
    state_timer_ = 0;
    paused_ = false;
  }
  void pause() { paused_ = true; }
  void resume() { paused_ = false; }
  bool is_paused() const { return paused_; }

private:
  void draw_title_card(gfx::Surface& dst) const;
  void transition_to(PlaybackState state);
  void switch_commercial(int commercial_id);

  FontManager* font_;
  VgaManager* vga_;
  // Everything is represented as a commercial (except title screen),
  // so we have three "commercials"
  Commercial switching_static_;
  Commercial intro_;
  Commercial ad_;

  const gfx::Surface& tvmisc_sheet_;
  optional_ref<const gfx::Surface> commercial_sheet_; // lazily loaded

  // working buffers
  gfx::Surface current_frame_;
  int state_timer_{0};
  PlaybackState playback_state_{PlaybackState::INACTIVE};
  bool paused_{false};
};
