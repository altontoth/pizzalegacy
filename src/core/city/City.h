/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "core/city/CentralDepot.h"
#include "core/city/CityTypes.h"
#include "core/city/ControlMap.h"
#include "core/city/EconomyTrend.h"
#include "core/city/IngredientTrendSystem.h"
#include "core/city/JobMarket.h"
#include "core/city/MayorOffice.h"
#include "core/city/PizzaCharts.h"
#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/food/Ingredient.h"
#include "core/food/IngredientCategoryType.h"
#include "core/marketing/AdvertisingCampaign.h"
#include "core/newspaper/NewspaperEventSystem.h"
#include "core/newspaper/NewspaperState.h"
#include "core/settings/BankSettings.h"
#include "core/settings/InsuranceSettings.h"
#include "core/settings/MafiaSettings.h"
#include "core/settings/RealtorSettings.h"
#include "core/supply/FoodSupplier.h"

using IngredientPrices = core::food::IngredientArray<Money>;
#include "core/util/Constants.h"
#include "engine/Size.h"

#define NUMBER_OF_TYPES 256
#define NUMBER_OF_ACTOR_TYPES 5

// Every city opens on this rate and drifts from there.
constexpr int starting_interest_rate = 8;

enum class MapTileType {
  TILE_TYPE_SPECIAL = 0,
  TILE_TYPE_NATURE,
  TILE_TYPE_WATER,
  TILE_TYPE_BUILDING,
  TILE_TYPE_PATH,
  TILE_TYPE_ROAD,
  TILE_TYPE_VILLA,
  TILE_TYPE_RESIDENTIAL_BUILDING,
  TILE_TYPE_OTHER,
};

struct PropertyInfo {
  std::string description{"BUILDING"};
  PremiseStatus status{PremiseStatus::NOT_AVAILABLE};
  Size size{0, 0};
  int sale_price{0};
  int rent_price{0};
};

class Actor;
class District;
class Property;
namespace core::city {
class CentralDepot;
}

class City {
  friend std::ostream& operator<<(std::ostream& os, const City& city);

public:
  City(core::CityId id);
  ~City();

  core::CityId id() const;
  std::string_view name() const;

  uint8_t type(int index) const;
  int data_length() const;
  bool is_business_premise_by_index(int index) const;
  static bool is_business_premise_by_type(uint8_t type);
  static bool is_property_by_type(uint8_t type);
  MapTileType tile_category(int index) const;
  bool is_road_tile(int index) const;
  std::string description(int index) const;
  std::string building_name(int index) const;
  std::string building_type_name(int index) const;
  std::string building_specific_name(int index) const;
  std::string tile_description(int index) const;
  const std::vector<std::unique_ptr<District>>& districts() const;
  const District& district(core::DistrictIndex district_index) const;
  void end_of_day(int difficulty);
  void end_of_month();
  static constexpr int index_to_x(int index) {
    return index % MAP_TILES_PER_ROW;
  }
  static constexpr int index_to_y(int index) {
    return index / MAP_TILES_PER_ROW;
  }
  static constexpr int x_y_to_index(int x, int y) {
    return (y * MAP_TILES_PER_ROW) + x;
  }

  // Setters for factory:
  void data_length(int length);
  void map_data(const std::vector<uint8_t>& d);
  void named_buildings(const std::map<int, std::string>& nb);
  void building_names(const std::array<std::array<std::string, 24>, 8>& bn,
                      const std::array<int, 8>& counts);
  void actors(std::map<core::ActorId, std::unique_ptr<Actor>> actors);
  void properties(std::map<int, std::unique_ptr<Property>> bm);
  std::vector<Property*> business_premises() const;
  std::vector<Property*> properties() const;
  Property* property_by_index(int index) const;
  void districts(std::vector<std::unique_ptr<District>> d);
  void suppliers(std::vector<std::unique_ptr<FoodSupplier>> s);
  void types(const std::array<uint8_t, NUMBER_OF_TYPES>& types);
  void
  type_names(const std::array<std::string, /*NUMBER_OF_TYPES*/ 10>& type_names);
  void district_bez(std::array<uint8_t, 300>);
  void
  bank_settings(std::unordered_map<core::ActorId, BankSettings> bank_settings) {
    bank_settings_ = std::move(bank_settings);
  };
  void realtor_settings(
      std::unordered_map<core::ActorId, RealtorSettings> realtor_settings) {
    realtor_settings_ = std::move(realtor_settings);
  };
  void insurance_settings(
      std::unordered_map<core::ActorId, InsuranceSettings> insurance_settings) {
    insurance_settings_ = std::move(insurance_settings);
  };
  void mafia_settings(
      std::unordered_map<core::ActorId, MafiaSettings> mafia_settings) {
    mafia_settings_ = std::move(mafia_settings);
  };

