/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/Property.h"

#include "core/Demographics.h"
#include "core/city/City.h"
#include "core/city/District.h"
#include "core/inventory/Inventory.h"
#include "core/restaurant/Restaurant.h"
#include "core/util/Constants.h"
#include "engine/Logger.h"

#include <algorithm>
#include <cmath>
#include <iostream>

using namespace core;

std::ostream& operator<<(std::ostream& os, const Property& premise) {
  os << "Property(width: " << static_cast<int>(premise.size().w)
     << ", height: " << static_cast<int>(premise.size().h)
     << ", realtor_id: " << static_cast<int>(premise.realtor_id())
     << ", status: " << static_cast<int>(premise.status())
     << ", city: " << premise.city().name() << ")";
  return os;
}

const uint8_t residential_pop_table[8][6] = {
    // Church:  Child, Young, Blue, White, Bus., Senior
    {2, 2, 2, 2, 2, 12},
    // Bank
    {1, 3, 5, 12, 18, 6},
    // Cinema
    {12, 20, 10, 5, 2, 1},
    // School
    {28, 10, 2, 3, 1, 1},
    // Disco
    {3, 25, 10, 3, 2, 0},
    // Bar
    {1, 3, 20, 5, 3, 5},
    // Department store
    {4, 8, 9, 19, 10, 5},
    // Hotel
    {1, 1, 2, 5, 25, 6}};

Property::Property(int index, PropertyType type, Size size, int realtor_id,
                   PremiseStatus status, City& city)
    : index_(index), type_(type), size_(size), realtor_id_(realtor_id),
      status_(status), city_(city) {}

Size Property::size() const { return size_; }
void Property::size(Size size) { size_ = size; }
int Property::realtor_id() const { return realtor_id_; }
void Property::realtor_id(int realtor_id) { realtor_id_ = realtor_id; }
PremiseStatus Property::status() const { return status_; }
void Property::status(PremiseStatus status) { status_ = status; }

City& Property::city() { return city_; }
const City& Property::city() const { return city_; }

int Property::index() const { return index_; }
PropertyType Property::type() const { return type_; }
std::optional<core::PlayerId> Property::owner_or_tenant() const {
  return owner_tenant_;
}
void Property::owner_or_tenant(core::PlayerId player) {
  owner_tenant_ = player;
  if (type_ == PropertyType::DEPOT) {
    weapons_.emplace(inventory::Inventory());
  }
}

void Property::clear_owner() {
  owner_tenant_.reset();
  weapons_.reset();
}

bool Property::is_for_sale() const {
  return status_ == PremiseStatus::FOR_SALE ||
         status_ == PremiseStatus::FOR_SALE_RENT;
}

bool Property::is_for_rent() const {
  return status_ == PremiseStatus::FOR_RENT ||
         status_ == PremiseStatus::FOR_SALE_RENT;
}

bool Property::is_owned() const { return status_ == PremiseStatus::OWNED; }

bool Property::is_rented() const { return status_ == PremiseStatus::RENTED; }

void Property::rent(core::PlayerId player_id) {
  status_ = PremiseStatus::RENTED;
  owner_or_tenant(player_id);
}

void Property::buy(core::PlayerId player_id) {
  status_ = PremiseStatus::OWNED;
  owner_or_tenant(player_id);
}

