/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "core/restaurant/Furniture.h"
#include "engine/BobRenderable.h"
#include "engine/geom/Spaces.h"

constexpr uint8_t UNLINKED_FURNITURE = 0xFF;

enum class Rotation {
  SOUTH = 240,
  WEST,
  NORTH,
  EAST,
};

class PlacedFurniture : public BobRenderable {
public:
  PlacedFurniture(const Furniture* furniture, core::FurnitureId id,
                  WorldPoint p, core::FurnitureId linked_furniture,
                  int bob_state);

  // Getters and setters for origin and rotation
  WorldPoint origin() const;
  void origin(WorldPoint p);

  int bob_state() const;
  void bob_state(int bob_state);

  // Access to the associated Furniture object
  const Furniture* furniture() const;
  const std::vector<int>& bobs() const;
  core::FurnitureId id() const;
  void id(core::FurnitureId id);
  std::optional<core::FurnitureId> linked_furniture_id() const;
  void linked_furniture_id(core::FurnitureId id);
  void rotate();
  void rotate(Rotation r);

private:
  const Furniture* furniture_;
  core::FurnitureId id_;
  WorldPoint origin_;
  std::optional<core::FurnitureId> linked_furniture_id_;
  int bob_state_;
};
