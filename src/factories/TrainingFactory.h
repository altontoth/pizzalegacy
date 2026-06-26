/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/character/Training.h"

#include <vector>

class TrainingFactory {
public:
  static Training create_training(TrainingType type);
  static std::vector<Training> all_trainings();
  static std::vector<Training> trainings_by_skill(SkillType skill);
};
