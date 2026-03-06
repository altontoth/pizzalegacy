/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <iterator>

namespace engine::random {
int rand_max_n(int max);
int rand_between(int min, int max);
bool rand_percentage_chance(int percentage);
bool coin_flip();
int watcom_rand15();

template <std::random_access_iterator It> void shuffle(It first, It last) {
  for (auto i = last - first - 1; i > 0; --i) {
    std::swap(first[i], first[rand_max_n(static_cast<int>(i + 1))]);
  }
}
} // namespace engine::random
