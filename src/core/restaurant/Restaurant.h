/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "core/character/Staff.h"
#include "core/item/Item.h"
#include "core/restaurant/Menu.h"
#include "core/restaurant/RestaurantEconomy.h"
#include "core/restaurant/RestaurantInventory.h"
#include "core/restaurant/RestaurantLayout.h"
#include "core/restaurant/RestaurantStats.h"
#include "core/restaurant/StaffEventData.h"
#include "core/time/TimeOfDay.h"
#include "engine/Random.h"

namespace core::food {
struct PlayerRecipes;
class Recipe;
class Pizza;
} // namespace core::food

class Property;

constexpr int MAXIMUM_STAFF_PER_CATEGORY = 6;

enum class OpenImpossibleReason {
  Success,
  NeedStaff,
  NeedOven,
  NeedMenu,
};

enum class HiringResult {
  Success,
  NoSlotsAvailable,
  AlreadyHasManager,
};

enum class ManagerReaction {
  DoNothing,
  CallMe,
  ActYourself,
};

// The behavior decides how to respond to random staff events;
// merciless & severe are the last choice of the popup that you would get
// without a manager (harsh, -13 motivation for ilness, -6 for being late),
// strict & moderate are neutral and are the middle choice (-5 for ilness, -2
// for being late) and cooperate and friendly are nice and give +3 for being
// ill and +2 for being late.
enum class ManagerBehavior {
  Merciless,
  Severe,
  Strict,
  Moderate,
  Cooperative,
  Friendly,
  Warmly,
};

struct ProductionScreenData {
  int order_count{0};
  std::vector<int> oven_capacities;
  int fulfillment_rate{0};
  int fulfilled_orders{0};
};

struct ManagerSettings {
  bool shopping_if_needed{false};
  bool change_prices{false};
  bool overdraw_account{false};
  ManagerReaction turnover_decrease{ManagerReaction::DoNothing};
  ManagerReaction turnover_increase{ManagerReaction::DoNothing};
  ManagerReaction supply_shortage{ManagerReaction::DoNothing};
  ManagerReaction staff_shortage{ManagerReaction::DoNothing};
  ManagerBehavior behavior_towards_staff{ManagerBehavior::Merciless};
};

// Per-category timing metrics for the staff screen.
struct ServiceTimeData {
  // Time per pizza (minutes)
  float avg_oven_time{0.0f};
  float avg_chef_time{0.0f};
  float avg_waiter_time{0.0f};

  int service_time_minutes{0};
  float avg_eating_time{0.0f};
  float avg_seating_time{0.0f};

  int chef_count{0};
  int waiter_count{0};
  int oven_count{0};

  int total_chef_speed{0};
  int total_waiter_speed{0};
  int total_oven_capacity{0};
};

struct KitchenEquipmentStats {
  int oven_count{0};
  float avg_bake_time{0.0f};
  int utilization_percent{0};
  Money total_value{0};
  int avg_quality{0};
  int total_capacity{0};
  int capacity_per_day{0};
};

struct FurnitureStats {
  int seating_utilization{0};
  Money total_value{0};
  int avg_quality{0};
  int seat_count{0};
  int customers_per_day{0};
};

struct PizzaQualityFactors {
  int ingredients{0};
  int chef{0};
  int oven{0};
  int contamination{0};

  std::array<int, 4> as_array() const {
    return {ingredients, chef, oven, contamination};
  }
};

struct RestaurantPopularity {
  int WaitingTime = 0;
  int PizzaPrices = 0;
  int RecipesOnMenu = 0;
  int Advertising = 0;
  int StarRating = 0;
  int Furnishing = 0;
  int DemandFulfillment = 0;
  int UnexpectedEvents = 0;

  std::array<int, 8> as_array() const {
    return {WaitingTime, PizzaPrices, RecipesOnMenu,     Advertising,
            StarRating,  Furnishing,  DemandFulfillment, UnexpectedEvents};
  }

