/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include "core/finance/Money.h"

enum class SkillType { NONE, COOKING, SERVING, MANAGEMENT };

class Training {
public:
  Training(const std::string& name, SkillType type, int duration, int min_level,
           Money cost, int score_increase, int pizzas_day_increase);

  // Use the default copy semantics.
  Training(const Training&) = default;
  Training& operator=(const Training&) = default;

  // Getters
  const std::string& name() const { return name_; }
  SkillType type() const { return type_; }
  int duration() const { return duration_; }
  int min_level() const { return min_level_; }
  Money cost() const { return cost_; }
  int score_increase() const { return score_increase_; }
  int pizzas_day_increase() const { return pizzas_day_increase_; }

private:
  std::string name_;
  SkillType type_;
  int duration_;
  int min_level_;
  Money cost_;
  int score_increase_;
  int pizzas_day_increase_;
};
