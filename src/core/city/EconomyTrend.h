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
// Wanders randomly between -120 and +120 over cycles of 0-39 days.
// Used by the control map: base_threshold = (40 - current/3) * 64
// Also displayed on the Market Research inflation graph (60-day history).
class EconomyTrend {
public:
  void daily_tick();

  int current() const { return current_; }
  void set_current(int v) { current_ = v; }

  int target() const { return target_; }
  void set_target(int v) { target_ = v; }

  int days_remaining() const { return days_remaining_; }
  void set_days_remaining(int d) { days_remaining_ = d; }

  int history_count() const { return history_count_; }
  void set_history_count(int c) { history_count_ = c; }

  const std::array<int, max_history>& history() const { return history_; }
  void set_history(std::array<int, max_history> h) { history_ = h; }

private:
  void pick_new_target();

  int current_{0};
  int target_{0};
  int days_remaining_{0};
  std::array<int, max_history> history_{};
  int history_count_{0};
};