  int total() const {
    return WaitingTime + PizzaPrices + RecipesOnMenu + Advertising +
           StarRating + Furnishing + DemandFulfillment + UnexpectedEvents;
  }
};

class Restaurant {
public:
  Restaurant(Property& property, Size kitchen_size, core::RestaurantId id,
             std::string name);
  ~Restaurant();
  core::RestaurantId id() const { return restaurant_id_; }
  // Derived open state: true when player wants open, not force-closed,
  // and within business hours
  bool is_open(const core::time::TimeOfDay& now) const;
  bool is_within_business_hours(const core::time::TimeOfDay& now) const;

  // Player intent
  bool wants_open() const { return wants_open_; }
  void wants_open(bool wants);
  bool ever_opened() const { return ever_opened_; }
  void set_ever_opened() { ever_opened_ = true; } // for save/load

  // Forced closure (staff screw-ups, mayor)
  void force_close(int days);
  bool is_force_closed() const { return forced_days_remaining_ > 0; }
  int forced_days_remaining() const { return forced_days_remaining_; }
  Size size() const;
  Size kitchen_size() const;
  void kitchen_size(Size kitchen_size);
  const std::unique_ptr<RestaurantLayout>& layout() const;
  void layout(std::unique_ptr<RestaurantLayout> layout);
  HiringResult add_staff(Staff&);
  bool fire_staff(core::character::JobRole role, int slot_index);
  Staff* staff_at(core::character::JobRole role, int slot_index);
  std::array<std::optional<Staff>, MAXIMUM_STAFF_PER_CATEGORY>& cooks();
  std::array<std::optional<Staff>, MAXIMUM_STAFF_PER_CATEGORY>& waiters();
  std::optional<Staff>& manager();
  const std::array<std::optional<Staff>, MAXIMUM_STAFF_PER_CATEGORY>&
  cooks() const {
    return cooks_;
  }
  const std::array<std::optional<Staff>, MAXIMUM_STAFF_PER_CATEGORY>&
  waiters() const {
    return waiters_;
  }
  const std::optional<Staff>& manager() const { return manager_; }
  OpenImpossibleReason can_open() const;
  const std::string& name() const;
  core::restaurant::Menu& menu() { return menu_; }
  const core::restaurant::Menu& menu() const { return menu_; }
  Property& property() { return property_; }
  const Property& property() const { return property_; }
  int range() const { return economy_.range_tiles(); }
  int range_raw() const { return economy_.range_raw(); }
  void range_raw(int raw) { economy_.range_raw(raw); }
  RestaurantPopularity popularity() const { return popularity_; }
  int total_popularity() const;

  // Jukebox/TV settings (0 = none, 1-6 = demographic targeting)
  int jukebox_music() const { return economy_.jukebox_music(); }
  void jukebox_music(int music) { economy_.jukebox_music(music); }
  int tv_channel() const { return economy_.tv_channel(); }
  void tv_channel(int channel) { economy_.tv_channel(channel); }

  int pizza_quality_rating() const { return pizza_quality_rating_; }
  void set_pizza_quality_rating(int val) { pizza_quality_rating_ = val; }
  const PizzaQualityFactors& pizza_quality_factors() const {
    return pizza_quality_factors_;
  }
  void set_pizza_quality_factors(const PizzaQualityFactors& f) {
    pizza_quality_factors_ = f;
  }

  // Sabotage: apply contamination + reputation damage for a sabotage item.
  void apply_sabotage(core::item::Item item);

  int sabotage_contamination() const { return sabotage_contamination_; }
  void set_sabotage_contamination(int val) { sabotage_contamination_ = val; }

  // Direct access to reputation damage slots for save/load
  std::array<int, 4>& event_reputation_penalties() {
    return event_reputation_penalties_;
  }
  const std::array<int, 4>& event_reputation_penalties() const {
    return event_reputation_penalties_;
  }
  std::array<int, 4>& event_reputation_recovery() {
    return event_reputation_recovery_;
  }
  const std::array<int, 4>& event_reputation_recovery() const {
    return event_reputation_recovery_;
  }

