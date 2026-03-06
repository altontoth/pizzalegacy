/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/character/Training.h"

Training::Training(const std::string& name, SkillType type, int duration,
                   int min_level, Money cost, int score_increase,
                   int pizzas_day_increase)
    : name_(name), type_(type), duration_(duration), min_level_(min_level),
      cost_(cost), score_increase_(score_increase),
      pizzas_day_increase_(pizzas_day_increase) {}
