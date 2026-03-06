/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "factories/TrainingFactory.h"

std::vector<Training> TrainingFactory::all_trainings() {
  std::vector<Training> list;

  // Iterate through all TrainingType values
  list.push_back(create_training(TrainingType::COOKERY_COURSE_1));
  list.push_back(create_training(TrainingType::COOKERY_COURSE_2));
  list.push_back(create_training(TrainingType::AVANTI_AVANTI));
  list.push_back(create_training(TrainingType::SPECIALITIES));
  list.push_back(create_training(TrainingType::SUPER_COOK));
  list.push_back(create_training(TrainingType::MASTER_COOK));

  list.push_back(create_training(TrainingType::SERVING_COURSE));
  list.push_back(create_training(TrainingType::SALES_COURSE));
  list.push_back(create_training(TrainingType::FAST_SELLING));
  list.push_back(create_training(TrainingType::CUSTOMER_RELATIONS));
  list.push_back(create_training(TrainingType::SUPER_SELLING));
  list.push_back(create_training(TrainingType::WAITER_DIPLOMA));

  list.push_back(create_training(TrainingType::ACCOUNTING));
  list.push_back(create_training(TrainingType::BOOK_KEEPING));
  list.push_back(create_training(TrainingType::STOCK_MANAGEMENT));
  list.push_back(create_training(TrainingType::PROFIT_MAXIMIZING));
  list.push_back(create_training(TrainingType::MANAGING_PERSONNEL));
  list.push_back(create_training(TrainingType::MASTER_OF_BUSINESS));

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

  switch (training_type) {
  // chefs
  case TrainingType::COOKERY_COURSE_1:
    return {
        "Cookery Course 1", // name
        SkillType::COOKING, // type
        4,                  // duration (days)
        0,                  // min_level (score requirement)
        1500_usd,           // cost
        5,                  // score_increase (skill bonus)
        0,                  // pizzas_day_increase
    };

  case TrainingType::COOKERY_COURSE_2:
    return {"Cookery Course 2", SkillType::COOKING, 4, 50, 2500_usd, 6, 0};

  case TrainingType::AVANTI_AVANTI:
    return {"Avanti Avanti", SkillType::COOKING, 6, 40, 6000_usd, 0, 40};

  case TrainingType::SPECIALITIES:
    return {"Specialities", SkillType::COOKING, 7, 60, 8000_usd, 15, 0};

  case TrainingType::SUPER_COOK:
    return {"Super Cook", SkillType::COOKING, 14, 70, 10'000_usd, 15, 10};

  case TrainingType::MASTER_COOK:
    return {"Master Cook", SkillType::COOKING, 20, 75, 18'000_usd, 0, 30};

  // waiters
  case TrainingType::SERVING_COURSE:
    return {"Serving Course", SkillType::SERVING, 4, 0, 2000_usd, 5, 0};

  case TrainingType::SALES_COURSE:
    return {"Sales Course", SkillType::SERVING, 7, 40, 5000_usd, 8, 0};

  case TrainingType::FAST_SELLING:
    return {"Fast-Selling", SkillType::SERVING, 7, 40, 7500_usd, 0, 30};

  case TrainingType::CUSTOMER_RELATIONS:
    return {"Customer Relations", SkillType::SERVING, 14, 60, 9000_usd, 12, 0};

  case TrainingType::SUPER_SELLING:
    return {"Super-Selling", SkillType::SERVING, 21, 70, 12'000_usd, 14, 10};

  case TrainingType::WAITER_DIPLOMA:
    return {"Waiter Diploma", SkillType::SERVING, 28, 75, 20'000_usd, 18, 30};

  // managers
  case TrainingType::ACCOUNTING:
    return {"Accounting", SkillType::MANAGEMENT, 7, 40, 8000_usd, 6, 0};

  case TrainingType::BOOK_KEEPING:
    return {"Book-keeping", SkillType::MANAGEMENT, 7, 45, 8000_usd, 6, 0};

  case TrainingType::STOCK_MANAGEMENT:
    return {
        "Stock Management", SkillType::MANAGEMENT, 14, 50, 10'000_usd, 8, 0};

  case TrainingType::PROFIT_MAXIMIZING:
    return {
        "Profit Maximizing", SkillType::MANAGEMENT, 21, 60, 16'000_usd, 10, 0};

  case TrainingType::MANAGING_PERSONNEL:
    return {
        "Managing Personnel", SkillType::MANAGEMENT, 28, 70, 25'000_usd, 12, 0};

  case TrainingType::MASTER_OF_BUSINESS:
    return {
        "Master Of Business", SkillType::MANAGEMENT, 56, 80, 50'000_usd, 15, 0};
  }

  // Should not be reached
  return {"Unknown", SkillType::NONE, 0, 0, Money{0}, 0, 0};
}
