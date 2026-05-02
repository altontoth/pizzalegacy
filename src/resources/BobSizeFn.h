/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>

#include "engine/Size.h"
#include "engine/geom/Spaces.h"

using BobSizeFn = std::function<Size(int ingredient_id, int state)>;

// Build a size provider from the Zutaten bob:
template <class BobT, class ToppingT>
inline BobSizeFn make_ingredient_bob_size_fn(BobT& zutaten_bob) {
  return [&zutaten_bob](int ingredient_id, int state) -> Size {
    ToppingT probe{ingredient_id, state, WorldPoint{0, 0}};
    const auto& bobs = probe.bobs();
    const std::size_t bob_state = static_cast<std::size_t>(probe.bob_state());
    if (bob_state >= bobs.size()) {
      return Size{0, 0};
    }
    const int frame_index = bobs[bob_state];
    return zutaten_bob.size(frame_index);
  };
}
