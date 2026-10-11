/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/food/ToppingSize.h"

#include <cstddef>

#include "core/food/Topping.h"
#include "engine/geom/Spaces.h"
#include "resources/Bob.h"

namespace core::food {

BobSizeFn topping_size_fn(BobManager& bobs) {
  Bob& zutaten_bob = bobs.get("bob/zutaten.bob");
  return [&zutaten_bob](int ingredient_id, int state) -> Size {
    const Topping probe{ingredient_id, state, WorldPoint{0, 0}};
    const auto& pictures = probe.bobs();
    const auto bob_state = static_cast<std::size_t>(probe.bob_state());
    if (bob_state >= pictures.size()) {
      return Size{0, 0};
    }
    return zutaten_bob.size(pictures[bob_state]);
  };
}

} // namespace core::food
