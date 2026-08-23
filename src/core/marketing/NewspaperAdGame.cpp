/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/marketing/NewspaperAdGame.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "engine/Random.h"
#include "resources/util/FileUtils.h"

namespace {
constexpr int AD_SIZE_BASE = 80; // effectiveness base at size 0
constexpr int AD_SIZE_STEP = 20; // base increment per size
constexpr int AD_SIZE_COST_STEP_DOLLARS = 500;
constexpr int WINDOW_WALK_RANGE = 5; // random(5) - 2 -> -2..+2 drift
constexpr int WINDOW_WALK_BIAS = 2;
} // namespace

void NewspaperAdGame::start(std::vector<std::string> lines, std::string company,
                            bool fix_legacy_bugs) {
  lines_ = std::move(lines);
  company_ = std::move(company);
  fix_legacy_bugs_ = fix_legacy_bugs;
  phase_ = Phase::SegmentSelection;
  round_ = 0;
  multiplier_ = START_MULTIPLIER;
  ad_format_ = NewspaperAdFormat::HalfTwoColor;
  chosen_index_.fill(0);
  chosen_text_.clear();
  // var_14 init = random(22), then walked at the top of every round.
  window_start_ = engine::random::rand_max_n(WINDOW_INIT_RANGE);
  begin_round();
}

void NewspaperAdGame::begin_round() {
  const int drift =
      engine::random::rand_max_n(WINDOW_WALK_RANGE) - WINDOW_WALK_BIAS;
  window_start_ = std::clamp(window_start_ + drift, 0, WINDOW_MAX_START);

  // Original-game bug(?): the option window random-walks each round and can
  // drift past the fragment you just picked, so it is no longer offered and the
  // slogan cannot be kept consistent (which is what scoring rewards). With
  // fix_legacy_bugs on, we re-clamp the window so the previous round's pick
  // stays in view.
  if (fix_legacy_bugs_ && round_ > 0) {
    const int anchor = chosen_index_[round_ - 1];
    const int lo = std::max(0, anchor - (OPTIONS_PER_ROUND - 1));
    const int hi = std::min(WINDOW_MAX_START, anchor);
    window_start_ = std::clamp(window_start_, lo, hi);
  }
}

std::string NewspaperAdGame::substitute_company(const std::string& line,
                                                const std::string& company) {
  auto pos = line.find('#');
  if (pos == std::string::npos) {
    return line;
  }
  return line.substr(0, pos) + company + line.substr(pos + 1);
}

const std::vector<std::string>& NewspaperAdGame::ad_lines() {
  static std::vector<std::string> cache =
      fileutils::load_text_lines("t/werbung.e");
  return cache;
}

std::vector<std::string>
NewspaperAdGame::slogan(const std::array<int, NUM_ROUNDS>& indices,
                        const std::string& company) {
  const auto& lines = ad_lines();
  std::vector<std::string> parts;
  for (int round = 0; round < NUM_ROUNDS; ++round) {
    const int idx = (round * (BLOCK_LINES + BLOCK_GAP)) + indices[round];
    if (idx < 0 || idx >= static_cast<int>(lines.size())) {
      return {};
    }
    parts.push_back(substitute_company(lines[idx], company));
  }
  return parts;
}

std::array<std::string, NewspaperAdGame::OPTIONS_PER_ROUND>
NewspaperAdGame::current_options() const {
  std::array<std::string, OPTIONS_PER_ROUND> options;
  const int base = (round_ * (BLOCK_LINES + BLOCK_GAP)) + window_start_;
  for (int row = 0; row < OPTIONS_PER_ROUND; ++row) {
    const int idx = base + row;
    if (idx >= 0 && idx < static_cast<int>(lines_.size())) {
      options[row] = substitute_company(lines_[idx], company_);
    }
  }
  return options;
}

void NewspaperAdGame::select(int row) {
  if (phase_ != Phase::SegmentSelection) {
    return;
  }
  const int absolute = window_start_ + row;
  if (round_ > 0 && absolute != chosen_index_[round_ - 1]) {
    multiplier_ /= 2; // mismatch: halve the multiplier
  }
  chosen_index_[round_] = absolute;
  chosen_text_.push_back(current_options()[row]);

  if (round_ + 1 >= NUM_ROUNDS) {
    phase_ = Phase::FormatSelect;
    return;
  }
  ++round_;
  begin_round();
}

void NewspaperAdGame::size_up() {
  const int next = std::min(NUM_AD_SIZES - 1, static_cast<int>(ad_format_) + 1);
  ad_format_ = static_cast<NewspaperAdFormat>(next);
}

void NewspaperAdGame::size_down() {
  const int prev = std::max(0, static_cast<int>(ad_format_) - 1);
  ad_format_ = static_cast<NewspaperAdFormat>(prev);
}

std::string NewspaperAdGame::size_label() const {
  const int idx = SIZE_LABEL_BASE_LINE - 1 + static_cast<int>(ad_format_);
  if (idx >= 0 && idx < static_cast<int>(lines_.size())) {
    return lines_[idx];
  }
  return {};
}

int NewspaperAdGame::effectiveness() const {
  const int base = AD_SIZE_BASE + (AD_SIZE_STEP * static_cast<int>(ad_format_));
  return multiplier_ * base / 100;
}

Money NewspaperAdGame::cost() const {
  const int dollars =
      (static_cast<int>(ad_format_) + 1) * AD_SIZE_COST_STEP_DOLLARS;
  return Money{static_cast<int64_t>(dollars) * 100};
}