  std::vector<const Actor*> actors(ActorType type) const;
  std::vector<const FoodSupplier*> food_suppliers(ActorType type) const;
  std::vector<FoodSupplier*> food_suppliers(ActorType type);
  const std::vector<std::unique_ptr<FoodSupplier>>& suppliers() const {
    return suppliers_;
  }

  // Returns the supplier with the preferred_id, or the first supplier in the
  // city if no preferred_id specified.
  const FoodSupplier* supplier_for(IngredientCategory category,
                                   std::optional<int> preferred_id) const;

  Actor* actor_by_id(core::ActorId actor_id) const;
  Actor* mayor() const { return actor_by_id(mayor_id_); }
  Actor* police() const { return actor_by_id(police_id_); }
  void mayor_id(core::ActorId id) { mayor_id_ = id; }
  void police_id(core::ActorId id) { police_id_ = id; }
  void first_bank_id(core::ActorId id) { first_bank_id_ = id; }
  core::ActorId first_bank_id() const { return first_bank_id_; }
  static BuildingType building_type(uint8_t type);
  BuildingType building_type_by_index(int index) const;

  static bool is_named_building(uint8_t type);
  bool is_named_building_by_index(int index) const;
  const std::map<int, std::string>& named_building_entries() const {
    return named_buildings_;
  }
  PropertyInfo property_info_by_index(uint8_t index);

  // District methods
  core::DistrictIndex district_by_district_index(int district_index) const;
  core::DistrictIndex district_by_xy_index(int tile_x, int tile_y) const;
  static int district_index_by_xy_index(int tile_x, int tile_y);
  core::DistrictIndex district_by_index(int index) const;

  IngredientTrendSystem& ingredient_trend_system() {
    return ingredient_trend_system_;
  }
  const IngredientTrendSystem& ingredient_trend_system() const {
    return ingredient_trend_system_;
  }

  // Spoilage rate (normal = 1, heat wave = 8)
  void spoilage_rate(int rate) { spoilage_rate_ = rate; }
  int spoilage_rate() const { return spoilage_rate_; }

  void spoilage_duration_counter(int days) {
    spoilage_duration_counter_ = days;
  }
  int spoilage_duration_counter() const { return spoilage_duration_counter_; }

  // Trade embargo counters (4 slots)
  void trade_embargo_counter(IngredientCategory cat, int days) {
    trade_embargo_counters_[static_cast<int>(cat)] = days;
  }

  int trade_embargo_counter(IngredientCategory cat) const {
    return trade_embargo_counters_[static_cast<int>(cat)];
  }

  // Advertising campaign options (randomized at city init)
  void init_advertising_options();

  void
  poster_rates(const std::array<AdvertisingRate, NUMBER_OF_POSTERS>& rates) {
    poster_rates_ = rates;
  }

  void tv_ad_rates(const std::array<AdvertisingRate, NUMBER_OF_TV_ADS>& rates) {
    tv_ad_rates_ = rates;
  }

  static uint8_t district_index_by_index(int index);

  static std::optional<BuildingType> decode_special_building_type(uint8_t type);

  const AdvertisingRate& poster_option(int index) const {
    return poster_rates_.at(index);
  }

  const AdvertisingRate& tv_ad_option(int index) const {
    return tv_ad_rates_.at(index);
  }
  const std::unordered_map<core::ActorId, BankSettings>& bank_settings() const {
    return bank_settings_;
  }

  const std::unordered_map<core::ActorId, RealtorSettings>&
  realtor_settings() const {
    return realtor_settings_;
  }

  const BankSettings& bank_settings(core::ActorId actor_id) const {
    auto it = bank_settings_.find(actor_id);
    assert(it != bank_settings_.end() && "Invalid bank actor_id");
    return it->second;
  }

