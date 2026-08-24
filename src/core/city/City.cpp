/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/City.h"

#include "core/city/CityNames.h"
#include "core/city/District.h"
#include "core/city/Property.h"
#include "core/finance/Money.h"
#include "core/food/Ingredient.h"
#include "core/food/IngredientCategory.h"
#include "core/util/Constants.h"
#include "engine/Random.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>

std::ostream& operator<<(std::ostream& os, const City& city) {
  os << "City:\n";
  os << "  id =" << city.id_ << "\n";
  os << "  name =" << city.name() << "\n";
  os << "  properties = " << '\n';
  for (const auto& [id, bp] : city.properties_) {
    os << "    " << id << ": " << *bp << "\n";
  }

  return os;
}

City::City(core::CityId id) : id_(id) {}

City::~City() = default;

core::CityId City::id() const { return id_; }

std::string_view City::name() const { return core::city_name(id_); }

void City::init_supplier_stock_levels() {
  for (auto& supplier : suppliers_) {
    for (auto ing_type :
         core::food::CategoryRegistry::range(supplier->category())) {
      if (supplier->price_level_for_ingredient(ing_type) != 0) {
        int random_stock =
            engine::random::rand_max_n(5000) + 8000; // 8000-13000 kg
        supplier->set_stock(ing_type, random_stock);
      }
    }
  }
}

// Replenish stock levels in the city warehouses
void City::replenish_supplier_stock_levels() {
  // Only one category gets replenished per day, chosen at random
  auto todays_category =
      static_cast<IngredientCategory>(engine::random::rand_max_n(4));
  for (auto& supplier : suppliers_) {
    if (supplier->category() != todays_category) {
      continue;
    }
    // If chosen we check if less than 10K and if so increasy by 4K to 12 K
    for (auto ing_type :
         core::food::CategoryRegistry::range(supplier->category())) {
      if (supplier->price_level_for_ingredient(ing_type) == 0) {
        continue;
      }
      const auto current_stock = supplier->stock(ing_type);
      if (current_stock < 10'000) {
        int new_stock =
            engine::random::rand_max_n(8000) + 4000; // 4000-12000 kg
        supplier->set_stock(ing_type, current_stock + new_stock);
      }
    }
  }
}

void City::init_advertising_options() {
  // The original just has one loop for posters and tv ads combined, so the
  // randomization is slightly different from the original. I prefer having
  // separate counts of posters and TV ads so that's why I split it across two
  // loops.
  for (int i = 0; i < NUMBER_OF_POSTERS; ++i) {
    int effectiveness = engine::random::rand_max_n(30) + 30;
    int dollars = (effectiveness + engine::random::rand_max_n(25)) * 40;
    poster_rates_[i] = {.effectiveness = effectiveness,
                        .cost = dollars * 1_usd};
  }

  for (int i = 0; i < NUMBER_OF_TV_ADS; ++i) {
    int effectiveness = engine::random::rand_max_n(50) + 50;
    int dollars = (effectiveness + engine::random::rand_max_n(20)) * 100;
    tv_ad_rates_[i] = {.effectiveness = effectiveness, .cost = dollars * 1_usd};
  }
}

bool City::is_property_by_type(uint8_t type) {
  if (is_business_premise_by_type(type) || type == 0x04) {
    return true;
  }
  char landsym_row = (type & 0x0F);
  char landsym_col = (type >> 4);

  return false;
}

bool City::is_business_premise_by_type(uint8_t type) {
  if (is_named_building(type)) {
    return false;
  }
  // We want to check if type is one of: 0x8a, 0x9a, 0xaa, 0xba, 0xca, 0xda,
  // 0xea, 0xfa That means:
  //   - The low nibble is 10:     (type & 0x0F) == 0x0A
  //   - The high nibble is 8..F: (type & 0xF0) is between 0x8A and 0xFA
  char landsym_row = (type & 0x0F);
  char landsym_col = (type >> 4);

  return (landsym_row == 0x0A && landsym_col >= 0x08) ||
         (landsym_row == 0x08 && landsym_col > 0x08);
}

bool City::is_business_premise_by_index(int index) const {
  return is_business_premise_by_type(data_[index]);
}

MapTileType City::tile_category(int index) const {
  unsigned char t = type(index);
  return static_cast<MapTileType>(types_[static_cast<int>(t)]);
}