  // Recalculate furniture appeal and competitive strength.
  // Call when menu or furniture changes outside the daily pipeline.
  void update_demographics(const core::food::PlayerRecipes& recipes);

  Popularity
  calculate_menu_popularity(const core::food::PlayerRecipes& recipes) const;
  Popularity calculate_furniture_appeal_popularity() const;

  core::restaurant::RestaurantInventory& inventory() { return inventory_; }
  const core::restaurant::RestaurantInventory& inventory() const {
    return inventory_;
  }

  Money purchase_ingredients(core::food::IngredientType type, int quantity_kg,
                             int quality, Money price_per_kg);
  Money purchase_furniture(const class Furniture* furniture, int quantity);

  core::restaurant::RestaurantStats& stats() { return stats_; }
  const core::restaurant::RestaurantStats& stats() const { return stats_; }

  RestaurantEconomy& economy() { return economy_; }
  const RestaurantEconomy& economy() const { return economy_; }

  // Each reputation slot decays toward 0 by its recovery rate per day
  void process_daily_reputation_deltas();

  // Returns the last non-zero value from the 4 event penalty slots
  int get_active_reputation_penalty() const;

  // Add a temporary reputation penalty that heals over time.
  // Finds first empty slot, or overwrites slot 0 if all full.
  void add_reputation_damage(int penalty, int recovery_per_day);

  int calculate_weighted_average_oven_time() const;

  // Formula: max(chef_time, oven_time) + waiter_time + demographic_eating_time
  int calculate_service_time_minutes(ConsumerType demographic) const;
  // Weighted average across demographics using actual customer mix
  int calculate_service_time_minutes() const;

  // Throughput for a 10-minute block: (10 * seat_count) / service_time
  int calculate_block_throughput(ConsumerType demographic) const;
  int calculate_block_throughput() const;

  int count_ovens() const;
  int count_cooks() const;
  int count_waiters() const;
  int count_menu_items() const;

  int calculate_daily_oven_throughput() const;
  int calculate_daily_chef_throughput() const;
  int calculate_daily_waiter_throughput() const;

  // (seat_count * 720) / avg_seating_time
  int calculate_seating_capacity(const ServiceTimeData* service_data) const;

  struct Occupancy {
    int guests;
    int seats;
  };
  // How many guests are concurrently seated vs total linked seats
  Occupancy guest_occupancy() const;

  ProductionScreenData get_production_screen_data() const;

  // All timing metrics used by staff screen and seating capacity calc
  ServiceTimeData calculate_service_time_data() const;

  KitchenEquipmentStats calculate_kitchen_equipment_stats() const;
  FurnitureStats calculate_furniture_stats() const;

  // Max pizzas makeable with current stock. Iteratively finds the limiting
  // ingredient, scales down affected pizzas, and repeats.
  int calculate_stock_capacity(const core::food::PlayerRecipes& recipes) const;

  // Operating hours
  core::time::TimeOfDay opening_time() const { return opening_time_; }
  core::time::TimeOfDay closing_time() const { return closing_time_; }
  void set_opening_time(core::time::TimeOfDay time) { opening_time_ = time; }
  void set_closing_time(core::time::TimeOfDay time) { closing_time_ = time; }
  int operating_blocks_per_day() const {
    return core::time::TimeOfDay::blocks_between(opening_time_, closing_time_);
  }

  core::food::IngredientDemand calculate_daily_ingredient_demand_grams(
      const core::food::PlayerRecipes& recipes) const;

  static std::vector<std::pair<core::food::IngredientType, int>>
  get_recipe_requirements_grams(const core::food::Recipe& recipe);

  // Sets AI tier/capacity from the player's character stats
  void init_ai_economy(const class Player& player);
  int ai_tier() const { return ai_tier_; }
  int ai_capacity_target() const { return ai_capacity_target_; }
  int ai_profit_days() const { return ai_profit_days_; }
  void ai_increment_profit_days() { ai_profit_days_++; }

