/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/food/Pizza.h"

struct CompetitionParticipant {
  core::PlayerId player_id;
  bool is_human{false};
  Money bet;
  int match_score{0};
  int time_ticks{0};
  int final_score{0};
};

struct CompetitionSession {
  std::vector<CompetitionParticipant> participants;
  std::shared_ptr<core::food::Pizza> canonical_pizza;
  int pizza_index{0};
  int speed_level{1};
  int delay_ticks{0};
  int reveal_idx{0};
  bool reveal_done{false};
  bool get_ready{false};
  uint64_t get_ready_start_ms{0};
  bool is_challenge{false};
  int challenger_slot{-1};
  int human_slot{-1};
  int results_rows_shown{0};
  uint64_t results_last_row_ms{0};
  int winner_slot{-1};
  bool all_tied{false};
  uint64_t human_design_ticks{0};
};