bool City::is_road_tile(int index) const {
  uint8_t t = type(index);
  uint8_t col = t & 0x0F;
  return (col == 0x06 || col == 0x07) && t != 0xF6 && t != 0xE6;
}

std::string City::description(int index) const {
  unsigned char t = type(index);
  uint8_t building_name_index = types_[(int)t];
  // Logger::debug("Type is %x and type is {}", type, types[(int)type]);
  if (building_name_index == 0) {
    // Todo: something with our premises array.
    // sprintf(name, "Business Premise %dx%d",
    // business_premises[index]->width(),
    // business_premises[index]->height());
    return {};
  }

  if (building_name_index == 3) {
    return building_type_name(index) + "|" + building_specific_name(index);
  }

  return type_names_[building_name_index];
}

int City::data_length() const { return data_length_; }

uint8_t City::type(int index) const {
  assert(index >= 0 && index < data_length_);

  // If this is a business premise owned by a player,
  // we have to return the shop frontend for that user,
  // and those live in 0x0A to 0x7A, which is what
  // the bitwise operations here return (3 => 0x3A)
  if (is_business_premise_by_index(index)) {
    auto* const property = property_by_index(index);
    auto owner_id = property->owner_or_tenant();
    if (owner_id) {
      return ((owner_id->value & 0xF) << 4) | 0xA;
    }
  }
  return data_[index];
}

std::string City::building_type_name(int index) const {
  auto type = data_[index];
  auto opt_type = decode_special_building_type(type);
  if (!opt_type) {
    return "";
  }

  static constexpr std::array<std::string_view, 8> names = {
      "Church", "Bank", "Cinema",           "School",
      "Disco",  "Bar",  "Department store", "Hotel",
  };
  auto name_index =
      static_cast<int>(*opt_type) - static_cast<int>(BuildingType::CHURCH);
  return std::string(names[name_index]);
}

std::string City::building_specific_name(int index) const {
  auto it = named_buildings_.find(index);
  if (it == named_buildings_.end()) {
    return "";
  }
  return it->second;
}

std::string City::building_name(int index) const {
  auto type_name = building_type_name(index);
  if (type_name.empty()) {
    return "";
  }
  auto specific = building_specific_name(index);
  if (specific.empty()) {
    return type_name;
  }
  return type_name + " " + specific;
}

std::optional<BuildingType> City::decode_special_building_type(uint8_t type) {
  // Special buildings: 0x88, 0x98, ..., 0xF8 (high nibble 0x8..0xF, low nibble
  // 0x8)
  if ((type & 0x0F) != 0x08) {
    return std::nullopt;
  }

  uint8_t btype = ((type & 0xF0) >> 4) - 8;
  if (btype > 7) {
    return std::nullopt;
  }

  return static_cast<BuildingType>(static_cast<int>(BuildingType::CHURCH) +
                                   btype);
}

BuildingType City::building_type(uint8_t type) {
  if (type == 0x04 || type == 0x05) {
    return BuildingType::DEPOT;
  }
  auto decoded = decode_special_building_type(type);
  return decoded.value_or(BuildingType::NONE);
}

BuildingType City::building_type_by_index(int index) const {
  // First check if this is a business premise with a restaurant
  auto* property = property_by_index(index);
  if (property && property->has_restaurant()) {
    return BuildingType::RESTAURANT;
  }
  // Otherwise use the static method based on tile type
  // Use type() instead of data_[] to get bounds checking
  return building_type(type(index));
}

bool City::is_named_building_by_index(int index) const {
  return is_named_building(data_[index]);
}

bool City::is_named_building(uint8_t type) {
  if ((type & 0x0F) == 0x8) {
    unsigned char c = ((type & 0xF0) >> 4);
    return (c >= 0x8 && c <= 0xF);
  }
  return false;
}

PropertyInfo City::property_info_by_index(uint8_t index) {
  PropertyInfo p;
  p.description = tile_description(index);
  auto it = properties_.find(index);
  if (it != properties_.end()) {
    const auto& bp = it->second;
    p.size = bp->size();
  }
  return p;
}

std::string City::tile_description(int index) const {
  uint8_t tile_data = type(index);
  uint8_t lower_nibble = tile_data & 0x0F;

  // These are basically the rows in landsym.vga:
  switch (lower_nibble) {
  case 0x0:
    return "nature";
  case 0x3:
    return "water";
  case 0x4:
    return "building";
  case 0x5:
    return "path";
  case 0x6:
  case 0x7:
    return "road";
  case 0x9:
    return "villa";
  case 0xB:
    return "residential|building";
  default:
    return "";
  }
}