  void start_of_day(class GameState& game_state);
  void end_of_day(class GameState& game_state);
  // Returns (and clears) player-facing staff event dialogs queued during
  // end_of_day.
  std::vector<core::restaurant::StaffEventData> take_pending_staff_events() {
    return std::move(pending_staff_events_);
  }
  void eod_batch_order_fulfillment(class GameState& game_state);
  void calculate_daily_pizza_sales_distribution(GameState& game_state);

  int get_predicted_daily_orders(core::RecipeId recipe_id) const;

  // 0-100, derived from menu/staff/ingredients. Drives star ratings.
  void calculate_pizza_quality_rating(const core::food::PlayerRecipes& recipes);

  // Award/remove stars based on pizza_quality_rating (mayor visit).
  // Returns true if a star was gained, false if lost, nullopt if unchanged.
  std::optional<bool> update_star_rating();

  int star_rating() const { return star_rating_; }
  void star_rating(int v) { star_rating_ = v; } // for save/load

  // Returns (and clears) a pending inspection event queued during end_of_day.
  // Returns true if it is the first-ever inspection, false for subsequent.
  // Returns nullopt if no inspection is pending.
  std::optional<bool> take_pending_inspection() {
    return std::exchange(pending_inspection_, std::nullopt);
  }
  // for save/load
  void inspection_days_remaining(int v) { inspection_days_remaining_ = v; }
  int inspection_days_remaining() const { return inspection_days_remaining_; }
  void first_inspection_pending(bool v) { first_inspection_pending_ = v; }
  bool first_inspection_pending() const { return first_inspection_pending_; }

  static constexpr int NUM_INGREDIENT_CATEGORIES = 4;

  // Manager shopping: preferred supplier ID per ingredient category.
  // 0xFF = none set (manager picks first available supplier).
  std::array<std::optional<int>, NUM_INGREDIENT_CATEGORIES>&
  preferred_suppliers() {
    return preferred_suppliers_;
  }
  const std::array<std::optional<int>, NUM_INGREDIENT_CATEGORIES>&
  preferred_suppliers() const {
    return preferred_suppliers_;
  }

  // Manager staff shortage augmentation: tracks which role the manager
  // helped with yesterday (0 = none, 1 = cook, 2 = waiter).
  // TODO: Change to an enum
  int manager_augmenting() const { return manager_augmenting_; }
  void set_manager_augmenting(int role) { manager_augmenting_ = role; }

  auto& top_ingredients() { return top_ingredients_; }
  const auto& top_ingredients() const { return top_ingredients_; }

  // Trend appeal bonus (-125 to +125) from top 4 trending ingredients
  int calculate_trend_appeal(const class City& city) const;

  // Calculates all 8 popularity components. Component 7 (UnexpectedEvents)
  // flows into total_popularity() => competitive_strength => customer demand.
  void calculate_popularity(const class Player& player, const class City& city,
                            const class District& district);

  // Re-reads reputation slots into popularity_.UnexpectedEvents so mid-day
  // changes (e.g. forced closure) are captured before daily decay.
  int sync_reputation_and_total_popularity();

  int service_time_minutes() const { return service_time_minutes_; }
  int fulfillment_rate() const { return fulfillment_rate_; }
  Money last_weekly_profit() const { return last_weekly_profit_; }
  void last_weekly_profit(Money m) { last_weekly_profit_ = m; }
  ManagerSettings& manager_settings() { return manager_settings_; };
  const ManagerSettings& manager_settings() const { return manager_settings_; };

