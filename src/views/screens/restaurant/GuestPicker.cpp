/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/GuestPicker.h"

#include "core/Demographics.h"
#include "engine/Random.h"

namespace views::screens::restaurant {

ConsumerType pick_weighted_demographic(const CustomerCounts& counts) {
  int total = 0;
  for (auto ct : ALL_CONSUMER_TYPES) {
    total += counts[ct];
  }
  if (total == 0) {
    constexpr int DEMO_COUNT = static_cast<int>(ConsumerType::Count);
    return static_cast<ConsumerType>(engine::random::rand_max_n(DEMO_COUNT));
  }
  int roll = engine::random::rand_max_n(total);
  for (ConsumerType ct : ALL_CONSUMER_TYPES) {
    roll -= counts[ct];
    if (roll < 0) {
      return ct;
    }
  }
  return ALL_CONSUMER_TYPES.back();
}

int pick_guest_type(ConsumerType demo) {
  return static_cast<int>(demo) + (engine::random::coin_flip() ? 6 : 0);
}

int pick_face_index(int guest_type) {
  return (guest_type * 2) + (engine::random::coin_flip() ? 1 : 0);
}

} // namespace views::screens::restaurant
