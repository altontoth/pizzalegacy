/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/PlacedFurniture.h"
#include "engine/geom/Spaces.h"

constexpr int NUMBER_OF_ROTATIONS = 4;

PlacedFurniture::PlacedFurniture(const Furniture* furniture,
                                 core::FurnitureId id, WorldPoint pos,
                                 core::FurnitureId linked_furniture_id,
                                 int bob_state)
    : furniture_(furniture), id_(id), origin_(pos),
      linked_furniture_id_(linked_furniture_id.value == 0xFF
                               ? std::nullopt
                               : std::optional{linked_furniture_id}),
      bob_state_(bob_state) {}

WorldPoint PlacedFurniture::origin() const { return origin_; }

core::FurnitureId PlacedFurniture::id() const { return id_; }

void PlacedFurniture::id(core::FurnitureId id) { id_ = id; }

void PlacedFurniture::origin(WorldPoint p) { origin_ = p; }

void PlacedFurniture::rotate() {
  bob_state_ = (bob_state_ + 1) % NUMBER_OF_ROTATIONS;
}

std::optional<core::FurnitureId> PlacedFurniture::linked_furniture_id() const {
  return linked_furniture_id_;
}

void PlacedFurniture::linked_furniture_id(core::FurnitureId id) {
  linked_furniture_id_ = id;
}

int PlacedFurniture::bob_state() const { return bob_state_; }

void PlacedFurniture::bob_state(int bob_state) { bob_state_ = bob_state; }

const Furniture* PlacedFurniture::furniture() const { return furniture_; }

const std::vector<int>& PlacedFurniture::bobs() const {
  return furniture_->bobs();
}

void PlacedFurniture::rotate(Rotation r) {
  bob_state_ = static_cast<int>(r) % NUMBER_OF_ROTATIONS;
}
