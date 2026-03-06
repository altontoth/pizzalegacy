/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/Furniture.h"

Furniture::Furniture(
    FurnitureType type, int u1, int quality, int supplier,
    const std::string& name, int price, const std::vector<int>& bobs,
    FurnitureCategory cat, int u2, int capacity, int bake_time,
    PlacementLayer placement,
    const std::array<int, static_cast<std::size_t>(ConsumerType::Count)>&
        popularity)
    : type_(type), u1_(u1), quality_(quality), supplier_(supplier), name_(name),
      price_(price), bobs_(bobs), category_(cat), u2_(u2), capacity_(capacity),
      bake_time_(bake_time), placement_layer_(placement),
      popularity_(popularity) {

  if (bobs_.size() != 4) {
    throw std::invalid_argument("Bobs vector must contain exactly 4 elements.");
  }
}

int Furniture::popularity(ConsumerType consumer) const {
  if (consumer >= ConsumerType::Count) {
    throw std::out_of_range("Invalid consumer type.");
  }
  return popularity_[static_cast<std::size_t>(consumer)];
}

void Furniture::popularity(ConsumerType consumer, int value) {
  if (consumer >= ConsumerType::Count) {
    throw std::out_of_range("Invalid consumer type.");
  }
  popularity_[static_cast<std::size_t>(consumer)] = value;
}
