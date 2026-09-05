/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/food/Topping.h"
#include <iostream>
#include "core/factories/IngredientFactory.h"
#include "engine/Logger.h"
#include "engine/geom/Spaces.h"

static constexpr uint8_t rotation_mask = 0x03; // 0b0011
static constexpr uint8_t chop_shift = 2;       // top bits encode chop
static constexpr uint8_t max_chop = 2;         // 0=whole, 1=chopped, 2=minced

namespace core::food {

std::ostream& operator<<(std::ostream& os, const Topping& topping) {
  os << "  ingredient name =" << topping.ingredient_->name() << "\n";
  os << "  weight =" << topping.weight() << " gram\n";
  return os;
}

Topping::Topping(IngredientType ingredient_type)
    : origin_(WorldPoint{0, 0}), ingredient_type_(ingredient_type), state_(0) {
  ingredient_ =
      IngredientFactory::create_ingredient(static_cast<int>(ingredient_type));
  // Logger::debug("Creating {} topping", ingredient_->name().c_str());
  //  reload_image();
}

/* Legacy state_
 *
 * This is the state_ as it can be found in the recipe files in the original
 * game. It looks like this:
 * 0 = whole, rotation 1
 * 1 = whole, rotation 2
 * 2 = whole, rotation 3
 * 3 = whole, rotation 4
 * 4 = chopped, rotation 1
 * 5 = chopped, rotation 2
 * 6 = chopped, rotation 3
 * 7 = chopped, rotation 4
 * 8 = minced, rotation 1
 * 9 = minced, rotation 2
 * 10 = minced, rotation 3
 * 11 = minced, rotation 4
 *
 * So it's really two pieces of information, first the four bits that tell us
 * the chopped state, and the last four bits that tell us the rotation state_.
 * However for compatibility with the bob file I'm keeping it in one
 * uint8_t just like in the original, with helper functions to get individual
 * states.
 */
Topping::Topping(int ingredient_id, int state, WorldPoint pos)
    : origin_(pos),
      ingredient_type_(static_cast<IngredientType>(ingredient_id)),
      state_(state) {
  ingredient_ = IngredientFactory::create_ingredient(ingredient_id);
  // Logger::debug("Creating {} topping", ingredient_->name().c_str());
  // reload_image();
}

Topping::Topping(const Topping& other) {
  ingredient_type_ = other.ingredient_type_;
  state_ = other.state_;
  origin_ = other.origin_;

  ingredient_ = IngredientFactory::create_ingredient(
      static_cast<int>(other.ingredient_type_));
}

void Topping::rotate() {
  uint8_t chop = static_cast<uint8_t>(state_ >> chop_shift);
  uint8_t rot = state_ & rotation_mask;
  rot = static_cast<uint8_t>((rot + 1) & rotation_mask); // wrap 0..3
  state_ = static_cast<uint8_t>((chop << chop_shift) | rot);
}

bool Topping::chop() {
  uint8_t chop = static_cast<uint8_t>(state_ >> chop_shift);
  if (chop >= max_chop) {
    return false; // already minced
  }
  uint8_t rot = state_ & rotation_mask;
  ++chop; // advance chop, keep rot
  state_ = static_cast<uint8_t>((chop << chop_shift) | rot);
  return true;
}

Ingredient* Topping::ingredient() const { return ingredient_; }

WorldPoint Topping::origin() const { return origin_; }

void Topping::place(WorldPoint p) { origin_ = p; }

float Topping::calories() const {
  return static_cast<float>(weight() * ingredient_->calories_per_100g()) /
         100.0f;
}

int Topping::weight() const {
  switch (state_ >> chop_shift) { // switch on CHOP LEVEL
  case 0:
    return ingredient_->weight_large(); // whole
  case 1:
    return ingredient_->weight_medium(); // chopped
  case 2:
    return ingredient_->weight_small(); // minced
  default:
    Logger::warn("Topping chop level out of range: {}", state_ >> chop_shift);
    return 0;
  }
}

const std::vector<int>& Topping::bobs() const { return ingredient_->bobs(); }

int Topping::bob_state() const { return state_; }

int Topping::chopped_state() const { return state_ >> 2; }

int Topping::rotation_state() const { return state_ & 0x03; }

} // namespace core::food
