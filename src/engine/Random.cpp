/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/Random.h"
#include <cstdlib>

namespace engine::random {

// Trying to match the random behavior from the original:
int rand_max_n(int max) {
  int r = watcom_rand15(); // 0..32767
  long long product = static_cast<long long>(r) * max;
  // product is up to 32767 * param
  // shr 15 is floor division by 32768:
  return static_cast<int>(product >> 15);
}

int rand_between(int min, int max) {
  auto rand = rand_max_n(max - min);
  return rand + min;
}

int watcom_rand15() { return std::rand() & 0x7FFF; }

bool rand_percentage_chance(int percentage) {
  if (percentage <= 0) {
    return false;
  }
  if (percentage >= 100) {
    return true;
  }
  // param is between 1..99
  int r = watcom_rand15();
  int r_scaled = (r * 100) >> 15; // uniform in [0..99]
  return r_scaled < percentage;
}

bool coin_flip() { return rand_percentage_chance(50); }

} // namespace engine::random
