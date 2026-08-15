/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/Random.h"

#include <cstdint>

namespace engine::random {

namespace {
uint32_t simulation_state = 1;
uint32_t presentation_state = 1;

// An arbitrary offset so the two streams do not start from the same value.
constexpr uint32_t presentation_seed_offset = 1000001u;

constexpr int rand_bits = 15;
constexpr int rand_output_shift = 16;
constexpr uint32_t rand_mask = (1u << rand_bits) - 1;

// 32-bit linear congruential generator
// Returns bits 30..16, because apparently the upper 15 bits is where an LCG's
// randomness quality is highest. Range 0..32767.
uint16_t next15(uint32_t& state) {
  state = state * 1103515245u + 12345u;
  return static_cast<uint16_t>((state >> rand_output_shift) & rand_mask);
}

int max_n(uint32_t& state, int max) {
  int r = next15(state); // 0..32767
  long long product = static_cast<long long>(r) * max;
  // Shifting by rand_bits is floor division by 32768.
  return static_cast<int>(product >> rand_bits);
}

// No draw at 0 or 100, so the stream does not advance there.
bool percentage_chance(uint32_t& state, int percentage) {
  if (percentage <= 0) {
    return false;
  }
  if (percentage >= 100) {
    return true;
  }
  return max_n(state, 100) < percentage;
}
} // anonymous namespace

uint16_t ansi_rand15() { return next15(simulation_state); }

void seed(uint32_t s) {
  simulation_state = s;
  presentation_state = s + presentation_seed_offset;
}

uint32_t get_state() { return simulation_state; }
void set_state(uint32_t s) { simulation_state = s; }

int rand_max_n(int max) { return max_n(simulation_state, max); }

int rand_between(int min, int max) { return rand_max_n(max - min) + min; }

int rand_triangular(int base, int spread) {
  const int first = rand_max_n(spread);
  const int second = rand_max_n(spread);
  return base + first + second - spread;
}

bool rand_percentage_chance(int percentage) {
  return percentage_chance(simulation_state, percentage);
}

bool coin_flip() { return rand_percentage_chance(50); }

namespace presentation {

int rand_max_n(int max) { return max_n(presentation_state, max); }

int rand_between(int min, int max) { return rand_max_n(max - min) + min; }

bool rand_percentage_chance(int percentage) {
  return percentage_chance(presentation_state, percentage);
}

bool coin_flip() { return rand_percentage_chance(50); }

} // namespace presentation

} // namespace engine::random
