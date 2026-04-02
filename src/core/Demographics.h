/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstdint>
#include <string_view>

enum class ConsumerType {
  Children = 0,
  YoungPeople,
  BlueCollar,
  WhiteCollar,
  BusinessPeople,
  SeniorCitizens,
  Count,
};

template <typename T> struct DemographicArray {
  std::array<T, static_cast<std::size_t>(ConsumerType::Count)> values{};

  T& operator[](ConsumerType d) { return values[static_cast<std::size_t>(d)]; }
  const T& operator[](ConsumerType d) const {
    return values[static_cast<std::size_t>(d)];
  }

  auto begin() { return values.begin(); }
  auto end() { return values.end(); }
  auto begin() const { return values.cbegin(); }
  auto end() const { return values.cend(); }

  std::array<T, static_cast<std::size_t>(ConsumerType::Count)>& data() {
    return values;
  }
  const std::array<T, static_cast<std::size_t>(ConsumerType::Count)>&
  data() const {
    return values;
  }
};

// Pizza popularity scores (-100 to +100)
using Popularity = DemographicArray<int>;
// Furniture appeal per demographic [0, 150]
using FurnitureAppeal = DemographicArray<int>;
// Menu demographic factor [1, 255]
using MenuFactor = DemographicArray<int>;
// Competitive strength per demographic [1, 250]
// Higher values = win more customers in competition with nearby restaurants
using CompetitiveStrength = DemographicArray<int>;
// Actual customer counts per demographic
using CustomerCounts = DemographicArray<int>;

inline constexpr std::array<ConsumerType,
                            static_cast<std::size_t>(ConsumerType::Count)>
    ALL_CONSUMER_TYPES = {
        ConsumerType::Children,       ConsumerType::YoungPeople,
        ConsumerType::BlueCollar,     ConsumerType::WhiteCollar,
        ConsumerType::BusinessPeople, ConsumerType::SeniorCitizens};

inline std::string_view to_string(ConsumerType d) {
  switch (d) {
  case ConsumerType::Children:
    return "Children";
  case ConsumerType::YoungPeople:
    return "Young People";
  case ConsumerType::BlueCollar:
    return "Blue Collar";
  case ConsumerType::WhiteCollar:
    return "White Collar";
  case ConsumerType::BusinessPeople:
    return "Business People";
  case ConsumerType::SeniorCitizens:
    return "Senior Citizens";
  default:
    return "Unknown";
  }
}
