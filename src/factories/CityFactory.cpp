/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "factories/CityFactory.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>

#include "core/city/District.h"
#include "core/city/Property.h"
#include "engine/Fatal.h"
#include "engine/Logger.h"
#include "resources/util/FileUtils.h"
#include "resources/util/Platform.h"
#include "resources/util/TextEncoding.h"

using namespace core;

#pragma pack(push, 1)
struct DistrictDataRaw {
  uint8_t traffic_intensity;
  uint8_t unknown_1;
  char name[24];
  uint8_t unknown_2[4];
  uint8_t control_screen_alternate_color;
  uint8_t unknown_10a[9];
  uint8_t demographic_something;
  uint8_t unknown_10b[7];
  uint8_t demographic_factor;
  uint8_t crime_stats;
  uint8_t demographic_base;
  uint8_t average_income_factor;
  uint8_t service_speed_sensitivity;
  uint8_t unused_1a;
  uint8_t minimum_desired_canonical_pizza_count;
  uint8_t minimum_desired_user_pizza_count;
  uint8_t unknown_4c;
  uint8_t district_type;
  uint8_t average_size;
  uint8_t unknown_5;
  uint8_t average_size_modifer;
  uint8_t unknown_9;
  uint16_t price_level_raw;
  uint16_t rent_level_raw;
  uint8_t unknown_6[4];
  uint8_t premises_availability;
  uint8_t unknown_7;
  float population_data[6];
  uint8_t unknown_8[8];
};
#pragma pack(pop)

static_assert(sizeof(DistrictDataRaw) == 104,
              "DistrictDataRaw should be 104 bytes");

std::unique_ptr<City> CityFactory::create_city(core::CityId cid) {
  CityFactory factory;
  factory.e = platform::is_amiga() ? Endian::Big : Endian::Little;
  auto id = cid.value;
  factory.load_color_definition();
  factory.load_city_data(id);
  factory.load_district_bez(id);
  factory.load_map_data(id);
  factory.populate_types();

  auto city = std::make_unique<City>(core::CityId{id});
  factory.populate_properties(*city);
  factory.build_city_object(*city, id);
  return city;
}

void CityFactory::load_color_definition() {
  district_color_list_ = {0x0F, 0x09, 0x1B, 0x05, 0x16, 0x14,
                          0x18, 0x11, 0x01, 0x03, 0x13, 0x0B};
}

void CityFactory::load_map_data(int city_id) {
  char filename[15];
  snprintf(filename, sizeof(filename),
           platform::is_amiga() ? "map%d.dat" : "dat/map%d.dat", city_id);
  std::string path(filename);
  std::ifstream is = fileutils::open(path);

  if (!is.good()) {
    std::cerr << "Failed to open map data file: " << path << "\n";
    return;
  }

  is.seekg(0, std::ios::end);
  data_length_ = (int)is.tellg();
  is.seekg(0, std::ios::beg);

  map_data_.resize(data_length_, 0);
  std::array<uint8_t, NUMBER_OF_NAMED_BUILDINGS> named_building_count{};
  for (auto& count : named_building_count) {
    count = 0;
  }

  for (int i = 0; i < data_length_ && is.good(); ++i) {
    unsigned char c = (unsigned char)is.get();
    map_data_[i] = c;

    if (City::is_named_building(c)) {
      unsigned char building_type = ((c & 0xF0) >> 4) - 8;
      int building_name_index = named_building_count[building_type];
      if (++named_building_count[building_type] >
          (uint8_t)building_name_count_[building_type]) {
        building_name_index = named_building_count[building_type] = 0;
      }
      named_buildings_[i] = building_names_[building_type][building_name_index];
    }
  }

  is.close();
}

