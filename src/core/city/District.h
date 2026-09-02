/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include "core/common/Ids.h"

class City;

enum class DistrictType {
  unknown = 0,
  residential,
  industrial,
  business,
  leisure,
  shopping,
  traffic,
  count,
};
enum class PopulationType {
  children = 0,
  young_people,
  blue_collar,
  white_collar,
  business_people,
  senior_citizens,
  count,
};

enum class RangeDescriptor {
  low,
  very_little,
  little,
  average,
  above_average,
  high,
  very_high,
  extremely_high,
  out_of_range,
};

class District {
public:
  District(int index, std::string name);
  uint8_t bez_id();
  void id(uint8_t);
  // The id the city file gives this district: mostly its index plus one, but a
  // couple of districts have a random different value for some reason.
  uint8_t id() const { return id_; }
  void district_type(DistrictType);
  DistrictType district_type() const;
  void population_percentages(
      std::array<uint8_t, static_cast<std::size_t>(PopulationType::count)>);
  const std::array<uint8_t, static_cast<std::size_t>(PopulationType::count)>&
  population_percentages() const;
  void demographic_demand(
      std::array<float, static_cast<std::size_t>(PopulationType::count)>);
  const std::array<float, static_cast<std::size_t>(PopulationType::count)>&
  demographic_demand() const;
  void average_income_factor(int);
  int average_income_factor() const;
  int average_income() const;
  void color(uint8_t);
  uint8_t color() const;
  std::string name() const;
  // Position in the city's district list, which is how the map's tiles and
  // every lookup name a district.
  core::DistrictIndex index() const { return index_; }
  void average_size(int);
  void average_size_modifier(int);
  int average_size() const;
  int average_size_modifier() const;
  int average_square_meters() const;
  void premises_availability(int);
  int premises_availability() const;
  void control_screen_alternate_color(bool);
  bool control_screen_alternate_color() const;
  RangeDescriptor range_discriptor(int value, int min, int max) const;
  void crime_statistics(int);
  void traffic_density(int);
  int crime_statistics() const;
  int traffic_density() const;
  void price_level(int price_level);
  int price_level() const;
  void rent_level(int rent_level);
  int rent_level() const;
  void city(City*);
  const City* city() const;

  // Demand calculation fields
  void demographic_factor(uint8_t factor);
  uint8_t demographic_factor() const;
  void demographic_base(uint8_t base);
  uint8_t demographic_base() const;

  // Popularity calculation fields
  void service_quality_factor(uint8_t factor);
  uint8_t service_quality_factor() const;
  void minimum_desired_canonical_pizza_count(uint8_t count);
  void minimum_desired_user_pizza_count(uint8_t tolerance);
  uint8_t minimum_desired_canonical_pizza_count() const;
  uint8_t minimum_desired_user_pizza_count() const;

private:
  core::DistrictIndex index_;
  uint8_t id_{0};
  std::string name_;
  bool control_screen_alternate_color_ = false;
  std::array<uint8_t, static_cast<std::size_t>(PopulationType::count)>
      population_percentages_;
  // Raw unnormalized demographic demand values (for customer calculations)
  std::array<float, static_cast<std::size_t>(PopulationType::count)>
      demographic_demand_{};
  uint16_t average_income_factor_{0};
  DistrictType district_type_{DistrictType::unknown};
  int traffic_density_{0};
  int crime_statistics_{0};
  uint8_t color_{0};
  int average_size_{-1};
  int average_size_modifier_{-1};
  int premises_availability_{-1};
  int rent_level_{0};
  int price_level_{0};
  City* city_{nullptr};

  // Demand calculation fields
  uint8_t demographic_factor_{0};
  uint8_t demographic_base_{0};

  // Popularity calculation fields
  uint8_t service_quality_factor_{0};
  uint8_t minimum_desired_canonical_pizza_count_{0};
  uint8_t minimum_desired_user_pizza_count_{0};
};
