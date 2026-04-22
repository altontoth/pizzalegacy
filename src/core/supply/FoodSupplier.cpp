/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/supply/FoodSupplier.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include "core/food/Ingredient.h"
#include "core/food/IngredientCategory.h"

FoodSupplier::FoodSupplier(IngredientCategory category, uint8_t supplier_id,
                           uint8_t quality, std::string name, uint8_t head_id,
                           int min_contract, const uint8_t* price_level,
                           int price_level_count)
    : category_(category), supplier_id_(supplier_id), quality_(quality),
      name_(std::move(name)), head_id_(head_id),
      min_contract_amount_(Money{static_cast<int64_t>(min_contract * 100)}) {
  auto meta = core::food::CategoryRegistry::get(category_);
  actor_type_ = meta.supplier_type;
  // Copy price levels into owned array
  price_levels_.fill(0);
  for (int i = 0; i < price_level_count && i < 22; ++i) {
    price_levels_[i] = price_level[i];
  }
}

Money FoodSupplier::minimum_contract_amount() const {
  return min_contract_amount_;
}

int FoodSupplier::number_of_products() const {
  switch (category_) {
  case IngredientCategory::VEGETABLES:
    return VEGETABLE_COUNT;
  case IngredientCategory::MEAT:
    return MEAT_COUNT;
  case IngredientCategory::FRUIT:
    return FRUIT_COUNT;
  case IngredientCategory::CHEESE:
    return CHEESE_COUNT;
  }
}

ActorType FoodSupplier::actor_type() const { return actor_type_; }

uint8_t FoodSupplier::id() const { return supplier_id_; }

uint8_t FoodSupplier::quality() const { return quality_; }

const std::string& FoodSupplier::name() const { return name_; }

uint8_t FoodSupplier::price_level_for_ingredient(
    core::food::IngredientType ingredient_type) const {
  using namespace core::food;
  auto range = ingredient_range_for_category(category_);
  auto ingredient_id = static_cast<int>(ingredient_type);
  auto first_id = static_cast<int>(range.first);
  auto last_id = static_cast<int>(range.last);

  if (ingredient_id < first_id || ingredient_id > last_id) {
    return 0;
  }
  int array_index = ingredient_id - first_id;
  if (array_index < number_of_products() && array_index < 22) {
    return price_levels_[array_index];
  }
  return 0;
}

Money FoodSupplier::calculate_price(core::food::IngredientType ingredient_type,
                                    Money base_price,
                                    float difficulty_multiplier) const {
  if (base_price.cents == 0) {
    return Money{0};
  }

  uint8_t price_level = price_level_for_ingredient(ingredient_type);

  // Formula: base = round((float)T * DifficultyMultiplier * (Q / 100.0))
  float calculated = static_cast<float>(base_price.cents) *
                     difficulty_multiplier * (price_level / 100.0f);
  int64_t final_price = static_cast<int64_t>(std::round(calculated));

  return Money{final_price};
}

uint8_t FoodSupplier::head_id() const { return head_id_; }

SupplierRelationship& FoodSupplier::relationship(core::PlayerId player_id) {
  return relationships_[player_id];
}

const SupplierRelationship&
FoodSupplier::relationship(core::PlayerId player_id) const {
  return relationships_[player_id];
}

int FoodSupplier::customer_tier(core::PlayerId player_id) const {
  const auto& rel = relationship(player_id);
  // Customer size category based on historical contract count
  // Small: 0-3, Medium: 4-9, Large: 10+
  return 10;
  if (rel.contract_count >= 10) {
    return 10;
  }
  if (rel.contract_count >= 4) {
    return 5;
  }
  return rel.contract_count;
}

uint8_t FoodSupplier::chance_of_no_delivery() const {
  // The original uses negative for impossible but I prefer keeping it 0-100.
  return std::clamp(0x19 - (quality_ >> 2), 0, 100);
}

int FoodSupplier::category_index(
    core::food::IngredientType ingredient_type) const {
  using namespace core::food;
  auto ingredient_id = static_cast<int>(ingredient_type);

  // Map ingredient ID to array index based on supplier type
  switch (category_) {
  case IngredientCategory::VEGETABLES:
    if (ingredient_id >= static_cast<int>(IngredientType::TOMATOES) &&
        ingredient_id <= static_cast<int>(IngredientType::W_MUSHROOMS)) {
      return ingredient_id - static_cast<int>(IngredientType::TOMATOES);
    }
    break;
  case IngredientCategory::MEAT:
    if (ingredient_id >= static_cast<int>(IngredientType::SALAMI) &&
        ingredient_id <= static_cast<int>(IngredientType::MUSSELS)) {
      return ingredient_id - static_cast<int>(IngredientType::SALAMI);
    }
    break;
  case IngredientCategory::FRUIT:
    if (ingredient_id >= static_cast<int>(IngredientType::APPLES) &&
        ingredient_id <= static_cast<int>(IngredientType::GOOSEBERRIES)) {
      return ingredient_id - static_cast<int>(IngredientType::APPLES);
    }
    break;
  case IngredientCategory::CHEESE:
    if (ingredient_id >= static_cast<int>(IngredientType::EGG) &&
        ingredient_id <= static_cast<int>(IngredientType::OREGANO)) {
      return ingredient_id - static_cast<int>(IngredientType::EGG);
    }
    break;
  }
  return -1; // Invalid ingredient for this supplier
}

int FoodSupplier::stock(core::food::IngredientType ingredient_type) const {
  int idx = category_index(ingredient_type);
  return (idx >= 0 && idx < 22) ? stock_levels_[idx] : 0;
}

void FoodSupplier::set_stock(core::food::IngredientType ingredient_type,
                             int quantity) {
  int idx = category_index(ingredient_type);
  if (idx >= 0 && idx < 22) {
    stock_levels_[idx] = quantity;
  }
}