void CityFactory::load_city_data(int city_id) {
  char filename[16];
  snprintf(filename, sizeof(filename),
           platform::is_amiga() ? "city%d.dat" : "dat/city%d.pc", city_id);
  std::string path(filename);
  std::ifstream is = fileutils::open(path);

  is.seekg(8);
  uint8_t district_count = (uint8_t)is.get();
  uint8_t supplier_count = (uint8_t)is.get();
  supplier_count_ = supplier_count;

  is.seekg(6, std::ios::cur);

  constexpr int city_name_length = 24;
  city_name_ = text::read_iso8859_1_string(is, city_name_length);

  is.seekg(40);

  // We need this in the business premise area so it's a member
  number_of_realtors_ = (uint8_t)is.get();
  auto number_of_banks = (uint8_t)is.get();
  auto number_of_insurance = (uint8_t)is.get();
  auto number_of_authorities = (uint8_t)is.get();
  auto number_of_others = (uint8_t)is.get();

  uint8_t unknown1 = is.get();
  // Logger::debug("Unknown byte: {}", unknown1);
  is.read((char*)heads_.data(), 50);

  is.ignore(10);

  parse_phone_numbers(is);

  // We're skipping ahead 40 0 bytes here:
  is.ignore(40);

  parse_contacts(is, number_of_realtors_, number_of_banks, number_of_insurance,
                 number_of_authorities, number_of_others);

  read_realtor_data(is, number_of_realtors_);
  read_bank_data(is, number_of_banks);
  read_insurance_data(is, number_of_insurance);
  read_mafia_data(is, number_of_others);

  // Skip 240 zero bytes.
  is.ignore(240);
  is.ignore(15);
  is.ignore(25);
  is.ignore(19);
  is.ignore(21);
  is.ignore(23);
  is.ignore(57);
  is.ignore(19);
  is.ignore(61);
  uint8_t unknown_again = is.get();
  is.ignore(1);
  is.ignore(168);
  is.ignore(352); // All zero again

  auto pos = is.tellg();

  load_districts(is, district_count);

  load_suppliers(is, supplier_count_);

  load_building_names(is);

  load_ingredient_prices(is);

  is.close();
}

void CityFactory::load_building_names(std::ifstream& is) {
  for (int i = 0; i < NUMBER_OF_NAMED_BUILDINGS; ++i) {
    building_name_count_[i] = 0;
    for (int j = 0; j < MAX_BUILDING_NAMES_PER_TYPE; j++) {
      auto name = text::read_iso8859_1_string(is, 16);
      if (name.empty()) {
        continue;
      }
      building_names_[i][j] = name;
      building_name_count_[i]++;
    }
  }
}

void CityFactory::load_ingredient_prices(std::ifstream& is) {
  is.seekg(2026);

  for (int i = 0; i < 90; ++i) {
    uint8_t low = is.get();
    uint8_t high = is.get();
    uint16_t price = math::two_bytes_to_uint16(low, high);
    ingredient_base_prices_[i] = Money{static_cast<int64_t>(price)};
    // Logger::debug("base price for ingredient index {} is {}", i, price);
  }
}

void CityFactory::load_suppliers(std::ifstream& is, int supplier_count) {
  // Logger::debug("Supplier count is {}", supplier_count);
  suppliers_.reserve(supplier_count);

  for (int i = 0; i < supplier_count; ++i) {
    if (i > 0) {
      is.seekg(10, std::ios_base::cur);
    }

    // The next 308 bytes (a guess, most are 319 but last one is 308, padding?)
    // are a supplier.

    // First, the category of this supplier:
    auto type = (uint8_t)is.get(); // 0 = veg, 1 = meat, 2 = fruit, 3 = cheese
    // Logger::debug("Type is {} ", (int)type);

    // Next, the quality of this supplier:
    uint8_t quality = (uint8_t)is.get();
    // Logger::debug("Quality is {}", (int)quality);

    is.seekg(2, std::ios_base::cur);

    // Now, the name of the supplier, I'm guessing 26 bytes this time:
    auto supplier_name = text::read_iso8859_1_string(is, 26);

    char c1 = is.get();
    char c2 = is.get();

    int minimum_contract_amount = math::two_bytes_to_uint16(c1, c2);

    is.seekg(201, std::ios_base::cur);

    uint8_t head_type = (uint8_t)is.get() + 1;

    is.seekg(5, std::ios_base::cur);

    // But then there is an array that specifies what products are available.
    // Length of the array depends on the type (22 for cheese, more for
    // vegetables, etc) Since 22 is the max, we just read that and ignore the
    // rest.
    std::array<uint8_t, 22> price_levels{};
    is.read((char*)price_levels.data(), 22);

    is.seekg(48, std::ios_base::cur);

    // Map file type (0-7) to IngredientCategory (0-3)
    // Not sure what's going on with this, but it seems to work like this
    IngredientCategory category;
    switch (type) {
    case 0:
      category = IngredientCategory::VEGETABLES;
      break;
    case 1:
      category = IngredientCategory::MEAT;
      break;
    case 2:
      category = IngredientCategory::FRUIT;
      break;
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
      category = IngredientCategory::CHEESE;
      break;
    default:
      category = IngredientCategory::VEGETABLES;
      break;
    }

    uint8_t supplier_id = head_type;
    auto s = std::make_unique<FoodSupplier>(
        category, supplier_id, quality, supplier_name, head_type,
        minimum_contract_amount, price_levels.data(), price_levels.size());
    suppliers_.push_back(std::move(s));
  }

  is.seekg(2, std::ios_base::cur);
}

