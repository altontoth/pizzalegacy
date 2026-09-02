/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/District.h"

#include <cassert>

District::District(int index, std::string name)
    : index_(core::DistrictIndex{index}), name_(name),
      population_percentages_({0, 0, 0, 0, 0, 0}) {}

void District::id(uint8_t id) { id_ = id; }

void District::control_screen_alternate_color(bool setting) {
  control_screen_alternate_color_ = setting;
}

uint8_t District::bez_id() {
  int offset = 0x80 + ((control_screen_alternate_color_ ? 1 : 0) * 0x10);
  return static_cast<uint8_t>(index_.value + offset);
}

void District::district_type(DistrictType district_type) {
  district_type_ = district_type;
}

DistrictType District::district_type() const { return district_type_; }

int District::average_income_factor() const { return average_income_factor_; }

int District::average_income() const {
  return 9000 - (average_income_factor_ * 80);
}

void District::average_income_factor(int average_income_factor) {
  average_income_factor_ = average_income_factor;
}

void District::color(uint8_t color) { color_ = color; }

uint8_t District::color() const { return color_; }

std::string District::name() const { return name_; }

int District::average_size() const { return average_size_; }

void District::average_size(int average_size) { average_size_ = average_size; }

int District::average_square_meters() const {
  float half_size = (average_size_ / 2.0f);
  return static_cast<int>(half_size * half_size);
}

void District::premises_availability(int prem_av) {
  premises_availability_ = prem_av;
}

int District::premises_availability() const { return premises_availability_; }

void District::population_percentages(
    std::array<uint8_t, static_cast<std::size_t>(PopulationType::count)>
        pop_perc) {
  population_percentages_ = pop_perc;
}

const std::array<uint8_t, static_cast<std::size_t>(PopulationType::count)>&
District::population_percentages() const {
  return population_percentages_;
}

void District::demographic_demand(
    std::array<float, static_cast<std::size_t>(PopulationType::count)> demand) {
  demographic_demand_ = demand;
}

const std::array<float, static_cast<std::size_t>(PopulationType::count)>&
District::demographic_demand() const {
  return demographic_demand_;
}

bool District::control_screen_alternate_color() const {
  return control_screen_alternate_color_;
};

RangeDescriptor District::range_discriptor(int value, int min, int max) const {
  int range = (max - min) / static_cast<int>(RangeDescriptor::out_of_range);
  int index = (value - min) / range;

  if (index < 0 || index >= static_cast<int>(RangeDescriptor::out_of_range)) {
    return RangeDescriptor::out_of_range;
  }
  return static_cast<RangeDescriptor>(index);
}

void District::crime_statistics(int crime_statistics) {
  crime_statistics_ = crime_statistics;
}

void District::traffic_density(int traffic_density) {
  traffic_density_ = traffic_density;
}

int District::crime_statistics() const { return crime_statistics_; }

int District::traffic_density() const { return traffic_density_; }

void District::price_level(int price_level) { price_level_ = price_level; }

int District::price_level() const { return price_level_; }

void District::rent_level(int rent_level) { rent_level_ = rent_level; }

int District::rent_level() const { return rent_level_; }

void District::city(City* city) { city_ = city; }

const City* District::city() const {
  assert(city_);
  return city_;
}

int District::average_size_modifier() const { return average_size_modifier_; }

void District::average_size_modifier(int average_size_modifier) {
  average_size_modifier_ = average_size_modifier;
}

void District::demographic_factor(uint8_t factor) {
  demographic_factor_ = factor;
}

uint8_t District::demographic_factor() const { return demographic_factor_; }

void District::demographic_base(uint8_t base) { demographic_base_ = base; }

uint8_t District::demographic_base() const { return demographic_base_; }

// Popularity calculation accessors
void District::service_quality_factor(uint8_t factor) {
  service_quality_factor_ = factor;
}

uint8_t District::service_quality_factor() const {
  return service_quality_factor_;
}

void District::minimum_desired_canonical_pizza_count(uint8_t count) {
  minimum_desired_canonical_pizza_count_ = count;
}

void District::minimum_desired_user_pizza_count(uint8_t tolerance) {
  minimum_desired_user_pizza_count_ = tolerance;
}

uint8_t District::minimum_desired_canonical_pizza_count() const {
  return minimum_desired_canonical_pizza_count_;
}

uint8_t District::minimum_desired_user_pizza_count() const {
  return minimum_desired_user_pizza_count_;
}
