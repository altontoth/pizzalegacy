/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/factories/TrainingFactory.h"

#include "core/character/Training.h"
#include "core/factories/DataFile.h"
#include "core/finance/Money.h"
#include "engine/Fatal.h"

#include <map>
#include <string>
#include <vector>

namespace {

std::map<TrainingType, TrainingData> trainings_;

TrainingData read_training(const data_file::json& entry, TrainingType course) {
  return TrainingData{
      .name = entry.at("name").get<std::string>(),
      .job = data_file::enum_field<core::character::JobRole>(entry, "job"),
      .duration = entry.at("duration").get<int>(),
      .min_level = entry.at("min_level").get<int>(),
      .cost = Money::from_dollars(entry.at("cost").get<int>()),
      .score_increase = entry.at("score_increase").get<int>(),
      .pizzas_day_increase = entry.at("pizzas_day_increase").get<int>(),
      .course = course,
  };
}

void ensure_loaded() {
  if (!trainings_.empty()) {
    return;
  }
  trainings_ = data_file::load<TrainingData, TrainingType>("trainings.json",
                                                           read_training);
}

} // namespace

std::vector<Training> TrainingFactory::all_trainings() {
  ensure_loaded();
  std::vector<Training> list;
  list.reserve(trainings_.size());
  for (const auto& [course, data] : trainings_) {
    list.emplace_back(data);
  }
  return list;
}

std::vector<Training>
TrainingFactory::trainings_for(core::character::JobRole job) {
  std::vector<Training> list;

  for (const auto& training : all_trainings()) {
    if (training.job() == job) {
      list.push_back(training);
    }
  }

  return list;
}

Training TrainingFactory::create_training(TrainingType training_type) {
  ensure_loaded();
  const auto it = trainings_.find(training_type);
  if (it == trainings_.end()) {
    fatal("Invalid TrainingType requested");
  }
  return Training(it->second);
}