const std::vector<std::unique_ptr<District>>& City::districts() const {
  return districts_;
}

const District& City::district(core::DistrictIndex district_index) const {
  return *districts_.at(district_index.value);
}

void City::end_of_day(int difficulty) {
  replenish_supplier_stock_levels();

  // Advance all active ingredient trend effects one day.
  ingredient_trend_system_.daily_tick();

  // The original doesn't actually seem to honor trade embargoes, but
  // we have an option to fix that 'bug', so update counters here
  // (it's safe regardless of the option because we check before using these
  // values in the EuroMap).
  for (int i = 0; i < 4; i++) {
    if (trade_embargo_counters_[i] > 0) {
      trade_embargo_counters_[i]--;
    }
  }

  if (spoilage_duration_counter_ > 0) {
    spoilage_duration_counter_--;
  }
  else if (spoilage_rate_ != 1) {
    spoilage_rate_ = 1;
  }

  job_market_.end_of_day(difficulty);
}

void City::end_of_month() {
  // Monthly property availability cycle
  // Each commercial property rolls against its district's appeal percentage.
  // If the roll succeeds, available properties are taken off market and
  // unavailable untenanted ones are put back on.
  for (auto& [index, property] : properties_) {
    auto district_index = district_by_index(index);
    const auto& dist = district(district_index);

    if (!engine::random::rand_percentage_chance(dist.premises_availability())) {
      continue;
    }

    auto status = property->status();
    if (status == PremiseStatus::FOR_RENT ||
        status == PremiseStatus::FOR_SALE ||
        status == PremiseStatus::FOR_SALE_RENT) {
      property->status(PremiseStatus::NOT_AVAILABLE);
    }
    else if (status == PremiseStatus::NOT_AVAILABLE &&
             !property->owner_or_tenant()) {
      auto new_status = premise_status_from_original(
          static_cast<uint8_t>(engine::random::rand_max_n(3) + 1));
      property->status(new_status);
    }
  }
}

Actor* City::actor_by_id(uint8_t actor_id) const {
  return actors_.at(actor_id).get();
}
void City::districts(std::vector<std::unique_ptr<District>> d) {
  districts_ = std::move(d);
}
void City::suppliers(std::vector<std::unique_ptr<FoodSupplier>> s) {
  suppliers_ = std::move(s);
  init_supplier_stock_levels();
}

void City::types(const std::array<uint8_t, 256>& types) { types_ = types; }
void City::type_names(
    const std::array<std::string, /*NUMBER_OF_TYPES*/ 10>& type_names) {
  type_names_ = type_names;
}

std::vector<const Actor*> City::actors(ActorType type) const {
  std::vector<const Actor*> actor_list{};
  for (const auto& [_, actor] : actors_) {
    if (actor->type() == type) {
      actor_list.push_back(actor.get());
    }
  }
  return actor_list;
}

std::vector<const FoodSupplier*> City::food_suppliers(ActorType type) const {
  std::vector<const FoodSupplier*> supplier_list{};
  for (const auto& supplier : suppliers_) {
    if (supplier->actor_type() == type) {
      supplier_list.push_back(supplier.get());
    }
  }
  return supplier_list;
}

std::vector<FoodSupplier*> City::food_suppliers(ActorType type) {
  std::vector<FoodSupplier*> supplier_list{};
  for (auto& supplier : suppliers_) {
    if (supplier->actor_type() == type) {
      supplier_list.push_back(supplier.get());
    }
  }
  return supplier_list;
}

void City::data_length(int length) { data_length_ = length; }

void City::map_data(const std::vector<unsigned char>& d) {
  data_.resize(d.size());
  std::ranges::copy(d, data_.begin());
}

void City::named_buildings(const std::map<int, std::string>& nb) {
  named_buildings_ = nb;
}

void City::building_names(const std::array<std::array<std::string, 24>, 8>& bn,
                          const std::array<int, 8>& counts) {
  building_name_count_ = counts;
  building_names_ = bn;
}

void City::actors(std::map<uint8_t, std::unique_ptr<Actor>> actors) {
  actors_ = std::move(actors);
}

void City::properties(std::map<int, std::unique_ptr<Property>> bm) {
  properties_ = std::move(bm);
}

