
/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <array>
#include <cstddef>
#include <string_view>

namespace core::item {
enum class Item : std::size_t {
  SmellyCheese,
  Laxatives,
  Rats,
  StinkBomb,
  FlameThrower, // Vanilla
  Chemicals,    // Chocolate
  Bazooka,      // Strawberry
  Bomb,         // Lemon,
  Count,
};
constexpr std::size_t item_count = static_cast<std::size_t>(Item::Count);

constexpr std::array<Item, 8> weapon_list = {
    Item::SmellyCheese, Item::Laxatives, Item::Rats,    Item::StinkBomb,
    Item::FlameThrower, Item::Chemicals, Item::Bazooka, Item::Bomb,
};

constexpr std::array<Item, 4> sabotage_list = {
    Item::SmellyCheese,
    Item::Laxatives,
    Item::Rats,
    Item::StinkBomb,
};

constexpr std::string_view to_string(Item i) noexcept {
  switch (i) {
  case Item::SmellyCheese:
    return "Smelly Cheese";
  case Item::Laxatives:
    return "Laxatives";
  case Item::Rats:
    return "Rats";
  case Item::StinkBomb:
    return "StinkBomb";
  case Item::FlameThrower:
    return "Flamethrower";
  case Item::Chemicals:
    return "Chemicals";
  case Item::Bazooka:
    return "Bazooka";
  case Item::Bomb:
    return "Bomb";
  default:
    return "Unknown";
  }
}

} // namespace core::item
