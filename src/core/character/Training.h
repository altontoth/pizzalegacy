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

enum class TrainingType {
  // Cooking:
  COOKERY_COURSE_1,
  COOKERY_COURSE_2,
  AVANTI_AVANTI,
  SPECIALITIES,
  SUPER_COOK,
  MASTER_COOK,

  // Serving:
  SERVING_COURSE,
  SALES_COURSE,
  FAST_SELLING,
  CUSTOMER_RELATIONS,
  SUPER_SELLING,
  WAITER_DIPLOMA,

  // Management:
  ACCOUNTING,
  BOOK_KEEPING,
  STOCK_MANAGEMENT,
  PROFIT_MAXIMIZING,
  MANAGING_PERSONNEL,
  MASTER_OF_BUSINESS,
};

struct TrainingData {
  std::string name;
  SkillType type;
  int duration;  // days
  int min_level; // score requirement
  Money cost;
  int score_increase; // skill bonus
  int pizzas_day_increase;
  TrainingType course;
};

class Training {
public:
  explicit Training(const TrainingData& data);

  // Use the default copy semantics.
  Training(const Training&) = default;
  Training& operator=(const Training&) = default;

  // Getters
  const std::string& name() const { return name_; }
  SkillType type() const { return type_; }
  TrainingType course() const { return course_; }
  // 0-5 position of this course within its skill category.
  int course_index() const;
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
  TrainingType course_;
};