  const RealtorSettings& realtor_settings(core::ActorId actor_id) const {
    auto it = realtor_settings_.find(actor_id);
    assert(it != realtor_settings_.end() && "Invalid realtor actor_id");
    return it->second;
  }

  const std::unordered_map<core::ActorId, InsuranceSettings>&
  insurance_settings() const {
    return insurance_settings_;
  }

  const InsuranceSettings& insurance_settings(core::ActorId actor_id) const {
    auto it = insurance_settings_.find(actor_id);
    assert(it != insurance_settings_.end() && "Invalid insurance actor_id");
    return it->second;
  }

  const std::unordered_map<core::ActorId, MafiaSettings>&
  mafia_settings() const {
    return mafia_settings_;
  }

  const MafiaSettings& mafia_settings(core::ActorId actor_id) const {
    auto it = mafia_settings_.find(actor_id);
    assert(it != mafia_settings_.end() && "Invalid mafia actor_id");
    return it->second;
  }
  void ingredient_base_prices(const IngredientPrices& prices) {
    ingredient_base_prices_ = prices;
  }
  const IngredientPrices& ingredient_base_prices() const {
    return ingredient_base_prices_;
  }
  Money ingredient_base_price(core::food::IngredientType ingredient) const {
    return ingredient_base_prices_[ingredient];
  }

  core::character::JobMarket& job_market() { return job_market_; }
  const core::character::JobMarket& job_market() const { return job_market_; }

  // Blacklist: staff remembers having been fired and for some time refuse to
  // work for you again.
  void blacklist_staff(uint8_t character_id, core::PlayerId player);
  bool is_blacklisted(uint8_t character_id, core::PlayerId player) const;
  // Daily chance of forgiving and becoming available
  void run_staff_forgiveness(core::PlayerId player, uint8_t charisma);
  const std::unordered_map<uint8_t, std::set<core::PlayerId>>&
  staff_blacklist() const {
    return staff_blacklist_;
  }

  // Actors who hung up and will not take calls until the next time the player
  // ends the day in this city. Ids are local to the city.
  void refuse_calls_from(core::ActorId actor_id) {
    actors_refusing_calls_.insert(actor_id);
  }
  bool is_refusing_calls(core::ActorId actor_id) const {
    return actors_refusing_calls_.contains(actor_id);
  }
  void clear_refused_calls() { actors_refusing_calls_.clear(); }
  std::vector<core::ActorId> refused_call_actors() const;

  // Vendor spending tracking (resets weekly, per-player)
  void record_vendor_purchase(core::PlayerId player_id, uint8_t vendor_id,
                              Money amount);
  Money vendor_spending_this_week(core::PlayerId player_id,
                                  uint8_t vendor_id) const;
  const std::unordered_map<uint8_t, Money>&
  weekly_vendor_spending(core::PlayerId player_id) const;
  // Overwrites the running total rather than adding to it.
  void restore_vendor_spending(core::PlayerId player_id, uint8_t vendor_id,
                               Money amount);
  void reset_weekly_vendor_spending();

  // Arms dealer visibility tracking (per-player)
  // Used to hide/reveal arms dealers in terminal OTHERS category
  const std::vector<core::ActorId>&
  known_arms_dealers(core::PlayerId player_id) const;
  uint8_t known_arms_dealer_count(core::PlayerId player_id) const;
  bool is_arms_dealer_known(core::PlayerId player_id, core::ActorId id) const;
  void add_known_arms_dealer(core::PlayerId player_id, core::ActorId id);
  void clear_known_arms_dealers(core::PlayerId player_id);
  void initialize_known_arms_dealers(core::PlayerId player_id);
  std::vector<const Actor*> all_arms_dealers() const;

  core::city::CentralDepot& central_depot(core::PlayerId player_id);
  const core::city::CentralDepot& central_depot(core::PlayerId player_id) const;
  MayorOffice& mayor_office() { return mayor_office_; }
  const MayorOffice& mayor_office() const { return mayor_office_; }

  PizzaCharts& pizza_charts() { return pizza_charts_; }
  const PizzaCharts& pizza_charts() const { return pizza_charts_; }

  ControlMap& control_map() { return control_map_; }
  const ControlMap& control_map() const { return control_map_; }

  EconomyTrend& economy_trend() { return economy_trend_; }
  const EconomyTrend& economy_trend() const { return economy_trend_; }

