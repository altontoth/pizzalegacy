/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/city/City.h"
#include "core/common/Ids.h"
#include "core/settings/InsuranceSettings.h"
#include "core/settings/MafiaSettings.h"
#include "core/supply/FoodSupplier.h"

class Property;

class CityFactory {
public:
  static std::unique_ptr<City>
  create_city(core::CityId id,
              const std::vector<std::pair<uint8_t, int>>& player_chars = {});

private:
  static constexpr int NUMBER_OF_NAMED_BUILDINGS = 8;
  static constexpr int MAX_BUILDING_NAMES_PER_TYPE = 24;
  static constexpr int MAX_ACTOR_PER_TYPE = 10;
  static constexpr int UNIQUE_DISTRICT_COLORS = 12;

  std::vector<unsigned char> map_data_;
  std::map<int, std::string> named_buildings_{};
  std::array<int, NUMBER_OF_NAMED_BUILDINGS> building_name_count_{};
  std::array<std::array<std::string, MAX_BUILDING_NAMES_PER_TYPE>,
             NUMBER_OF_NAMED_BUILDINGS>
      building_names_{};

  std::array<int, NUMBER_OF_ACTOR_TYPES> actor_count_{};
  std::array<std::vector<std::string>, NUMBER_OF_ACTOR_TYPES> actor_contacts_{};
  std::array<std::vector<uint8_t>, NUMBER_OF_ACTOR_TYPES> actor_heads_{};
  std::array<std::array<uint32_t, MAX_ACTOR_PER_TYPE>, NUMBER_OF_ACTOR_TYPES>
      phone_numbers_{};
  std::array<uint8_t, NUMBER_OF_ACTOR_TYPES * MAX_ACTOR_PER_TYPE> heads_{};
  std::array<std::string, /*NUMBER_OF_TYPES*/ 10> type_names_{};
  std::array<uint8_t, /*NUMBER_OF_TILES */ 256> types_{};
  std::array<uint8_t, UNIQUE_DISTRICT_COLORS> district_color_list_;
  std::vector<std::unique_ptr<District>> districts_;
  std::map<int, std::unique_ptr<Property>> properties_;
  std::vector<BankSettings> bank_settings_;
  std::vector<InsuranceSettings> insurance_settings_;
  std::vector<MafiaSettings> mafia_settings_;
  std::vector<RealtorSettings> realtor_settings_;

  // Suppliers:
  int supplier_count_ = 0;
  std::vector<std::unique_ptr<FoodSupplier>> suppliers_;
  int number_of_realtors_{-1};

  std::string city_name_;
  int data_length_ = 0;
  std::array<uint8_t, 300> district_bez_;
  IngredientPrices ingredient_base_prices_;

  void load_map_data(int city_id);
  void load_city_data(int city_id);

  void load_building_names(std::ifstream& is);
  void load_actors(std::ifstream& is); // Not strictly needed now
  void load_suppliers(std::ifstream& is, int supplier_count);
  void load_districts(std::ifstream& is, uint8_t district_count);
  void load_ingredient_prices(std::ifstream& is);
  void parse_contacts(std::ifstream& is, uint8_t number_of_realtors,
                      uint8_t number_of_banks, uint8_t number_of_insurance,
                      uint8_t number_of_authorities, uint8_t number_of_others);
  void parse_phone_numbers(std::ifstream& is);
  void populate_properties(City& city);

  void build_city_object(City& city, int id);
  void populate_types();
  void load_color_definition();
  static uint32_t transform_phone_number(uint32_t);
  static uint32_t invert_transform_phone_number(uint32_t);
  void load_district_bez(int id);
  void read_bank_data(std::ifstream& is, int number_of_banks);
  void read_insurance_data(std::ifstream& is, int number_of_insurance);
  void read_mafia_data(std::ifstream& is, int number_of_mafia);
  void read_realtor_data(std::ifstream& is, int number_of_realtors);
};
