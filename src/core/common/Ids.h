/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <ostream>

#include <cstdint>
#include <functional>
template <class Tag, class Under = std::int32_t> struct StrongId {
  Under value{};
  friend constexpr bool operator==(StrongId a, StrongId b) {
    return a.value == b.value;
  }
  friend constexpr bool operator!=(StrongId a, StrongId b) { return !(a == b); }
  friend constexpr bool operator<(StrongId a, StrongId b) {
    return (a.value < b.value);
  }

  StrongId& operator++() { // prefix
    ++value;
    return *this;
  }
  StrongId operator++(int) { // postfix
    StrongId tmp = *this;
    ++value;
    return tmp;
  }

  // optional: hash support for unordered_map
  struct Hasher {
    std::size_t operator()(StrongId id) const noexcept {
      return std::hash<Under>{}(id.value);
    }
  };
};

struct PlayerTag {};
struct RestaurantTag {};
struct CityTag {};
struct BankTag {};
struct RecipeTag {};
struct FurnitureTag {};
struct DistrictTag {};

namespace core {
using PlayerId = StrongId<PlayerTag>;
using RestaurantId = StrongId<RestaurantTag>;
using CityId = StrongId<CityTag>;
using BankId = StrongId<BankTag>;
using RecipeId = StrongId<RecipeTag>;
using FurnitureId = StrongId<FurnitureTag>;
using DistrictId = StrongId<DistrictTag>;
} // namespace core

template <class Tag, class Under>
inline std::ostream& operator<<(std::ostream& os, StrongId<Tag, Under> id) {
  return os << id.value;
}

namespace std {

template <class Tag, class Under> struct hash<StrongId<Tag, Under>> {
  std::size_t operator()(StrongId<Tag, Under> id) const noexcept {
    return std::hash<Under>{}(id.value);
  }
};

} // namespace std
