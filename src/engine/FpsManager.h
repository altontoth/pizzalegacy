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
  explicit FpsManager(int target_fps)
      : target_frame_duration_ms_(static_cast<uint32_t>(1000 / target_fps)) {}

  inline void begin_frame(uint64_t wall_ms) { frame_start_ms_ = wall_ms; }

  // Sleep if we finished early, then update FPS counters.
  inline void end_frame_and_sleep(uint64_t wall_ms) {
    uint64_t elapsed = wall_ms - frame_start_ms_;
    if (elapsed < target_frame_duration_ms_) {
      const uint32_t sleep_ms =
          static_cast<uint32_t>(target_frame_duration_ms_ - elapsed);
      SDL_Delay(sleep_ms);
      wall_ms += sleep_ms; // advance to the time after sleeping
      elapsed += sleep_ms;
    }

    // FPS accounting (1 second window)
    ++frame_count_;
    if (wall_ms - last_fps_sample_ms_ >= 1000) {
      current_fps_ = frame_count_;
      frame_count_ = 0;
      last_fps_sample_ms_ = wall_ms;
    }

    last_frame_elapsed_ms_ = static_cast<uint32_t>(elapsed);
  }

  inline uint32_t frame_elapsed_ms() const { return last_frame_elapsed_ms_; }
  inline uint32_t current_fps() const { return current_fps_; }

  inline void set_target_fps(int fps) {
    target_frame_duration_ms_ = static_cast<uint32_t>(1000 / fps);
  }

private:
  uint32_t target_frame_duration_ms_;
  uint64_t frame_start_ms_ = 0;
  uint64_t last_fps_sample_ms_ = 0;
  uint32_t last_frame_elapsed_ms_ = 0;
  uint32_t frame_count_ = 0;
  uint32_t current_fps_ = 0;
};