void City::district_bez(std::array<uint8_t, 300> district_bez) {
  district_bez_ = district_bez;
}

core::DistrictIndex City::district_by_district_index(int district_index) const {
  assert(district_index < static_cast<int>(district_bez_.size()));
  return core::DistrictIndex{0x0F & district_bez_.at(district_index)};
}

// Given city tile coordinates (in the 160x120 grid), this function computes
// the corresponding district cell and returns its index.
int City::district_index_by_xy_index(int tile_x, int tile_y) {
  // Each district cell covers (MAP_TILES_PER_ROW / NUMBER_OF_CELL_COLUMNS)
  // city tiles in the x-direction (and similarly for y).
  int tiles_per_district_x =
      MAP_TILES_PER_ROW / NUMBER_OF_DISTRICT_CELL_COLUMNS;
  int tiles_per_district_y =
      MAP_TILES_PER_COLUMN / NUMBER_OF_DISTRICT_CELL_ROWS;

  int district_col = tile_x / tiles_per_district_x;
  int district_row = tile_y / tiles_per_district_y;

  return (district_row * NUMBER_OF_DISTRICT_CELL_COLUMNS) + district_col;
}

core::DistrictIndex City::district_by_xy_index(int tile_x, int tile_y) const {
  return district_by_district_index(district_index_by_xy_index(tile_x, tile_y));
}

// Given the tile index, figure out which district it is in.
core::DistrictIndex City::district_by_index(int index) const {
  int tile_x = index % MAP_TILES_PER_ROW;
  int tile_y = index / MAP_TILES_PER_ROW;
  return district_by_xy_index(tile_x, tile_y);
}

// Given the tile index, figure out which district index it has
uint8_t City::district_index_by_index(int index) {
  int tile_x = index % MAP_TILES_PER_ROW;
  int tile_y = index / MAP_TILES_PER_ROW;
  return district_index_by_xy_index(tile_x, tile_y);
}

std::vector<Property*> City::business_premises() const {
  std::vector<Property*> result;
  result.reserve(properties_.size());

  for (const auto& [index, premise] : properties_) {
    if (is_business_premise_by_index(index) && !premise->has_restaurant()) {
      result.push_back(premise.get());
    }
  }

  return result;
}

std::vector<Property*> City::properties() const {
  std::vector<Property*> result;
  result.reserve(properties_.size());

  for (const auto& [_, premise] : properties_) {
    result.push_back(premise.get());
  }

  return result;
}

