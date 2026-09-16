/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>

#include "engine/ui/InteractionEvent.h"

// Pointer handling for a handle that moves along a one-dimensional track.
// Clicking the track moves the handle there and may start a drag; clicking
// the handle itself starts dragging from its current position. Positions are
// along the track's axis in the caller's coordinates.
class TrackDrag {
public:
  struct Track {
    int start = 0;
    int length = 0;
    // Position and length of the handle along the track. A zero-length handle
    // is a point centered on its value.
    int handle_pos = 0;
    int handle_length = 0;
    // How far past either end of the track the pointer still counts.
    int padding = 0;

    bool contains(int pointer) const {
      return pointer >= start - padding && pointer <= start + length + padding;
    }
  };

  struct Result {
    bool handled = false;
    // New handle position as a fraction from 0 to 1 of the track's travel.
    std::optional<float> fraction;
  };

  Result on_interaction(const views::ui::InteractionEvent& ie, int pointer,
                        const Track& track);

  bool dragging() const { return dragging_; }
  // Forget a click that turned out not to be for this track.
  void reset() {
    armed_ = false;
    dragging_ = false;
  }

private:
  float fraction_at(int pointer, const Track& track) const;

  // A click landed on the track and could still turn into a drag.
  bool armed_ = false;
  bool dragging_ = false;
  // Pointer offset from the handle's start when it was grabbed.
  int grab_offset_ = 0;
};
