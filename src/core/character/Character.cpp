/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/character/Character.h"

#include <cassert>

namespace core::character {

Character::Character(CharacterData* data)
    : data_(data), age_(data_->age), health_(data_->health),
      energy_(data_->energy), experience_(data_->experience),
      cooking_(data_->cooking), serving_(data_->serving),
      management_(data_->management), motivation_(data_->motivation),
      trustworthiness_(data_->trustworthiness), honesty_(data_->honesty),
      pizzas_per_day_(data_->pizzas_per_day) {}

int Character::picture_width() const { return PICTURE_WIDTH; }

int Character::picture_height() const { return PICTURE_HEIGHT; }

std::span<const uint8_t> Character::picture_data() const {
  return data_->picture_data();
}

uint8_t Character::palette_index() const { return data_->palette_index(); }

CharacterData Character::to_character_data() const {
  // Copy the immutable template
  CharacterData snapshot = *data_;

  // Override with current mutable runtime values
  snapshot.age = age_;
  snapshot.health = health_;
  snapshot.energy = energy_;
  snapshot.experience = experience_;
  snapshot.cooking = cooking_;
  snapshot.serving = serving_;
  snapshot.management = management_;
  snapshot.motivation = motivation_;
  snapshot.trustworthiness = trustworthiness_;
  snapshot.honesty = honesty_;
  snapshot.pizzas_per_day = pizzas_per_day_;

  return snapshot;
}

} // namespace core::character