void CityFactory::load_districts(std::ifstream& is, uint8_t district_count) {
  auto district_offset = static_cast<int>(is.tellg());
  for (int i = 0; i < district_count; ++i) {
    // Read the entire 104-byte struct in one call
    DistrictDataRaw raw{};
    is.read(reinterpret_cast<char*>(&raw), sizeof(raw));

    // Create the District object
    auto name = text::iso8859_1_to_utf8(raw.name);
    auto district = std::make_unique<District>(i, name);

    district->control_screen_alternate_color(
        raw.control_screen_alternate_color == 1);
    district->traffic_density(raw.traffic_intensity);
    district->average_income_factor(raw.average_income_factor);
    district->crime_statistics(raw.crime_stats);

    district->district_type(static_cast<DistrictType>(raw.district_type));

    Logger::debug("Setting color for district {} to {}", i,
                  district_color_list_[i % district_color_list_.size()]);

    // Popularity calculation fields
    district->service_quality_factor(raw.service_speed_sensitivity);
    // Decode from x10 encoding: raw stores valuex10, we store actual value
    district->minimum_desired_canonical_pizza_count(
        raw.minimum_desired_canonical_pizza_count / 10);
    // Decode from <<4 encoding: raw stores value<<4, we store actual value
    district->minimum_desired_user_pizza_count(
        raw.minimum_desired_user_pizza_count >> 4);
    district->color(district_color_list_[i % district_color_list_.size()]);
    district->average_size(raw.average_size);
    district->average_size_modifier(raw.average_size_modifer);
    district->price_level(raw.price_level_raw);
    district->rent_level(raw.rent_level_raw);
    district->premises_availability(raw.premises_availability);

    district->demographic_factor(raw.demographic_factor);
    district->demographic_base(raw.demographic_base);

    constexpr uint8_t demographic_count = 6;

    // Store raw demographic demand values (used for customer calculations)
    std::array<float, demographic_count> demographic_demand;
    for (int c = 0; c < demographic_count; ++c) {
      demographic_demand[c] = raw.population_data[c];
    }
    district->demographic_demand(demographic_demand);

    // Also calculate normalized percentages (used for UI display)
    float sum = 0.0f;
    for (int c = 0; c < demographic_count; ++c) {
      sum += raw.population_data[c];
    }
    std::array<uint8_t, demographic_count> population_percentages;
    for (int c = 0; c < demographic_count; ++c) {
      float percent =
          (sum == 0.0f) ? 0.0f : (raw.population_data[c] / sum) * 100.0f;
      population_percentages[c] = static_cast<uint8_t>(std::floor(percent));
    }
    district->population_percentages(population_percentages);

    // Logger::debug("Pushing district to position {}", i);
    districts_.push_back(std::move(district));
  }
}