  // Mafia protection racket timer.
  // nullopt = never contacted, 1 = expired (trigger visit), >1 = counting down.
  std::optional<int> protection_timer() const { return protection_timer_; }
  void protection_timer(std::optional<int> t) { protection_timer_ = t; }
  void decrement_protection_timer() {
    if (protection_timer_.has_value() && *protection_timer_ > 1) {
      (*protection_timer_)--;
    }
  }

private:
  int calculate_average_cook_quality() const;
  int calculate_average_oven_quality() const;
  int calculate_ingredient_quality_for_pizza(
      const core::food::Pizza& pizza) const;
  Property& property_;
  bool wants_open_{false};
  bool ever_opened_{false};
  Size kitchen_size_;
  core::RestaurantId restaurant_id_{0};
  std::string name_;
  std::unique_ptr<RestaurantLayout> layout_;
  // Mayor awards stars when pizza_quality > star_rating * 16 + 24,
  // removes one when pizza_quality < star_rating * 16 + 8.
  int star_rating_{0};
  std::array<std::optional<Staff>, MAXIMUM_STAFF_PER_CATEGORY> cooks_;
  std::array<std::optional<Staff>, MAXIMUM_STAFF_PER_CATEGORY> waiters_;
  std::optional<Staff> manager_;
  core::restaurant::Menu menu_;
  core::restaurant::RestaurantInventory inventory_;
  core::restaurant::RestaurantStats stats_;
  RestaurantEconomy economy_;
  core::time::TimeOfDay opening_time_{9, 0};
  core::time::TimeOfDay closing_time_{21, 0};
  ManagerSettings manager_settings_;
  std::optional<int> protection_timer_;
  std::vector<core::restaurant::StaffEventData> pending_staff_events_;

  // Mayor inspection countdown. Ticks down each open day.
  // When it reaches 0 an inspection is queued.
  int inspection_days_remaining_{engine::random::rand_max_n(20) + 12};
  bool first_inspection_pending_{true};
  std::optional<bool> pending_inspection_; // true=first, false=regular

  int pizza_quality_rating_{0};
  PizzaQualityFactors pizza_quality_factors_{};

  // Sabotage contamination level, clamped to [-120, +120].
  // Divided by 4 to produce the contamination quality factor.
  // Original game bug: never decays. We fix this when fix_legacy_bugs is on.
  int sabotage_contamination_{0};

  // Top ingredient per category (veg, meat, fruit, cheese)
  std::array<std::optional<int>, NUM_INGREDIENT_CATEGORIES> top_ingredients_{
      std::nullopt, std::nullopt, std::nullopt, std::nullopt};

  // 8 popularity component scores; total feeds into economy_ for demand
  RestaurantPopularity popularity_{};

  int service_time_minutes_{0}; // max(chef, oven) + waiter
  int fulfillment_rate_{0};     // can-make vs demanded, 0-100%

  // Temporary reputation penalties from events (sabotage, mistakes, etc.)
  // that decay toward 0 each day at the corresponding recovery rate.
  std::array<int, 4> event_reputation_penalties_{};
  std::array<int, 4> event_reputation_recovery_{};

  // Forecasted orders per recipe, calculated at day start
  std::unordered_map<core::RecipeId, int, core::RecipeId::Hasher>
      predicted_daily_orders_;

  // Forced closure countdown (health inspection, damage, etc.)
  // Decremented each start_of_day; at 0 the restaurant is eligible to open.
  int forced_days_remaining_{0};

  // Manager preferred supplier ID per ingredient category (0=veg,1=meat,
  // 2=fruit, 3=cheese). 0xFF means no preference - use first available.
  std::array<std::optional<int>, NUM_INGREDIENT_CATEGORIES>
      preferred_suppliers_{std::nullopt, std::nullopt, std::nullopt,
                           std::nullopt};

  // Manager augmentation flag: which role the manager helped with today
  // (0 = none, 1 = cook, 2 = waiter). Read and cleared next morning.
  int manager_augmenting_{0};

  // 0 = human, 1-4 = AI tier level
  int ai_tier_{0};
  int ai_capacity_target_{0};
  int ai_profit_days_{0};

  Money last_weekly_profit_;
};
