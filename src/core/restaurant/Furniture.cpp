/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/Furniture.h"

Furniture::Furniture(const FurnitureData& d)
    : type_(d.type), u1_(d.u1), quality_(d.quality), supplier_(d.supplier),
      name_(d.name), price_(d.price), bobs_(d.bobs.begin(), d.bobs.end()),
      category_(d.category), u2_(d.u2), capacity_(d.capacity),
      bake_time_(d.bake_time), placement_layer_(d.placement_layer),
      role_(d.role), popularity_(d.popularity) {}

int Furniture::popularity(ConsumerType consumer) const {
  if (consumer >= ConsumerType::Count) {
    throw std::out_of_range("Invalid consumer type.");
  }
  return popularity_[static_cast<std::size_t>(consumer)];
}