void CityFactory::parse_contacts(std::ifstream& is, uint8_t number_of_realtors,
                                 uint8_t number_of_banks,
                                 uint8_t number_of_insurance,
                                 uint8_t number_of_authorities,
                                 uint8_t number_of_others) {
  const std::array<uint8_t, NUMBER_OF_ACTOR_TYPES> counts = {
      number_of_realtors,    number_of_banks,  number_of_insurance,
      number_of_authorities, number_of_others,
  };

  for (int i = 0; i < NUMBER_OF_ACTOR_TYPES; ++i) {
    actor_contacts_[i].resize(MAX_ACTOR_PER_TYPE);
    actor_count_[i] = 0;
  }

  for (int i = 0; i < NUMBER_OF_ACTOR_TYPES; ++i) {
    for (int j = 0; j < MAX_ACTOR_PER_TYPE; ++j) {
      if (actor_heads_[i].size() <= static_cast<std::size_t>(j)) {
        actor_heads_[i].resize(j + 1);
      }

      auto name = text::read_iso8859_1_string(is, 24);
      if (j < counts[i] && !name.empty()) {
        actor_contacts_[i][j] = std::move(name);
        actor_count_[i]++;
        actor_heads_[i][j] =
            static_cast<int>(heads_[(i * MAX_ACTOR_PER_TYPE) + j] + 1);
      }
    }
  }
}

void CityFactory::parse_phone_numbers(std::ifstream& is) {

  for (int i = 0; i < NUMBER_OF_ACTOR_TYPES; ++i) {
    for (int j = 0; j < MAX_ACTOR_PER_TYPE; ++j) {
      uint32_t data;
      is.read(reinterpret_cast<char*>(&data), 4);
      if (data == 0) {
        phone_numbers_[i][j] = 0;
        continue;
      }
      phone_numbers_[i][j] = invert_transform_phone_number(data);
    }
  }
}

// Function to replicate sub_38DC0
uint32_t CityFactory::transform_phone_number(uint32_t phoneNumber) {
  const uint32_t divisor = 1'000'000;

  uint32_t quotient = phoneNumber / divisor;
  uint32_t result = quotient + phoneNumber;

  // Multiply by 4 (shift left by 2)
  result <<= 2;

  // Bitwise NOT and increment
  result = ~result;
  result += 1;
  result -= 15'000'000;

  return result;
}

uint32_t
CityFactory::invert_transform_phone_number(uint32_t transformed_value) {
  // Undo "result = ~result + 1;"
  // If T = ~x + 1, then x = ~(T - 1).
  uint32_t step3 = ~(transformed_value - 1u);

  // Undo "result <<= 2;"
  // i.e. step2 = step3 >> 2 (logical shift because we're dealing with
  // uint32_t).
  uint32_t step2 = step3 >> 2;

  // We know step2 = N + (N / 1,000,000).
  // Let M = N / 1,000,000, R = N % 1,000,000, so step2 = 1,000,001 * M + R.
  // We can solve for M, R by integer division/modulo on step2 with
  // 1,000,001. Then check that R < 1,000,000 to ensure a valid solution.
  const uint32_t divisor = 1'000'001u;
  uint32_t M = step2 / divisor; // candidate for N/1,000,000
  uint32_t R = step2 % divisor; // candidate for N % 1,000,000

  if (R >= 1'000'000u) {
    // If R >= 1,000,000, there's no valid N for this transform.
    fatal("No valid phone number (inverse transform failed).");
  }

  // Reconstruct N
  uint32_t phone_number = (M * 1'000'000u) + R;

  return phone_number;
}

