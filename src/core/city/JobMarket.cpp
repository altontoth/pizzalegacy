/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/JobMarket.h"

#include <algorithm>
#include <array>
#include <cassert>

#include "engine/Logger.h"
#include "engine/Random.h"
#include "resources/util/FileUtils.h"

namespace core::character {

void JobMarket::init(const std::vector<std::pair<uint8_t, int>>& player_chars) {
  // Determine how many doubles are available from t/double.e (2 lines per slot)
  auto double_lines = fileutils::load_text_lines("t/double.e");
  double_slots_ = static_cast<int>(double_lines.size()) / 2;

  slots_.assign(ORIGINAL_SLOTS + double_slots_, MarketStatus::CHEF);

  // Read chr/amarkt: 1 count byte + 100 role bytes (0=chef,1=waiter,2=manager)
  auto is = fileutils::open("chr/amarkt");
  uint8_t count = 0;
  is.read(reinterpret_cast<char*>(&count), 1);

  for (uint8_t i = 0; i < count && i < ORIGINAL_SLOTS; ++i) {
    uint8_t role_raw = 0;
    is.read(reinterpret_cast<char*>(&role_raw), 1);
    slots_[i] = static_cast<MarketStatus>(std::min(role_raw, uint8_t{2}));
  }

  // For every original character we see if we want to put them in the reserve
  // pool or not by flipping a coin. The first 9 times (per job role) that we
  // decide we want to put them in the reserve pool we ignore this result to
  // ensure the pool has at least 9 people in it. After those first 9 times
  // where the coin told us to put them in reserve but we ignored it, we start
  // actually honoring the result of the coin flip.
  std::array<int, 3> coin_flip_hits{};
  for (int i = 0; i < ORIGINAL_SLOTS; ++i) {
    if (!engine::random::coin_flip()) {
      continue; // leave this character available in the hire pool
    }

    // Consider removing from the pool
    auto job_role_idx = static_cast<uint8_t>(slots_[i]);
    assert(job_role_idx < 3);

    coin_flip_hits[job_role_idx]++;

    if (coin_flip_hits[job_role_idx] >= 10) {
      slots_[i] = to_reserve(slots_[i]);
    }
  }

  // Double slots start in reserve
  for (int i = ORIGINAL_SLOTS; i < total_slots(); ++i) {
    int base_id = (i - ORIGINAL_SLOTS) % ORIGINAL_SLOTS;
    auto base_role = static_cast<MarketStatus>(
        static_cast<uint8_t>(job_role(slots_[base_id])));
    slots_[i] = to_reserve(base_role);
  }

  // Remove player characters from the pool
  for (const auto& [char_id, player_idx] : player_chars) {
    slots_[char_id] = player_status(player_idx);
  }

  Logger::debug(
      "JobMarket initialized: {} originals, {} doubles, {} player characters",
      ORIGINAL_SLOTS, double_slots_, player_chars.size());
}

void JobMarket::end_of_day(int difficulty) {
  // We adjust the job market daily to make sure the number of candidates is in
  // line with the difficulty level.
  const int shrink_threshold = 16 - (difficulty * 2);
  const int grow_threshold = 26 - (difficulty * 2);

  // Count how many characters are currently visible in the hire screen per type
  std::array<int, 3> available{}; // indexed by JobRole
  for (int id = 0; id < total_slots(); ++id) {
    if (is_available(slots_[id])) {
      JobRole role = job_role(slots_[id]);
      available[static_cast<int>(role)]++;
    }
  }

  for (int id = 0; id < total_slots(); ++id) {
    MarketStatus s = slots_[id];
    if (is_hired_or_player(s)) {
      continue;
    }

    JobRole role = job_role(s);
    int& pool = available[static_cast<int>(role)];

    if (is_available(s) && pool > shrink_threshold) {
      // Pool is too large
      if (engine::random::rand_percentage_chance(30)) {
        slots_[id] = to_reserve(s);
        pool--;
      }
    }
    else if (is_reserve(s) && pool < grow_threshold) {
      // Pool is too small
      if (engine::random::rand_percentage_chance(30)) {
        slots_[id] = from_reserve(s);
        pool++;
      }
    }
  }

  // counts just for the debug log
  std::array<int, 3> reserve{};
  std::array<int, 3> hired{};
  for (int id = 0; id < total_slots(); ++id) {
    MarketStatus s = slots_[id];
    JobRole role = job_role(s);
    if (is_hired(s)) {
      hired[static_cast<int>(role)]++;
    }
    else if (is_reserve(s)) {
      reserve[static_cast<int>(role)]++;
    }
  }
  Logger::debug("JobMarket end_of_day (difficulty {}): "
                "chefs {}/{}/{}, waiters {}/{}/{}, managers {}/{}/{} "
                "(available/reserve/hired), thresholds shrink>{} grow<{}",
                difficulty, available[0], reserve[0], hired[0], available[1],
                reserve[1], hired[1], available[2], reserve[2], hired[2],
                shrink_threshold, grow_threshold);
}

} // namespace core::character
