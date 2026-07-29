/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/SDL_include.h"

#include <cstdint>

class FpsManager {
public:
  explicit FpsManager(int target_fps) : last_fps_sample_ns_(SDL_GetTicksNS()) {
    set_target_fps(target_fps);
  }

  // Sleep until the next frame deadline, then update the FPS counter.
  inline void end_frame_and_sleep() {
    uint64_t now_ns = SDL_GetTicksNS();
    next_frame_ns_ += frame_duration_ns_;

    if (now_ns < next_frame_ns_) {
      SDL_DelayNS(next_frame_ns_ - now_ns);
      now_ns = SDL_GetTicksNS();
    }
    else if (now_ns - next_frame_ns_ >
             frame_duration_ns_ * max_catchup_frames) {
      // Too far behind to make up by running flat out, so start again from now.
      next_frame_ns_ = now_ns;
    }

    ++frame_count_;
    if (now_ns - last_fps_sample_ns_ >= SDL_NS_PER_SECOND) {
      current_fps_ = frame_count_;
      frame_count_ = 0;
      last_fps_sample_ns_ += SDL_NS_PER_SECOND;
      if (now_ns - last_fps_sample_ns_ >= SDL_NS_PER_SECOND) {
        last_fps_sample_ns_ = now_ns;
      }
    }
  }

  inline uint32_t current_fps() const { return current_fps_; }

  // Start the new interval now, so changing the setting takes effect this
  // frame.
  inline void set_target_fps(int fps) {
    frame_duration_ns_ = SDL_NS_PER_SECOND / static_cast<uint64_t>(fps);
    next_frame_ns_ = SDL_GetTicksNS();
  }

private:
  // How far behind the deadline may fall before it is abandoned rather than
  // chased; a long stall should not be followed by uncapped catch-up frames.
  static constexpr uint64_t max_catchup_frames = 4;

  uint64_t frame_duration_ns_ = 0;
  // When the next frame is due. It's absolute so waking up late just makes
  // the next sleep shorter instead of us slowly losing frames.
  uint64_t next_frame_ns_ = 0;
  uint64_t last_fps_sample_ns_;
  uint32_t frame_count_ = 0;
  uint32_t current_fps_ = 0;
};
