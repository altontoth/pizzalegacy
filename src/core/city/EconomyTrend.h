/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstdint>

constexpr int max_history = 60;

// City-level macro-economic indicator.
class EconomyTrend {
public:
  // fix_legacy_bugs keeps a fresh trend from picking a counter of zero.
  void tick(bool fix_legacy_bugs);

  int current() const { return current_; }
  void current(int v) { current_ = v; }

  int target() const { return target_; }
  void target(int v) { target_ = v; }

  // Weeks left before a new goal is picked.
  int counter() const { return counter_; }
  void counter(int v) { counter_ = static_cast<uint8_t>(v); }

  int history_count() const { return history_count_; }
  void history_count(int c) { history_count_ = c; }

  const std::array<int, max_history>& history() const { return history_; }
  void history(std::array<int, max_history> h) { history_ = h; }

private:
  void start_new_trend(bool fix_legacy_bugs);

  int current_{0};
  int target_{0};
  uint8_t counter_{0};
  std::array<int, max_history> history_{};
  int history_count_{0};
};