  // The base every bank in this city prices its loans off, before each bank's
  // own modifier.
  int interest_rate() const { return interest_rate_; }
  void interest_rate(int rate) { interest_rate_ = rate; }

  core::newspaper::NewspaperState& newspaper_state() {
    return newspaper_state_;
  }
  const core::newspaper::NewspaperState& newspaper_state() const {
    return newspaper_state_;
  }
  core::newspaper::NewspaperEventSystem& newspaper_event_system() {
    return newspaper_event_system_;
  }
  const core::newspaper::NewspaperEventSystem& newspaper_event_system() const {
    return newspaper_event_system_;
  }

private:
  void init_supplier_stock_levels();
  void replenish_supplier_stock_levels();

  core::CityId id_;

  int data_length_ = 0;
  std::vector<uint8_t> data_;

  // Actor lists: 5 categories, each a vector of unique_ptr<Actor>
  std::map<core::ActorId, std::unique_ptr<Actor>> actors_;

  // Suppliers:
  std::vector<std::unique_ptr<FoodSupplier>> suppliers_;
  std::array<AdvertisingRate, NUMBER_OF_POSTERS> poster_rates_{};
  std::array<AdvertisingRate, NUMBER_OF_TV_ADS> tv_ad_rates_{};

  // Named buildings and related info:
  std::map<int, std::string> named_buildings_;
  std::array<std::array<std::string, 24>, 8> building_names_;
  std::array<int, 8> building_name_count_{};
  std::array<std::string, /*NUMBER_OF_TYPES*/ 10> type_names_{};
  std::array<uint8_t, NUMBER_OF_TYPES> types_{};
  std::vector<std::unique_ptr<District>> districts_;
  std::map<int, std::unique_ptr<Property>> properties_;
  std::array<uint8_t, 300> district_bez_{};
  std::unordered_map<core::ActorId, BankSettings> bank_settings_;
  std::unordered_map<core::ActorId, InsuranceSettings> insurance_settings_;
  std::unordered_map<core::ActorId, MafiaSettings> mafia_settings_;
  std::unordered_map<core::ActorId, RealtorSettings> realtor_settings_;
  core::ActorId mayor_id_{-1};
  core::ActorId police_id_{-1};
  core::ActorId first_bank_id_{-1};
  std::unordered_map<uint8_t, std::set<core::PlayerId>> staff_blacklist_;
  std::unordered_set<core::ActorId> actors_refusing_calls_;
  core::character::JobMarket job_market_;
  // Vendor spending tracking (resets weekly, per-player): vendor_id -> total
  // spent this week
  std::array<std::unordered_map<uint8_t, Money>, NUMBER_OF_PLAYERS>
      weekly_vendor_spending_;
  IngredientPrices ingredient_base_prices_;

  // Unified ingredient trend system (replaces old separate arrays)
  IngredientTrendSystem ingredient_trend_system_;

  // Spoilage system (for heat wave event)
  int spoilage_rate_{1};
  int spoilage_duration_counter_{0};

  // Trade embargo counters
  std::array<int, 4> trade_embargo_counters_{};
  std::array<core::city::CentralDepot, NUMBER_OF_PLAYERS> central_depots_;

  // Arms dealer visibility tracking: per-player known arms dealers
  // Used to implement "hidden contacts" feature for arms dealers in OTHERS
  // category
  struct KnownArmsDealer {
    uint8_t known_count{0};
    std::vector<core::ActorId> known_actor_ids; // Max 10 in original
  };

  std::array<KnownArmsDealer, NUMBER_OF_PLAYERS> known_arms_dealers_;
  MayorOffice mayor_office_;
  PizzaCharts pizza_charts_;
  ControlMap control_map_;
  EconomyTrend economy_trend_;
  int interest_rate_{starting_interest_rate};

  core::newspaper::NewspaperState newspaper_state_;
  core::newspaper::NewspaperEventSystem newspaper_event_system_;
};

/*
[ Business Premises ]
5 bits for premise W
5 bits for premise H
3 bits for realtor
3 bits for status (Not Available/Sale/Rent/Combo/Owned)
    32 bits for price
    16 bits for rent
Initial Rent = W x H x 15 (madrid)
Initial Price = Rent x 80 (madrid)
Paris is rent x 20 but price x 40
*/
