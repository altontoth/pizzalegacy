/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/Random.h"

#include <cstdint>

namespace engine::random {

namespace {
uint32_t rng_state = 1;
} // anonymous namespace

// 32-bit linear congruential generator
// Returns bits 30..16, because apparently the upper 15 bits is where an LCG's
// randomness quality is highest. Range 0..32767.
uint16_t ansi_rand15() {
  rng_state = rng_state * 1103515245u + 12345u;
  return static_cast<uint16_t>((rng_state >> 16) & 0x7FFF);
}

void seed(uint32_t s) { rng_state = s; }
uint32_t get_state() { return rng_state; }
void set_state(uint32_t s) { rng_state = s; }

int rand_max_n(int max) {
  int r = ansi_rand15(); // 0..32767
  long long product = static_cast<long long>(r) * max;
  // shr 15 is floor division by 32768
  return static_cast<int>(product >> 15);
}

int rand_between(int min, int max) { return rand_max_n(max - min) + min; }

bool rand_percentage_chance(int percentage) {
  if (percentage <= 0) {
    return false;
  }
  if (percentage >= 100) {
    return true;
  }
  return rand_max_n(100) < percentage;
}

bool coin_flip() { return rand_percentage_chance(50); }

} // namespace engine::random
