/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/character/Training.h"

enum class TrainingType {
  // Cooking (6)
  COOKERY_COURSE_1,
  COOKERY_COURSE_2,
  AVANTI_AVANTI,
  SPECIALITIES,
  SUPER_COOK,
  MASTER_COOK,

  // Serving (6)
  SERVING_COURSE,
  SALES_COURSE,
  FAST_SELLING,
  CUSTOMER_RELATIONS,
  SUPER_SELLING,
  WAITER_DIPLOMA,

  // Management (6)
  ACCOUNTING,
  BOOK_KEEPING,
  STOCK_MANAGEMENT,
  PROFIT_MAXIMIZING,
  MANAGING_PERSONNEL,
  MASTER_OF_BUSINESS,
};

#include <vector>

class TrainingFactory {
public:
  static Training create_training(TrainingType type);
  static std::vector<Training> all_trainings();
  static std::vector<Training> trainings_by_skill(SkillType skill);
};
