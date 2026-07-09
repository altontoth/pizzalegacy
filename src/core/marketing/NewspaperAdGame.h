/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/finance/Money.h"
#include "core/marketing/NewspaperAdFormat.h"

// The newspaper advertising mini-game
//
// The advertisement is one sentence split into 5 parts. WERBUNG.E holds 5
// blocks of 28 fragment lines, each followed by 2 blank separator lines;
// using one sentence fragment per block you're supposed to form one coherent
// slogan. Each round shows a 5-fragment sliding window over that round's
// block and the player picks one. A score multiplier starts at 50 and is
// halved every time the chosen fragment index differs from the previous
// rounds.

class NewspaperAdGame {
public:
  enum class Phase {
    SegmentSelection,
    FormatSelect,
  };

  static constexpr int NUM_ROUNDS = 5;
  static constexpr int OPTIONS_PER_ROUND = 5;
  static constexpr int BLOCK_LINES = 28; // fragment lines per sentence part
  static constexpr int BLOCK_GAP = 2; // blank separator lines after each block
  static constexpr int WINDOW_MAX_START = BLOCK_LINES - OPTIONS_PER_ROUND;
  static constexpr int WINDOW_INIT_RANGE = 22; // random(22) initial window
  static constexpr int START_MULTIPLIER = 50;
  static constexpr int NUM_AD_SIZES = 4;
  // 1-indexed line number in WERBUNG.E where the size labels start.
  static constexpr int SIZE_LABEL_BASE_LINE = 150;

  // lines: the WERBUNG.E lines where the '#' has already been replaced by the
  // company name.
  void start(std::vector<std::string> lines, std::string company,
             bool fix_legacy_bugs);

  [[nodiscard]] Phase phase() const { return phase_; }
  [[nodiscard]] int current_round() const { return round_; }

  // The OPTIONS_PER_ROUND fragments shown this round, '#' substituted.
  [[nodiscard]] std::array<std::string, OPTIONS_PER_ROUND>
  current_options() const;

  // Pick option `row` (0..OPTIONS_PER_ROUND-1) for the current round. Advances
  // to the next round, or into SizeSelect after the final round.
  void select(int row);

  [[nodiscard]] NewspaperAdFormat ad_format() const { return ad_format_; }
  void size_up();
  void size_down();
  [[nodiscard]] std::string size_label() const;

  [[nodiscard]] int effectiveness() const;
  [[nodiscard]] Money cost() const;

  // The fragments chosen so far, for the live assembled-ad preview.
  [[nodiscard]] const std::vector<std::string>& chosen_fragments() const {
    return chosen_text_;
  }

private:
  [[nodiscard]] std::string substitute_company(const std::string& line) const;
  void begin_round(); // random-walk the window and clamp it

  std::vector<std::string> lines_;
  std::string company_;
  Phase phase_{Phase::SegmentSelection};
  int round_{0};
  int window_start_{0}; // var_14
  int multiplier_{START_MULTIPLIER};
  NewspaperAdFormat ad_format_{NewspaperAdFormat::HalfTwoColor};
  bool fix_legacy_bugs_{false};
  // Absolute fragment index (window + row, 0..27) chosen each round.
  std::array<int, NUM_ROUNDS> chosen_index_{};
  std::vector<std::string> chosen_text_;
};
