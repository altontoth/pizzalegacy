/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm> // std::clamp
#include <cstdint>

struct GameClock {
  uint64_t now_ms{0}; // monotonic game time in ms
  uint32_t dt_ms{0};  // last frame's delta in ms (clamped)
  uint64_t frame{0};  // frame index

  // Tunables
  uint32_t max_dt_ms{50}; // clamp for tab-outs / breakpoints
  uint32_t min_dt_ms{0};

  // Internal
  uint64_t _last_ms{0};
  bool _initialized{false};

  // Call once per frame before tick/render/handle_events.
  // Pass in a wall-clock timestamp in ms (e.g., SDL_GetTicks()).
  inline void begin_frame(uint64_t wall_ms) {
    if (!_initialized) {
      _last_ms = wall_ms;
      _initialized = true;
      dt_ms = 0;
    }
    else {
      const uint64_t raw = wall_ms - _last_ms;
      _last_ms = wall_ms;

      // clamp to [min_dt_ms, max_dt_ms]:
      dt_ms = static_cast<uint32_t>(
          std::clamp<uint64_t>(raw, min_dt_ms, max_dt_ms));
      now_ms += dt_ms;
      ++frame;
    }
  }

  inline bool elapsed_since(uint64_t start_ms, uint32_t duration_ms) const {
    return now_ms - start_ms >= duration_ms;
  }

  inline bool every_n_ms(uint32_t period_ms, uint64_t* last_phase) const {
    const uint64_t phase = period_ms ? (now_ms / period_ms) : 0;
    if (phase != *last_phase) {
      *last_phase = phase;
      return true;
    }
    return false;
  }
};
