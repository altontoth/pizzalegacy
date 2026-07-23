/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <optional>

#include "core/food/Ingredient.h"

namespace core::newspaper {

class NewspaperState {
public:
  // Article ID encoding
  //   0        = no article
  //   1-50     = flavor text from ARTIKEL0.E
  //   51-100   = event article from ARTIKEL1.E (subtract EVENT_OFFSET to get
  //   index) 0xC8     = ingredient template from ARTIKEL3.E
  static constexpr uint8_t INGREDIENT_ARTICLE = 0xC8;
  static constexpr uint8_t EVENT_OFFSET = 50;
  static constexpr int EVENT_COOLDOWN = 4;
  static constexpr int FALLBACK_COOLDOWN = 10;

  // Inclusive id range of the flavor articles in ARTIKEL0.E
  static constexpr uint8_t FLAVOR_ARTICLE_FIRST = 1;
  static constexpr uint8_t FLAVOR_ARTICLE_LAST = 50;

  void mark_as_read() { cooldown = 0; }

  void clear() {
    article_id = 0;
    polarity = 0;
    category = 0;
    target_ingredient = std::nullopt;
  }

  bool is_event_article() const {
    return article_id > EVENT_OFFSET && article_id <= EVENT_OFFSET * 2;
  }

  bool is_ingredient_article() const {
    return article_id == INGREDIENT_ARTICLE;
  }

  // For event articles (51-100), returns the 1-50 index into ARTIKEL1.E
  uint8_t event_article_index() const { return article_id - EVENT_OFFSET; }

  // Display state
  int cooldown{0};
  uint8_t article_id{0};
  int8_t polarity{0};
  uint8_t category{0};
  std::optional<core::food::IngredientType> target_ingredient;
};

} // namespace core::newspaper
