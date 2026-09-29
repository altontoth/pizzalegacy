/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/character/Training.h"

Training::Training(const TrainingData& d)
    : name_(d.name), job_(d.job), duration_(d.duration),
      min_level_(d.min_level), cost_(d.cost), score_increase_(d.score_increase),
      pizzas_day_increase_(d.pizzas_day_increase), course_(d.course) {}

// Give us the index of a given training (from 0, for that job type).
int Training::course_index() const {
  int course = static_cast<int>(course_);
  if (course >= static_cast<int>(TrainingType::ACCOUNTING)) {
    return course - static_cast<int>(TrainingType::ACCOUNTING);
  }
  if (course >= static_cast<int>(TrainingType::SERVING_COURSE)) {
    return course - static_cast<int>(TrainingType::SERVING_COURSE);
  }
  return course - static_cast<int>(TrainingType::COOKERY_COURSE_1);
}