void CityFactory::build_city_object(City& city, int id) {
  city.data_length(data_length_);
  city.map_data(map_data_);
  city.named_buildings(named_buildings_);
  city.city_name(city_name_);
  city.building_names(building_names_, building_name_count_);
  city.type_names(type_names_);
  // ensure each district has a pointer to the city, do this
  // before std::moving the districts to city:
  for (auto& district : districts_) {
    district->city(&city);
  }
  city.districts(std::move(districts_));
  city.types(types_);
  city.district_bez(district_bez_);
  city.properties(std::move(properties_));

  std::array<ActorType, NUMBER_OF_ACTOR_TYPES> type_list = {
      ActorType::REALTOR, ActorType::BANK,  ActorType::INSURANCE,
      ActorType::MAYOR,   ActorType::MAFIA,
  };
  std::map<uint8_t, std::unique_ptr<Actor>> actor_map;
  std::unordered_map<int, BankSettings> bank_settings_map;
  std::unordered_map<int, InsuranceSettings> insurance_settings_map;
  std::unordered_map<int, MafiaSettings> mafia_settings_map;
  std::unordered_map<int, RealtorSettings> realtor_settings_map;
  int actor_id = 0;
  std::optional<int> mayor_id;
  std::optional<int> police_id;
  int bank_index = 0;
  int insurance_index = 0;
  int mafia_index = 0;
  int realtor_index = 0;
  for (int i = 0; i < NUMBER_OF_ACTOR_TYPES; ++i) {
    int count = actor_count_[i];
    for (int j = 0; j < count; ++j) {
      // heads_[(i*10)+j] is the head argument
      // actor_contacts_[i][j] is the name
      // '1' is a placeholder value
      auto actor_type = type_list.at(i);
      if (actor_type == ActorType::MAYOR && j > 0) {
        actor_type = ActorType::POLICE;
      }
      // Todo, not sure if there can be more than two mafiosos?
      else if (actor_type == ActorType::MAFIA && j > 1) {
        actor_type = ActorType::ARMS_DEALER;
      }
      auto actor = std::make_unique<Actor>(actor_type, actor_id);
      actor->phone_number(phone_numbers_[i][j]);
      actor->name(actor_contacts_[i][j]);
      actor->head_id(heads_[(i * MAX_ACTOR_PER_TYPE) + j] + 1);

      // Track special actor IDs
      if (actor_type == ActorType::MAYOR && mayor_id) {
        mayor_id = actor_id;
      }
      else if (actor_type == ActorType::POLICE && !police_id) {
        police_id = actor_id;
      }
      else if (actor_type == ActorType::BANK) {
        if (bank_index < static_cast<int>(bank_settings_.size())) {
          bank_settings_map.emplace(actor_id, bank_settings_[bank_index]);
          bank_index++;
        }
      }
      else if (actor_type == ActorType::INSURANCE) {
        if (insurance_index < static_cast<int>(insurance_settings_.size())) {
          insurance_settings_map.emplace(actor_id,
                                         insurance_settings_[insurance_index]);
          insurance_index++;
        }
      }
      else if (actor_type == ActorType::MAFIA ||
               actor_type == ActorType::ARMS_DEALER) {
        if (mafia_index < static_cast<int>(mafia_settings_.size())) {
          mafia_settings_map.emplace(actor_id, mafia_settings_[mafia_index]);
          mafia_index++;
        }
      }
      else if (actor_type == ActorType::REALTOR) {
        realtor_settings_map.emplace(actor_id,
                                     realtor_settings_[realtor_index]);
        realtor_index++;
      }

      actor_map[actor_id] = std::move(actor);
      actor_id++;
    }
  }
  city.police_id(*police_id);
  city.mayor_id(*mayor_id);
  city.actors(std::move(actor_map));
  city.bank_settings(bank_settings_map);
  city.insurance_settings(insurance_settings_map);
  city.mafia_settings(mafia_settings_map);
  city.realtor_settings(realtor_settings_map);

  city.suppliers(std::move(suppliers_));
  city.ingredient_base_prices(ingredient_base_prices_);

  // Initialize advertising campaign options (randomized per city)
  // Assembly: init_city.asm lines 342-367
  std::array<AdvertisingRate, 9> poster_options;
  std::array<AdvertisingRate, 9> tv_ad_options;

  for (int i = 0; i < 9; i++) {
    // Poster effectiveness: random(30) + 30 = 30-59%
    int poster_eff = engine::random::rand_max_n(30) + 30;
    poster_options[i].effectiveness = poster_eff;
    // Poster cost: (effectiveness + random(25)) * 40
    poster_options[i].cost =
        (poster_eff + engine::random::rand_max_n(25)) * 40 * 100;

    // TV ad effectiveness: random(50) + 50 = 50-99%
    int tv_eff = engine::random::rand_max_n(50) + 50;
    tv_ad_options[i].effectiveness = tv_eff;
    // TV ad cost: (effectiveness + random(20)) * 100
    tv_ad_options[i].cost =
        (tv_eff + engine::random::rand_max_n(20)) * 100 * 100;
  }

  city.poster_rates(poster_options);
  city.tv_ad_rates(tv_ad_options);

  // Initialize known arms dealers for all players
  for (int player_idx = 0; player_idx < NUMBER_OF_PLAYERS; ++player_idx) {
    city.initialize_known_arms_dealers(core::PlayerId{player_idx});
  }

  // Seed the economy graph with 10 initial data points
  for (int i = 0; i < 10; ++i) {
    city.economy_trend().daily_tick();
  }
}

