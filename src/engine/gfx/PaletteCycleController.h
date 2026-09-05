/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

struct PaletteCycleController {
  enum class State {
    STOPPED,
    PENDING,
    RUNNING,
    STOPPING,
    DISABLED,
  };

  State state = State::STOPPED;
  int frame_index = 0;
  int tick_counter = 0;
  int ticks_per_frame = 10;
  int frame_count = 4;
  bool needs_palette_update = false;

  // Utility methods for state management
  void request_start() {
    if (state == State::STOPPED) {
      state = State::PENDING;
    }
  }

  void start_if_pending() {
    if (state == State::PENDING) {
      state = State::RUNNING;
      frame_index = 0;
      tick_counter = 0;
      needs_palette_update = true;
    }
  }

  void request_stop() {
    if (state == State::RUNNING) {
      state = State::STOPPING;
    }
  }

  void update() {
    if (state == State::STOPPED || state == State::DISABLED) {
      return;
    }

    start_if_pending();
    tick_counter++;
    if (tick_counter >= ticks_per_frame) {
      tick_counter = 0;
      frame_index = (frame_index + 1) % frame_count;
      needs_palette_update = true;

      // Check if we should stop
      if (state == State::STOPPING) {
        state = State::STOPPED;
      }
    }
  }

  bool is_active() const {
    return state == State::RUNNING || state == State::PENDING;
  }

  bool needs_update() const {
    return state == State::RUNNING && tick_counter >= ticks_per_frame;
  }

  void clear_palette_update_flag() { needs_palette_update = false; }

  bool needs_palette_update_check() const { return needs_palette_update; }
};