Property* City::property_by_index(int index) const {
  auto it = properties_.find(index);
  if (it != properties_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void City::blacklist_staff(uint8_t character_id, core::PlayerId player) {
  staff_blacklist_[character_id].insert(player);
}

bool City::is_blacklisted(uint8_t character_id, core::PlayerId player) const {
  auto it = staff_blacklist_.find(character_id);
  return it != staff_blacklist_.end() && it->second.contains(player);
}

void City::run_staff_forgiveness(core::PlayerId player, uint8_t charisma) {
  for (auto it = staff_blacklist_.begin(); it != staff_blacklist_.end();) {
    if (it->second.contains(player) &&
        engine::random::rand_percentage_chance(charisma)) {
      it->second.erase(player);
    }
    it = it->second.empty() ? staff_blacklist_.erase(it) : std::next(it);
  }
}

void City::record_vendor_purchase(core::PlayerId player_id, uint8_t vendor_id,
                                  Money amount) {
  assert(player_id.value >= 0 && player_id.value < NUMBER_OF_PLAYERS);
  auto& spending_map = weekly_vendor_spending_[player_id.value];
  auto it = spending_map.find(vendor_id);
  if (it != spending_map.end()) {
    it->second = it->second + amount;
  }
  else {
    spending_map.emplace(vendor_id, amount);
  }
}

Money City::vendor_spending_this_week(core::PlayerId player_id,
                                      uint8_t vendor_id) const {
  assert(player_id.value >= 0 && player_id.value < NUMBER_OF_PLAYERS);
  const auto& spending_map = weekly_vendor_spending_[player_id.value];
  auto it = spending_map.find(vendor_id);
  if (it != spending_map.end()) {
    return it->second;
  }
  return Money{0};
}

void City::restore_vendor_spending(core::PlayerId player_id, uint8_t vendor_id,
                                   Money amount) {
  assert(player_id.value >= 0 && player_id.value < NUMBER_OF_PLAYERS);
  weekly_vendor_spending_[player_id.value][vendor_id] = amount;
}

const std::unordered_map<uint8_t, Money>&
City::weekly_vendor_spending(core::PlayerId player_id) const {
  assert(player_id.value >= 0 && player_id.value < NUMBER_OF_PLAYERS);
  return weekly_vendor_spending_[player_id.value];
}

void City::reset_weekly_vendor_spending() {
  for (auto& spending_map : weekly_vendor_spending_) {
    spending_map.clear();
  }
}

core::city::CentralDepot& City::central_depot(core::PlayerId player_id) {
  assert(player_id.value >= 0 && player_id.value < NUMBER_OF_PLAYERS);
  return central_depots_[player_id.value];
}

const core::city::CentralDepot&
City::central_depot(core::PlayerId player_id) const {
  assert(player_id.value >= 0 && player_id.value < NUMBER_OF_PLAYERS);
  return central_depots_[player_id.value];
}

// Arms dealer visibility tracking implementation
const std::vector<ActorId>&
City::known_arms_dealers(core::PlayerId player_id) const {
  assert(player_id.value >= 0 && player_id.value < NUMBER_OF_PLAYERS);
  return known_arms_dealers_[static_cast<size_t>(player_id.value)]
      .known_actor_ids;
}

uint8_t City::known_arms_dealer_count(core::PlayerId player_id) const {
  assert(player_id.value >= 0 && player_id.value < NUMBER_OF_PLAYERS);
  return known_arms_dealers_[static_cast<size_t>(player_id.value)].known_count;
}

bool City::is_arms_dealer_known(core::PlayerId player_id, ActorId id) const {
  const auto& known_ids = known_arms_dealers(player_id);
  return std::find(known_ids.begin(), known_ids.end(), id) != known_ids.end();
}

void City::add_known_arms_dealer(core::PlayerId player_id, ActorId id) {
  assert(player_id.value >= 0 && player_id.value < NUMBER_OF_PLAYERS);

  auto& known = known_arms_dealers_[static_cast<size_t>(player_id.value)];

  // Don't add if already known
  if (std::ranges::find(known.known_actor_ids, id) !=
      known.known_actor_ids.end()) {
    return;
  }

  // Add to known list (max 10 in original game)
  if (known.known_actor_ids.size() < 10) {
    known.known_actor_ids.push_back(id);
    known.known_count = static_cast<uint8_t>(known.known_actor_ids.size());
  }
}

void City::clear_known_arms_dealers(core::PlayerId player_id) {
  assert(player_id.value >= 0 && player_id.value < NUMBER_OF_PLAYERS);

  auto& known = known_arms_dealers_[static_cast<size_t>(player_id.value)];
  known.known_actor_ids.clear();
  known.known_count = 0;
}

void City::initialize_known_arms_dealers(core::PlayerId player_id) {
  assert(player_id.value >= 0 && player_id.value < NUMBER_OF_PLAYERS);

  auto& known = known_arms_dealers_[static_cast<size_t>(player_id.value)];
  known.known_actor_ids.clear();
  known.known_count = 0;

  // Find all actors in OTHERS category (mafia + arms dealers)
  std::vector<ActorId> others_actor_ids;
  for (const auto& [actor_id, actor_ptr] : actors_) {
    ActorType type = actor_ptr->type();
    if (type == ActorType::MAFIA || type == ActorType::ARMS_DEALER) {
      others_actor_ids.push_back(actor_id);
    }
  }

  // Add first 3 actors from OTHERS category as initially known
  // (This includes both mafia and arms dealers in actor ID order)
  int num_to_add = std::min(3, static_cast<int>(others_actor_ids.size()));
  for (int i = 0; i < num_to_add && i < 10; ++i) {
    ActorType type =
        actor_by_id(static_cast<uint8_t>(others_actor_ids[i]))->type();
    // Only track arms dealers (mafia is always visible)
    if (type == ActorType::ARMS_DEALER || type == ActorType::MAFIA) {
      known.known_actor_ids.push_back(others_actor_ids[i]);
    }
  }
  known.known_count = static_cast<uint8_t>(known.known_actor_ids.size());
}

std::vector<const Actor*> City::all_arms_dealers() const {
  std::vector<const Actor*> result;
  for (const auto& [id, actor] : actors_) {
    if (actor->type() == ActorType::ARMS_DEALER) {
      result.push_back(actor.get());
    }
  }
  return result;
}