void CityFactory::populate_types() {
  type_names_[static_cast<int>(MapTileType::TILE_TYPE_SPECIAL)] = "Special";
  type_names_[static_cast<int>(MapTileType::TILE_TYPE_NATURE)] = "Nature";
  type_names_[static_cast<int>(MapTileType::TILE_TYPE_WATER)] = "Water";
  type_names_[static_cast<int>(MapTileType::TILE_TYPE_BUILDING)] = "Building";
  // type_na_mes[static_cast<int>(MapTileType::TILE_TYPE_ROOF)] = "Building";
  type_names_[static_cast<int>(MapTileType::TILE_TYPE_PATH)] = "Path";
  type_names_[static_cast<int>(MapTileType::TILE_TYPE_ROAD)] = "Road";
  type_names_[static_cast<int>(MapTileType::TILE_TYPE_VILLA)] = "Villa";
  type_names_[static_cast<int>(MapTileType::TILE_TYPE_RESIDENTIAL_BUILDING)] =
      "Residential building";
  type_names_[static_cast<int>(MapTileType::TILE_TYPE_OTHER)] = "";

  for (int i = 0; i < NUMBER_OF_TYPES; ++i) {
    uint8_t col = i >> 4;
    uint8_t row = i & 0x0F;

    MapTileType type;
    switch (col) {
    case 0x00:
    case 0x01:
      type = MapTileType::TILE_TYPE_OTHER;
      break;
    case 0x02:
      type = (row < 5) ? MapTileType::TILE_TYPE_OTHER
                       : MapTileType::TILE_TYPE_WATER;
      break;
    case 0x03:
      type = MapTileType::TILE_TYPE_WATER;
      break;
    case 0x04:
      type = (row < 6) ? MapTileType::TILE_TYPE_SPECIAL
                       : MapTileType::TILE_TYPE_BUILDING;
      break;
    case 0x05:
      type = MapTileType::TILE_TYPE_PATH;
      break;
    case 0x06:
    case 0x07:
      type = MapTileType::TILE_TYPE_ROAD;
      break;
    case 0x08:
      type = (row < 7) ? MapTileType::TILE_TYPE_SPECIAL
                       : MapTileType::TILE_TYPE_BUILDING;
      break;
    case 0x09:
      type = MapTileType::TILE_TYPE_VILLA;
      break;
    case 0x0A:
      type = MapTileType::TILE_TYPE_SPECIAL;
      break;
    case 0x0B:
      type = MapTileType::TILE_TYPE_RESIDENTIAL_BUILDING;
      break;
    default:
      type = MapTileType::TILE_TYPE_OTHER;
      break;
    }
    types_[i] = static_cast<int>(type);
  }
}

void CityFactory::populate_properties(City& city) {
  // For every tile check if it's a business premise, and if it is
  // figure out which district it's in, then generate a Property
  // based on the average size of the property.
  for (std::size_t index = 0; index < map_data_.size(); ++index) {
    auto type = map_data_[index];
    if (City::is_business_premise_by_type(type) || type == 0x04) {
      auto prop_type = PropertyType::BUSINESS_PREMISE;
      // Todo: restaurant?
      if (type == 0x04) {
        prop_type = PropertyType::DEPOT;
      }
      auto district_index = City::district_index_by_index(index);
      const auto& district = districts_[0x0F & district_bez_[district_index]];
      int average_size = district->average_size();
      int average_size_modifier = district->average_size_modifier();
      int rand1 = engine::random::rand_max_n(average_size_modifier);
      int rand2 = engine::random::rand_max_n(4);
      int w = std::clamp(rand1 + rand2 + average_size - average_size_modifier,
                         10, 20);
      int diff = engine::random::rand_max_n(average_size_modifier);
      int h = std::clamp(w - diff, 10, 20);
      assert(number_of_realtors_ >= 0);
      int r_id = engine::random::rand_max_n(number_of_realtors_);
      auto status = PremiseStatus::NOT_AVAILABLE;
      if (engine::random::rand_percentage_chance(
              district->premises_availability())) {
        // Status 1=FOR_RENT, 2=FOR_SALE, 3=FOR_SALE_RENT
        status = static_cast<PremiseStatus>(engine::random::rand_max_n(3) + 1);
      }
      auto bp = std::make_unique<Property>(index, prop_type, Size{w, h}, r_id,
                                           status, city);
      properties_[index] = std::move(bp);
    }
  }
}

