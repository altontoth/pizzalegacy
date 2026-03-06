/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/helpers/TVPlaybackEngine.h"

#include "core/marketing/AdvertisingCampaign.h"
#include "core/util/Tools.h"
#include "factories/CommercialFactory.h"
#include "magic_enum/magic_enum.hpp"

/*
   How commercials work

   After clicking the buy button, we see static
   then a title and director.
   <press arrow>
   show static
   show new title/director
   etc
   <click buy>
   show intro
   show advertising (loop)
*/

TVPlaybackEngine::TVPlaybackEngine(FontManager* font, VgaManager* vga)
    : font_(font), vga_(vga),
      // We've store the static just behind the normal commercials (and the
      // intro behind that):
      switching_static_(CommercialFactory::create(NUMBER_OF_TV_ADS)),
      intro_(CommercialFactory::create(NUMBER_OF_TV_ADS + 1)),
      ad_(CommercialFactory::create(0)),
      tvmisc_sheet_(vga->get(switching_static_.filename())),
      current_frame_(COMMERCIAL_WIDTH, COMMERCIAL_HEIGHT,
                     gfx::PaletteHandle{tvmisc_sheet_.palette()}) {}

bool TVPlaybackEngine::tick() {
  if (playback_state_ == PlaybackState::INACTIVE) {
    return false;
  }

  // Skip animation advancement while paused (RMB held)
  if (paused_) {
    return false;
  }

  ++state_timer_;

  bool mark_dirty = false;
  switch (playback_state_) {
  case PlaybackState::PRE_TITLE_STATIC:
    if (state_timer_ % 3 == 0) {
      mark_dirty = true;
      switching_static_.next_frame();
    }
    if (state_timer_ > 25) {
      mark_dirty = true;
      transition_to(PlaybackState::TITLE_PAGE);
    }
    break;
  case PlaybackState::INTRO:
    if (state_timer_ % 3 == 0) {
      mark_dirty = true;
      intro_.next_frame();
    }
    if (state_timer_ > 14) {
      mark_dirty = true;
      transition_to(PlaybackState::AD);
    }
    break;
  case PlaybackState::AD:
    if (state_timer_ % 3 == 0) {
      mark_dirty = true;
      ad_.next_frame();
    }
    break;
  default:
    break;
  }
  return mark_dirty;
}

gfx::Surface& TVPlaybackEngine::next_frame() {
  constexpr uint8_t BACKGROUND_COLOR = 240; // black
  current_frame_.fill(BACKGROUND_COLOR);

  // pick source sheet + frame index
  const gfx::Surface* src = nullptr;
  int frame = 0;
  int frames_per_row = 4;

  switch (playback_state_) {
  case PlaybackState::PRE_TITLE_STATIC:
    src = &tvmisc_sheet_;
    frames_per_row = 3;
    frame = switching_static_.current_frame();
    break;
  case PlaybackState::INTRO:
    src = &tvmisc_sheet_;
    frames_per_row = 3;
    frame = intro_.current_frame();
    break;
  case PlaybackState::AD:
    if (!commercial_sheet_) {
      commercial_sheet_ = vga_->get(ad_.filename());
    }
    src = &commercial_sheet_->get();
    frame = ad_.current_frame();
    break;
  case PlaybackState::TITLE_PAGE:
    draw_title_card(current_frame_);
    return current_frame_;
  default:
    return current_frame_;
  }

  // compute source rect and blit (masked)
  ViewRect src_pos{
      (frame % frames_per_row) * COMMERCIAL_WIDTH,
      (frame / frames_per_row) * COMMERCIAL_HEIGHT,
      COMMERCIAL_WIDTH,
      COMMERCIAL_HEIGHT,
  };
  current_frame_.blit(*src, src_pos); // transparency aware

  return current_frame_;
}

void TVPlaybackEngine::next_commercial() {
  int id = ad_.id();
  id = (id + 1) % NUMBER_OF_TV_ADS;
  switch_commercial(id);
}

void TVPlaybackEngine::previous_commercial() {
  int id = ad_.id();
  id = (id - 1 + NUMBER_OF_TV_ADS) % NUMBER_OF_TV_ADS;
  switch_commercial(id);
}

void TVPlaybackEngine::switch_commercial(int id) {
  ad_ = CommercialFactory::create(id);
  commercial_sheet_ = vga_->get(ad_.filename());
  transition_to(PlaybackState::PRE_TITLE_STATIC);
}

void TVPlaybackEngine::draw_title_card(gfx::Surface& dst) const {
  dst.fill(240); // black

  uint8_t text_color = 255;
  ViewPoint text_pos{3, 4};

  font_->render_line_onto(dst, "TITLE:", text_pos, FontFace::SUPERSMA,
                          text_color);
  text_pos.y = 20;
  font_->render_line_onto(dst, Tools::to_uppercase(ad_.title()), text_pos,
                          FontFace::SUPERSMA, text_color);
  text_pos.y = 36;
  font_->render_line_onto(dst, "DIRECTOR:", text_pos, FontFace::SUPERSMA,
                          text_color);
  text_pos.y = 52;
  font_->render_line_onto(dst, Tools::to_uppercase(ad_.director()), text_pos,
                          FontFace::SUPERSMA, text_color);
}

void TVPlaybackEngine::transition_to(PlaybackState state) {
  playback_state_ = state;
  state_timer_ = 0;
}

void TVPlaybackEngine::start_playback() { transition_to(PlaybackState::INTRO); }
