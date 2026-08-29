/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/food/IngredientCategory.h"

#include <optional>

namespace core::food {

const std::array<CategoryMetadata, 4> CategoryRegistry::metadata_ = {{
    {.category = IngredientCategory::VEGETABLES,
     .name = "Vegetables",
     .start_id = 0,
     .end_id = 22,
     .count = 22,
     .supplier_type = ActorType::VEGETABLE_SUPPLIER,
     .icon = Blitmap1Icon::TOMATOES},
    {.category = IngredientCategory::MEAT,
     .name = "Meat & Fish",
     .start_id = 24,
     .end_id = 41,
     .count = 17,
     .supplier_type = ActorType::MEAT_SUPPLIER,
     .icon = Blitmap1Icon::COW},
    {.category = IngredientCategory::FRUIT,
     .name = "Fruit",
     .start_id = 48,
     .end_id = 61,
     .count = 13,
     .supplier_type = ActorType::FRUIT_SUPPLIER,
     .icon = Blitmap1Icon::APPLES},
    {.category = IngredientCategory::CHEESE,
     .name = "Cheese & Spices",
     .start_id = 72,
     .end_id = 85,
     .count = 13,
     .supplier_type = ActorType::CHEESE_SUPPLIER,
     .icon = Blitmap1Icon::CHEESE},
}};

int CategoryRegistry::index_for(IngredientCategory category) {
  int idx = static_cast<int>(category);
  // Bounds check to prevent crashes
  if (idx < 0 || idx >= static_cast<int>(metadata_.size())) {
    return 0; // Default to VEGETABLES
  }
  return idx;
}

const CategoryMetadata& CategoryRegistry::get(IngredientCategory category) {
  return metadata_[index_for(category)];
}

int CategoryRegistry::start_id(IngredientCategory category) {
  return get(category).start_id;
}

int CategoryRegistry::end_id(IngredientCategory category) {
  return get(category).end_id;
}

int CategoryRegistry::count(IngredientCategory category) {
  return get(category).count;
}

std::string_view CategoryRegistry::name(IngredientCategory category) {
  return get(category).name;
}

ActorType CategoryRegistry::supplier_type(IngredientCategory category) {
  return get(category).supplier_type;
}

Blitmap1Icon CategoryRegistry::icon(IngredientCategory category) {
  return get(category).icon;
}

CategoryIdRange CategoryRegistry::range(IngredientCategory category) {
  const auto& meta = get(category);
  return {meta.start_id, meta.end_id};
}

std::pair<int, int> CategoryRegistry::id_range(IngredientCategory category) {
  const auto& meta = get(category);
  return {meta.start_id, meta.end_id};
}

std::optional<IngredientCategory> CategoryRegistry::from_ingredient_id(int id) {
  for (const auto& meta : metadata_) {
    if (id >= meta.start_id && id < meta.end_id) {
      return meta.category;
    }
  }
  return std::nullopt;
}

IngredientCategory CategoryRegistry::from_actor_type(ActorType type) {
  for (const auto& meta : metadata_) {
    if (meta.supplier_type == type) {
      return meta.category;
    }
  }
  return IngredientCategory::VEGETABLES; // fallback
}

} // namespace core::food
