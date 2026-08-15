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
// The same bounded draw the original uses. The multiply runs in 32 bits and
// wraps there, so max is read as unsigned and the result never reaches 131072.
int rand_max_n(int max);

// Uniform over [min, max), so max is one past the largest value it returns.
int rand_between(int min, int max);

// Two draws over [0, spread) added together, so the result is triangular
// over [base - spread, base + spread - 2] and peaks at base - 1.
int rand_triangular(int base, int spread);

bool rand_percentage_chance(int percentage);
bool coin_flip();

uint16_t ansi_rand15();

// Seed both streams. Called once at startup.
void seed(uint32_t s);

// Direct state access for serialization.
uint32_t get_state();
void set_state(uint32_t s);

template <std::random_access_iterator It> void shuffle(It first, It last) {
  for (auto i = last - first - 1; i > 0; --i) {
    std::swap(first[i], first[rand_max_n(static_cast<int>(i + 1))]);
  }
}

// A second stream for draws that only affect what is on screen. It is seeded
// at startup and never saved, so time spent watching a screen leaves the game
// unchanged. Draws that decide an outcome should call the same functions in
// engine::random instead.
namespace presentation {
int rand_max_n(int max);
int rand_between(int min, int max);
bool rand_percentage_chance(int percentage);
bool coin_flip();

template <std::random_access_iterator It> void shuffle(It first, It last) {
  for (auto i = last - first - 1; i > 0; --i) {
    std::swap(first[i], first[rand_max_n(static_cast<int>(i + 1))]);
  }
}
} // namespace presentation
} // namespace engine::random