void CityFactory::load_district_bez(int id) {
  std::string path =
      (platform::is_amiga() ? "bez" : "dat/bez") + std::to_string(id) + ".dat";
  std::ifstream is = fileutils::open(path);

  district_bez_.fill(0);
  is.read(reinterpret_cast<char*>(district_bez_.data()), district_bez_.size());
}

void CityFactory::read_realtor_data(std::ifstream& is, int number_of_realtors) {
  // Save current position
  std::streampos current_pos = is.tellg();

  // Seek to bank data at offset 1826
  is.seekg(1786);

  // Read bank settings
  std::vector<RealtorSettings> realtors;
  realtors.reserve(number_of_realtors);

  for (int i = 0; i < number_of_realtors; ++i) {
    auto unknown_1 = static_cast<uint8_t>(is.get());
    auto unknown_2 = static_cast<uint8_t>(is.get());
    auto rip_off_chance = static_cast<uint8_t>(is.get());
    auto padding = static_cast<uint8_t>(is.get());

    realtors.emplace_back(unknown_1, unknown_2, rip_off_chance, padding);
  }
  realtor_settings_ = std::move(realtors);

  // Restore original position
  is.seekg(current_pos);
}

void CityFactory::read_bank_data(std::ifstream& is, int number_of_banks) {
  // Save current position
  std::streampos current_pos = is.tellg();

  // Seek to bank data at offset 1826
  is.seekg(1826);

  // Read bank settings
  std::vector<BankSettings> banks;
  banks.reserve(number_of_banks);

  for (int i = 0; i < number_of_banks; ++i) {
    auto interest_mod = static_cast<uint8_t>(is.get());
    auto loan_mult = static_cast<uint8_t>(is.get());
    auto unused = static_cast<uint8_t>(is.get());
    auto padding = static_cast<uint8_t>(is.get());

    banks.emplace_back(interest_mod, loan_mult, unused, padding);
  }
  bank_settings_ = std::move(banks);

  // Restore original position
  is.seekg(current_pos);
}

void CityFactory::read_insurance_data(std::ifstream& is,
                                      int number_of_insurance) {
  std::streampos current_pos = is.tellg();
  is.seekg(1866);

  std::vector<InsuranceSettings> insurance;
  insurance.reserve(number_of_insurance);

  for (int i = 0; i < number_of_insurance; ++i) {
    auto premium_factor = static_cast<uint8_t>(is.get());
    auto willingness_threshold = static_cast<uint8_t>(is.get());
    auto rejection_pct = static_cast<uint8_t>(is.get());
    auto padding = static_cast<uint8_t>(is.get());

    Logger::debug(
        "Insurance [{}]: premium_factor={}, willingness_threshold={}, "
        "rejection_pct={}, padding={}",
        i, premium_factor, willingness_threshold, rejection_pct, padding);

    insurance.emplace_back(premium_factor, willingness_threshold, rejection_pct,
                           padding);
  }
  insurance_settings_ = std::move(insurance);

  is.seekg(current_pos);
}

void CityFactory::read_mafia_data(std::ifstream& is, int number_of_mafia) {
  std::streampos current_pos = is.tellg();
  is.seekg(1946);

  std::vector<MafiaSettings> mafia;
  mafia.reserve(number_of_mafia);

  for (int i = 0; i < number_of_mafia; ++i) {
    auto crime_danger_level = static_cast<uint8_t>(is.get());
    auto unknown_1 = static_cast<uint8_t>(is.get());
    auto unknown_2 = static_cast<uint8_t>(is.get());
    auto unknown_3 = static_cast<uint8_t>(is.get());

    Logger::debug("Mafia [{}]: crime_danger_level={}, unknown_1={}, "
                  "unknown_2={}, unknown_3={}",
                  i, crime_danger_level, unknown_1, unknown_2, unknown_3);

    mafia.emplace_back(crime_danger_level, unknown_1, unknown_2, unknown_3);
  }
  mafia_settings_ = std::move(mafia);

  is.seekg(current_pos);
}