void Property::calculate_demographics(const Restaurant* restaurant) {
  // Get property coordinates
  int prop_x = City::index_to_x(index_);
  int prop_y = City::index_to_y(index_);

  // Calculate radius of influence and edge probability
  //
  // Edge probability handles fractional radius coverage. Since radius is an
  // integer (sqrt of range_raw), tiles exactly at that radius may be partially
  // within the true circular range. Instead of hard cutoff, we
  // probabilistically include edge tiles based on how much range_raw exceeds
  // the integer radius^2.
  //
  // Example: range_raw=60, radius=7 (since sqrt(60)=~7.7, truncated to 7)
  //   - Tiles at distance <7: always included
  //   - Tiles at distance =7: included with probability based on the 0.7
  //   fraction
  //   - Tiles at distance >7: never included
  //
  // The denominator (2*radius+1) represents how many "units" of range_raw span
  // from radius^2 to (radius+1)^2. The numerator is how far into that span we
  // are.
  int radius = 7;
  int edge_probability = 0;

  if (restaurant) {
    // range() returns the tile radius (already sqrt of range_raw)
    // range_raw() returns the raw value (0-200)
    radius = restaurant->range();
    int range_raw = restaurant->range_raw();

    int radius_squared = radius * radius;
    int radius_plus_1_squared = (radius + 1) * (radius + 1);
    int denominator = radius_plus_1_squared - radius_squared;

    if (denominator > 0) {
      edge_probability = ((range_raw - radius_squared) * 100) / denominator;
    }
  }

  // Calculate search bounds (bounding box)
  int min_x = std::max(0, prop_x - radius);
  int max_x = std::min(MAP_TILES_PER_ROW - 1, prop_x + radius);
  int min_y = std::max(0, prop_y - radius);
  int max_y = std::min(MAP_TILES_PER_COLUMN - 1, prop_y + radius);

  // Pre-calculate base multipliers for all demographics
  auto property_district_id = city_.district_by_index(index_);
  const auto& property_district = city_.district(property_district_id);
  const auto& demographic_demand = property_district.demographic_demand();

  DemographicArray<float> base_multipliers{};
  for (auto demo : ALL_CONSUMER_TYPES) {
    base_multipliers[demo] =
        demographic_demand.at(static_cast<int>(demo)) * 0.5f;
  }

  // Accumulate population per demographic
  DemographicArray<float> demo_populations{};

  // Scan surrounding tiles, calculate all demographics per tile
  for (int y = min_y; y <= max_y; y++) {
    for (int x = min_x; x <= max_x; x++) {
      int manhattan_dist = abs(prop_x - x) + abs(prop_y - y);

      if (manhattan_dist > radius) {
        continue;
      }

      // For tiles exactly at radius, use edge probability
      if (manhattan_dist == radius) {
        if (!engine::random::rand_percentage_chance(edge_probability)) {
          continue;
        }
      }

      int tile_index = City::x_y_to_index(x, y);
      uint8_t tile_type = city_.type(tile_index);

      // Calculate contribution for all demographics
      for (auto demographic : ALL_CONSUMER_TYPES) {
        float contribution =
            calculate_tile_population(tile_type, static_cast<int>(demographic),
                                      base_multipliers[demographic]);
        demo_populations[demographic] += contribution;
      }
    }
  }

  float total_pop = 0.0f;
  for (auto demo : ALL_CONSUMER_TYPES) {
    potential_customers_[demo] =
        static_cast<int>(std::round(demo_populations[demo]));
    total_pop += demo_populations[demo];
  }
  total_population_ = static_cast<int>(std::round(total_pop));
}

float Property::calculate_tile_population(uint8_t tile_type,
                                          int demographic_type,
                                          float district_percentage) {
  // Tile type encodes position in landsym tileset (building graphics):
  // - landsym_row (lower 4 bits): row in tileset
  // - landsym_col (upper 4 bits): column in tileset
  // TODO: Should we use some method in City to do this for us rather than
  // hardcode it here as well?
  uint8_t landsym_row = tile_type & 0x0F;
  uint8_t landsym_col = (tile_type >> 4) & 0x0F;

  // Named buildings (Church, Bank, etc
  // These are tiles 0x88-0xF8 (landsym_row=0x8, landsym_col 8-15)
  if (landsym_row == 0x8 && landsym_col >= 8) {
    int building_index = std::min(landsym_col - 8, 7);

    uint8_t population =
        residential_pop_table[building_index][demographic_type];
    return district_percentage * static_cast<float>(population);
  }

  // Other tiles use simple multipliers
  switch (landsym_row) {
  case 0x0: // nature
  case 0x1:
  case 0x2:
  case 0x3: // water
  case 0x4: // building
  case 0x5: // path
    return district_percentage * 1.0f;

  case 0x6: // road
  case 0x7: // road
    return district_percentage * 2.0f;

  case 0x9: // villa
    return district_percentage * 2.0f;

  case 0x0A: // business premise (but col < 8)
    return district_percentage * 4.0f;

  case 0x0B: // residential
  default:
    return district_percentage * 2.0f;
  }
}

Money Property::get_purchase_price() const {
  auto district_index = city_.district_by_index(index_);
  const auto& district_ptr = city_.district(district_index);
  Logger::debug("Price lvel is {} and wxh is {}", district_ptr.price_level(),
                size_);

  int base_price = district_ptr.price_level() * size_.w * size_.h * 100;

  if (type_ == PropertyType::DEPOT) {
    base_price *= 5;
  }

  return Money{base_price};
}

Money Property::get_monthly_rent() const {
  auto district_index = city_.district_by_index(index_);
  const auto& district_ptr = city_.district(district_index);

  int base_rent = district_ptr.rent_level() * static_cast<int>(size_.w) *
                  static_cast<int>(size_.h) * 100;

  // depot costs 5x the rent
  if (type_ != PropertyType::BUSINESS_PREMISE) {
    base_rent *= 5;
  }

  return Money{base_rent};
}

void Property::restaurant_id(core::RestaurantId r_id) { restaurant_id_ = r_id; }

void Property::clear_restaurant() { restaurant_id_.reset(); }

std::optional<core::RestaurantId> Property::restaurant_id() const {
  return restaurant_id_;
}

const District& Property::district() const {
  auto district_index = city_.district_by_index(index_);
  return city_.district(district_index);
}
