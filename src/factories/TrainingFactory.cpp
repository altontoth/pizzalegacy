/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "factories/TrainingFactory.h"

#include "engine/Fatal.h"

#include <array>

namespace {

// This array is indexed implicitly by TrainingType
// (order must match the enum).
constexpr auto trainings = std::to_array<TrainingData>({
    // chefs
    {.name = "Cookery Course 1",
     .type = SkillType::COOKING,
     .duration = 4,
     .min_level = 0,
     .cost = 1500_usd,
     .score_increase = 5,
     .pizzas_day_increase = 0},
    {.name = "Cookery Course 2",
     .type = SkillType::COOKING,
     .duration = 4,
     .min_level = 50,
     .cost = 2500_usd,
     .score_increase = 6,
     .pizzas_day_increase = 0},
    {.name = "Avanti Avanti",
     .type = SkillType::COOKING,
     .duration = 6,
     .min_level = 40,
     .cost = 6000_usd,
     .score_increase = 0,
     .pizzas_day_increase = 40},
    {.name = "Specialities",
     .type = SkillType::COOKING,
     .duration = 7,
     .min_level = 60,
     .cost = 8000_usd,
     .score_increase = 15,
     .pizzas_day_increase = 0},
    {.name = "Super Cook",
     .type = SkillType::COOKING,
     .duration = 14,
     .min_level = 70,
     .cost = 10'000_usd,
     .score_increase = 15,
     .pizzas_day_increase = 10},
    {.name = "Master Cook",
     .type = SkillType::COOKING,
     .duration = 20,
     .min_level = 75,
     .cost = 18'000_usd,
     .score_increase = 0,
     .pizzas_day_increase = 30},

    // waiters
    {.name = "Serving Course",
     .type = SkillType::SERVING,
     .duration = 4,
     .min_level = 0,
     .cost = 2000_usd,
     .score_increase = 5,
     .pizzas_day_increase = 0},
    {.name = "Sales Course",
     .type = SkillType::SERVING,
     .duration = 7,
     .min_level = 40,
     .cost = 5000_usd,
     .score_increase = 8,
     .pizzas_day_increase = 0},
    {.name = "Fast-Selling",
     .type = SkillType::SERVING,
     .duration = 7,
     .min_level = 40,
     .cost = 7500_usd,
     .score_increase = 0,
     .pizzas_day_increase = 30},
    {.name = "Customer Relations",
     .type = SkillType::SERVING,
     .duration = 14,
     .min_level = 60,
     .cost = 9000_usd,
     .score_increase = 12,
     .pizzas_day_increase = 0},
    {.name = "Super-Selling",
     .type = SkillType::SERVING,
     .duration = 21,
     .min_level = 70,
     .cost = 12'000_usd,
     .score_increase = 14,
     .pizzas_day_increase = 10},
    {.name = "Waiter Diploma",
     .type = SkillType::SERVING,
     .duration = 28,
     .min_level = 75,
     .cost = 20'000_usd,
     .score_increase = 18,
     .pizzas_day_increase = 30},

    // managers
    {.name = "Accounting",
     .type = SkillType::MANAGEMENT,
     .duration = 7,
     .min_level = 40,
     .cost = 8000_usd,
     .score_increase = 6,
     .pizzas_day_increase = 0},
    {.name = "Book-keeping",
     .type = SkillType::MANAGEMENT,
     .duration = 7,
     .min_level = 45,
     .cost = 8000_usd,
     .score_increase = 6,
     .pizzas_day_increase = 0},
    {.name = "Stock Management",
     .type = SkillType::MANAGEMENT,
     .duration = 14,
     .min_level = 50,
     .cost = 10'000_usd,
     .score_increase = 8,
     .pizzas_day_increase = 0},
    {.name = "Profit Maximizing",
     .type = SkillType::MANAGEMENT,
     .duration = 21,
     .min_level = 60,
     .cost = 16'000_usd,
     .score_increase = 10,
     .pizzas_day_increase = 0},
    {.name = "Managing Personnel",
     .type = SkillType::MANAGEMENT,
     .duration = 28,
     .min_level = 70,
     .cost = 25'000_usd,
     .score_increase = 12,
     .pizzas_day_increase = 0},
    {.name = "Master Of Business",
     .type = SkillType::MANAGEMENT,
     .duration = 56,
     .min_level = 80,
     .cost = 50'000_usd,
     .score_increase = 15,
     .pizzas_day_increase = 0},
});

static_assert(trainings.size() ==
                  static_cast<std::size_t>(TrainingType::MASTER_OF_BUSINESS) +
                      1,
              "trainings table out of sync with TrainingType enum");

} // namespace

std::vector<Training> TrainingFactory::all_trainings() {
  std::vector<Training> list;
  list.reserve(std::size(trainings));
  for (const auto& data : trainings) {
    list.emplace_back(data);
  }
  return list;
}

std::vector<Training> TrainingFactory::trainings_by_skill(SkillType skill) {
  std::vector<Training> list;

  for (const auto& training : all_trainings()) {
    if (training.type() == skill) {
      list.push_back(training);
    }
  }

  return list;
}

Training TrainingFactory::create_training(TrainingType training_type) {
  auto index = static_cast<std::size_t>(training_type);
  if (index >= std::size(trainings)) {
    fatal("Invalid TrainingType requested");
  }
  return Training(trainings[index]);
}
