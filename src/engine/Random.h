/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>

namespace engine::random {
int rand_max_n(int max);
int rand_between(int min, int max);
bool rand_percentage_chance(int percentage);
bool coin_flip();

uint16_t ansi_rand15();

// Seed the LCG. Called once at startup; also called by the save system on load.
void seed(uint32_t s);

// Direct state access for serialization.
uint32_t get_state();
void set_state(uint32_t s);

template <std::random_access_iterator It> void shuffle(It first, It last) {
  for (auto i = last - first - 1; i > 0; --i) {
    std::swap(first[i], first[rand_max_n(static_cast<int>(i + 1))]);
  }
}
} // namespace engine::random
