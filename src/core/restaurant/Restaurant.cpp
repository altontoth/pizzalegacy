/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/Restaurant.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include "core/Demographics.h"
#include "core/character/Staff.h"
#include "core/city/City.h"
#include "core/city/District.h"
#include "core/city/Property.h"
#include "core/events/LeventId.h"
#include "core/finance/Money.h"
#include "core/food/IngredientCategory.h"
#include "core/food/IngredientType.h"
#include "core/restaurant/AIEconomy.h"
#include "core/restaurant/DemographicTiming.h"
#include "core/restaurant/ManagerFeedback.h"
#include "core/restaurant/ManagerRoutine.h"
#include "core/restaurant/Menu.h"
#include "core/restaurant/PizzaDemand.h"
#include "core/restaurant/StaffEvents.h"
#include "core/state/GameState.h"
#include "core/status/StatusHub.h"
#include "core/util/ConfigManager.h"
#include "engine/Fatal.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/Random.h"

Restaurant::Restaurant(Property& property, Size kitchen_size,
                       core::RestaurantId id, std::string name)
    : property_(property), kitchen_size_(kitchen_size), restaurant_id_(id),
      name_(std::move(name)), layout_(std::make_unique<RestaurantLayout>(
                                  property.size(), kitchen_size)) {}

Restaurant::~Restaurant() = default;

void Restaurant::init_ai_economy(const Player& player) {
  ai_tier_ = engine::random::rand_max_n(4) + 1;

  ai_capacity_target_ = ai_tier_ * 2 + 6;

  auto cooking = player.character().cooking();
  auto intelligence = player.character().intelligence();

  for (auto ct : ALL_CONSUMER_TYPES) {
    int mf = (cooking > 0 ? engine::random::rand_max_n(cooking) : 0) + 20;
    economy_.menu_factor()[ct] = std::min(mf, 255);

    int fa =
        (intelligence > 0 ? engine::random::rand_max_n(intelligence) : 0) + 20;
    economy_.furniture_appeal()[ct] = std::min(fa, 255);
  }
}

bool Restaurant::is_open(const core::time::TimeOfDay& now) const {
  return wants_open_ && forced_days_remaining_ == 0 &&
         is_within_business_hours(now);
}

bool Restaurant::is_within_business_hours(
    const core::time::TimeOfDay& now) const {
  return now >= opening_time_ && now < closing_time_;
}

void Restaurant::wants_open(bool wants) {
  wants_open_ = wants;
  if (wants) {
    ever_opened_ = true;
  }
}

void Restaurant::force_close(int days) { forced_days_remaining_ = days; }

void Restaurant::update_demographics(const core::food::PlayerRecipes& recipes) {
  property_.calculate_demographics(this);
  economy_.calculate_furniture_appeal(*this);
  economy_.calculate_competitive_strength(*this, &recipes);
}

void Restaurant::process_daily_reputation_deltas() {
  // 4 reputation damage slots that decay toward 0 each day
  for (int i = 0; i < 4; i++) {
    auto damage_val = event_reputation_penalties_[i];
    auto decay_rate = event_reputation_recovery_[i];

    int abs_damage = std::abs(damage_val);
    int abs_decay = std::abs(decay_rate);

    if (abs_damage > abs_decay) {
      event_reputation_penalties_[i] += decay_rate;
    }
    else {
      event_reputation_penalties_[i] = 0;
      event_reputation_recovery_[i] = 0;
    }
  }
}

int Restaurant::get_active_reputation_penalty() const {
  // Returns the highest-index non-zero penalty
  for (int i = 3; i >= 0; i--) {
    if (event_reputation_penalties_[i] != 0) {
      return event_reputation_penalties_[i];
    }
  }
  return 0;
}

void Restaurant::add_reputation_damage(int penalty, int recovery_per_day) {
  // Find first empty slot; if all full, silently fail
  for (int i = 0; i < 4; i++) {
    if (event_reputation_penalties_[i] == 0) {
      event_reputation_penalties_[i] = penalty;
      event_reputation_recovery_[i] = recovery_per_day;
      Logger::info("[Restaurant] {} - Added reputation damage: {} (recovery: "
                   "{}/day) in slot {}",
                   name_, penalty, recovery_per_day, i);
      return;
    }
  }

  Logger::warn(
      "[Restaurant] {} - Cannot add reputation damage {}: all 4 slots full",
      name_, penalty);
}

void Restaurant::apply_sabotage(core::item::Item item) {
  using core::item::Item;

  struct SabotageEffect {
    int contamination_delta;
    int reputation_penalty;
    int decay_rate;
  };

  SabotageEffect effect{};
  switch (item) {
  case Item::SmellyCheese:
    effect = {
        .contamination_delta = -30, .reputation_penalty = -20, .decay_rate = 4};
    break;
  case Item::Laxatives:
    effect = {
        .contamination_delta = -40, .reputation_penalty = -30, .decay_rate = 5};
    break;
  case Item::Rats:
    effect = {
        .contamination_delta = -60, .reputation_penalty = -40, .decay_rate = 8};
    break;
  case Item::StinkBomb:
    effect = {
        .contamination_delta = 0, .reputation_penalty = -60, .decay_rate = 10};
    break;
  default:
    Logger::warn("[Restaurant] {} - apply_sabotage called with non-sabotage "
                 "item: {}",
                 name_, core::item::to_string(item));
    return;
  }

  if (effect.contamination_delta != 0) {
    int new_val = sabotage_contamination_ + effect.contamination_delta;
    sabotage_contamination_ = std::clamp(new_val, -120, 120);
  }

  add_reputation_damage(effect.reputation_penalty, effect.decay_rate);

  Logger::info("[Restaurant] {} - Sabotaged with {}: contamination={}, "
               "reputation penalty={}",
               name_, core::item::to_string(item), sabotage_contamination_,
               effect.reputation_penalty);
}

Size Restaurant::size() const { return property_.size(); }

Size Restaurant::kitchen_size() const { return kitchen_size_; }

void Restaurant::kitchen_size(Size kitchen_size) {
  kitchen_size_ = kitchen_size;
}

const std::string& Restaurant::name() const { return name_; }

const std::unique_ptr<RestaurantLayout>& Restaurant::layout() const {
  return layout_;
}

void Restaurant::layout(std::unique_ptr<RestaurantLayout> layout) {
  layout_ = std::move(layout);
}

OpenImpossibleReason Restaurant::can_open() const {
  return OpenImpossibleReason::Success;
}

HiringResult Restaurant::add_staff(Staff& staff) {
  if (staff.job_role() == core::character::JobRole::MANAGER) {
    if (!manager_) {
      manager_ = staff;
    }
    else {
      return HiringResult::AlreadyHasManager;
    }
  }
  else {
    bool added = false;
    for (auto& slot : staff.job_role() == core::character::JobRole::CHEF
                          ? cooks_
                          : waiters_) {
      if (!slot) {
        slot = staff;
        added = true;
        break;
      }
    }
    if (!added) {
      return HiringResult::NoSlotsAvailable;
    }
  }
  return HiringResult::Success;
}

Staff* Restaurant::staff_at(core::character::JobRole role, int slot_index) {
  if (role == core::character::JobRole::MANAGER) {
    return manager_.has_value() ? &*manager_ : nullptr;
  }
  auto& arr = (role == core::character::JobRole::CHEF) ? cooks_ : waiters_;
  if (slot_index >= 0 && slot_index < static_cast<int>(arr.size())) {
    return arr[slot_index].has_value() ? &*arr[slot_index] : nullptr;
  }
  return nullptr;
}

bool Restaurant::fire_staff(core::character::JobRole role, int slot_index) {
  if (role == core::character::JobRole::MANAGER) {
    if (manager_.has_value()) {
      manager_.reset();
      return true;
    }
    return false;
  }

  auto& staff_array =
      (role == core::character::JobRole::CHEF) ? cooks_ : waiters_;

  if (slot_index >= 0 && slot_index < static_cast<int>(staff_array.size())) {
    if (staff_array[slot_index].has_value()) {
      // Shift remaining staff left to fill the gap
      for (int i = slot_index; i < static_cast<int>(staff_array.size()) - 1;
           ++i) {
        staff_array[i] = std::move(staff_array[i + 1]);
      }
      staff_array.back().reset();
      return true;
    }
  }
  return false;
}

std::array<std::optional<Staff>, MAXIMUM_STAFF_PER_CATEGORY>&
Restaurant::cooks() {
  return cooks_;
}

std::array<std::optional<Staff>, MAXIMUM_STAFF_PER_CATEGORY>&
Restaurant::waiters() {
  return waiters_;
}

std::optional<Staff>& Restaurant::manager() { return manager_; }

int Restaurant::count_ovens() const {
  if (!layout_) {
    return 0;
  }

  int count = 0;
  for (const auto& pf : layout_->placed_kitchen_furniture()) {
    if (pf && pf->furniture()->category() == FurnitureCategory::KITCHENWARE) {
      count++;
    }
  }
  return count;
}

int Restaurant::count_cooks() const {
  int count = 0;
  for (const auto& cook : cooks_) {
    if (cook.has_value()) {
      count++;
    }
  }
  return count;
}

int Restaurant::count_waiters() const {
  int count = 0;
  for (const auto& waiter : waiters_) {
    if (waiter.has_value()) {
      count++;
    }
  }
  return count;
}

int Restaurant::count_menu_items() const {
  return static_cast<int>(menu_.entries().size());
}

int Restaurant::calculate_daily_oven_throughput() const {
  // sum((capacity * operating minutes) / bake_time)
  if (!layout_) {
    return 0;
  }

  const int operating_minutes = operating_minutes_per_day();
  int total_throughput = 0;
  for (const auto& pf : layout_->placed_kitchen_furniture()) {
    if (pf && pf->furniture()->category() == FurnitureCategory::KITCHENWARE) {
      int bake_time = pf->furniture()->bake_time();
      int capacity = pf->furniture()->capacity();
      if (bake_time > 0 && capacity > 0) {
        total_throughput += (capacity * operating_minutes) / bake_time;
      }
    }
  }

  return total_throughput;
}

int Restaurant::calculate_daily_chef_throughput() const {
  int total = 0;
  for (const auto& cook : cooks_) {
    if (cook.has_value() && !cook->is_away()) {
      int contribution = cook->pizzas_per_day();
      if (cook->is_late()) {
        contribution /= 2;
      }
      total += contribution;
    }
  }
  return total;
}

int Restaurant::calculate_daily_waiter_throughput() const {
  int total = 0;
  for (const auto& waiter : waiters_) {
    if (waiter.has_value() && !waiter->is_away()) {
      int contribution = waiter->pizzas_per_day();
      if (waiter->is_late()) {
        contribution /= 2;
      }
      total += contribution;
    }
  }
  return total;
}

Money Restaurant::calculate_electricity_cost() const {
  int oven_cap = calculate_daily_oven_throughput();
  return Money{static_cast<int64_t>(oven_cap) * 300};
}

Money Restaurant::calculate_slot_machine_income() const {
  int young_adults = economy_.customer_counts()[ConsumerType::YoungPeople];
  Money income{0};
  for (const auto& pf : layout_->placed_furniture()) {
    if (!pf) {
      continue;
    }
    // Original formula was (8 - category_value), this is more readable.
    int mult;
    switch (pf->furniture()->category()) {
    case FurnitureCategory::GAMES:
      mult = 3;
      break;
    case FurnitureCategory::SLOT_MACHINES:
      mult = 2;
      break;
    default:
      continue;
    }
    int rand1 = young_adults > 0 ? engine::random::rand_max_n(young_adults) : 0;
    int rand2 = engine::random::rand_max_n(20);
    income += Money{static_cast<int64_t>(mult * (rand1 + rand2)) * 100};
  }
  return income;
}

int Restaurant::calculate_seating_capacity(
    const ServiceTimeData* service_data) const {
  // seating_capacity = (seat_count * operating minutes) / avg_seating_time
  assert(service_data);

  int seat_count = 0;
  if (layout_) {
    for (const auto& pf : layout_->placed_furniture()) {
      if (pf && pf->furniture()->category() == FurnitureCategory::CHAIR &&
          pf->linked_furniture_id().has_value()) {
        seat_count += pf->furniture()->capacity();
      }
    }
  }

  int seating_time = static_cast<int>(service_data->avg_seating_time);

  int capacity = (seating_time > 0 && seat_count > 0)
                     ? (operating_minutes_per_day() * seat_count) / seating_time
                     : 0;

  return capacity;
}

Restaurant::Occupancy Restaurant::guest_occupancy() const {
  int seat_count = 0;
  if (layout_) {
    for (const auto& pf : layout_->placed_furniture()) {
      if (pf && pf->furniture()->category() == FurnitureCategory::CHAIR &&
          pf->linked_furniture_id().has_value()) {
        ++seat_count;
      }
    }
  }
  if (seat_count == 0) {
    return {.guests = 0, .seats = 0};
  }

  int total_daily = 0;
  for (auto ct : ALL_CONSUMER_TYPES) {
    total_daily += economy_.customer_counts()[ct];
  }

  const auto service_data = calculate_service_time_data();
  const float avg_seating_time = std::max(service_data.avg_seating_time, 1.0f);
  const float current_demand = static_cast<float>(total_daily) *
                               avg_seating_time /
                               static_cast<float>(operating_minutes_per_day());

  const int guests = std::min(
      static_cast<int>(std::lround(current_demand + 0.5f)), seat_count);
  return {.guests = guests, .seats = seat_count};
}

CapacityAnalysis Restaurant::calculate_capacity_analysis(
    const core::food::PlayerRecipes& recipes,
    const ServiceTimeData& service_data) const {
  CapacityAnalysis c;

  // demand is current per demographic customer counts
  for (const auto& count : economy_.customer_counts()) {
    c.demand += count;
  }

  c.seating_cap = calculate_seating_capacity(&service_data);
  c.stock_cap = calculate_stock_capacity(recipes);
  c.chef_cap = calculate_daily_chef_throughput();
  c.oven_cap = calculate_daily_oven_throughput();
  c.waiter_cap = calculate_daily_waiter_throughput();

  // Cascade: each stage limits the next.
  c.d_seating = std::min(c.demand, c.seating_cap);
  c.d_stock = std::min(c.d_seating, c.stock_cap);
  c.d_chef = std::min(c.d_stock, c.chef_cap);
  c.d_oven = std::min(c.d_chef, c.oven_cap);

  // Utilization is each stage's incoming demand divided by its capacity.
  auto util_pct = [](int numerator, int denominator) -> int {
    if (denominator <= 0) {
      return 0;
    }
    return std::clamp((numerator * 100) / denominator, 0, 100);
  };

  c.seating_util = util_pct(c.demand, c.seating_cap);
  c.stock_util = util_pct(c.d_seating, c.stock_cap);
  c.chef_util = util_pct(c.d_stock, c.chef_cap);
  c.oven_util = util_pct(c.d_chef, c.oven_cap);
  c.waiter_work = util_pct(c.d_oven, c.waiter_cap);

  return c;
}

ProductionScreenData Restaurant::get_production_screen_data(
    const core::food::PlayerRecipes& recipes) const {
  ProductionScreenData data;

  auto service_data = calculate_service_time_data();
  auto analysis = calculate_capacity_analysis(recipes, service_data);

  // Money box (bills): demand * avg_seating_time / operating minutes, cap 20.
  if (service_data.avg_seating_time > 0.0f) {
    float raw = static_cast<float>(analysis.demand) *
                service_data.avg_seating_time /
                static_cast<float>(operating_minutes_per_day());
    data.bill_count = std::min(static_cast<int>(raw), 20);
  }

  data.oven_util = analysis.oven_util;

  // South (delivered pizzas)
  int base_baked = (analysis.d_oven < 160) ? analysis.d_oven / 16 : 10;
  data.baked_pizza_count = (base_baked * analysis.waiter_work) / 100;

  // North (order slips)
  int base_slips = (analysis.d_seating < 160) ? analysis.d_seating / 16 : 10;
  int max_util = std::max(analysis.stock_util, analysis.chef_util);
  data.order_count =
      std::min(((base_slips * max_util * max_util) / 10000) + 1, 11);

  // East (ovens + pizza stacks)
  if (layout_) {
    int oven_count = 0;
    for (const auto& pf : layout_->placed_kitchen_furniture()) {
      if (pf && pf->furniture()->category() == FurnitureCategory::KITCHENWARE) {
        int capacity = pf->furniture()->capacity();
        int scaled_capacity;
        if (analysis.oven_util > 90) {
          scaled_capacity = capacity;
        }
        else if (analysis.oven_util == 0) {
          scaled_capacity = 0;
        }
        else {
          scaled_capacity = (capacity * analysis.oven_util) / 100;
        }
        data.oven_capacities.push_back(scaled_capacity);
        oven_count++;
        if (oven_count >= 4) {
          break;
        }
      }
    }
  }

  return data;
}

ServiceTimeData Restaurant::calculate_service_time_data() const {
  ServiceTimeData data;

  const float operating_minutes =
      static_cast<float>(operating_minutes_per_day());

  // Per-demographic eating times: Children, YoungAdults, BlueCollar,
  // WhiteCollar, Business, Seniors
  constexpr int DEMOGRAPHIC_EATING_TIMES[6] = {12, 10, 8, 10, 7, 15};

  // Weighted average oven time: sum(capacity * bake_time) / sum(capacity)
  if (layout_) {
    float total_oven_minutes = 0.0f;
    float total_capacity = 0.0f;

    for (const auto& pf : layout_->placed_kitchen_furniture()) {
      if (pf && pf->furniture()->category() == FurnitureCategory::KITCHENWARE) {
        int bake_time = pf->furniture()->bake_time();
        int capacity = pf->furniture()->capacity();

        if (bake_time > 0 && capacity > 0) {
          total_oven_minutes += static_cast<float>(capacity * bake_time);
          total_capacity += static_cast<float>(capacity);
          data.oven_count++;
        }
      }
    }

    if (total_capacity > 0.0f) {
      data.avg_oven_time = total_oven_minutes / total_capacity;
    }
  }

  data.total_oven_capacity = calculate_daily_oven_throughput();

  // Average chef time: (operating minutes * num_chefs) / total_chef_speed
  {
    float chef_minutes = 0.0f;
    float total_speed = 0.0f;

    for (const auto& cook : cooks_) {
      if (cook.has_value() && !cook->is_away()) {
        chef_minutes += operating_minutes;
        total_speed += static_cast<float>(cook->pizzas_per_day());
        data.chef_count++;
      }
    }

    if (total_speed > 0.0f) {
      data.avg_chef_time = chef_minutes / total_speed;
    }
  }

  data.total_chef_speed = calculate_daily_chef_throughput();

  // Average waiter time (same formula as chefs)
  {
    float waiter_minutes = 0.0f;
    float total_speed = 0.0f;

    for (const auto& waiter : waiters_) {
      if (waiter.has_value() && !waiter->is_away()) {
        waiter_minutes += operating_minutes;
        total_speed += static_cast<float>(waiter->pizzas_per_day());
        data.waiter_count++;
      }
    }

    if (total_speed > 0.0f) {
      data.avg_waiter_time = waiter_minutes / total_speed;
    }
  }

  data.total_waiter_speed = calculate_daily_waiter_throughput();

  // Total service time = max(chef_time, oven_time) + waiter_time
  // Staff/equipment only -- no demographics. Used for popularity Component 0.
  float kitchen_time = std::max(data.avg_chef_time, data.avg_oven_time);
  float service_time_float = kitchen_time + data.avg_waiter_time;
  data.service_time_minutes = static_cast<int>(std::lround(service_time_float));

  if (data.service_time_minutes < 2) {
    data.service_time_minutes = 30;
  }

  // Weighted average eating time (separate from service_time, only used for
  // seating capacity calculations)
  {
    float weighted_sum = 0.0f;
    float total_demand = 0.0f;

    int demo_index = 0;
    for (ConsumerType demographic : ALL_CONSUMER_TYPES) {
      // Add 1 to each demand count to avoid zero weights
      auto demand =
          static_cast<float>(economy_.customer_counts()[demographic] + 1);
      weighted_sum +=
          demand * static_cast<float>(DEMOGRAPHIC_EATING_TIMES[demo_index]);
      total_demand += demand;
      demo_index++;
    }

    if (total_demand > 0.0f) {
      data.avg_eating_time = weighted_sum / total_demand;
    }
  }

  // Total seating time = service_time + eating_time
  data.avg_seating_time =
      static_cast<float>(data.service_time_minutes) + data.avg_eating_time;

  return data;
}

KitchenEquipmentStats Restaurant::calculate_kitchen_equipment_stats() const {
  KitchenEquipmentStats stats;

  if (!layout_) {
    return stats;
  }

  const int operating_minutes = operating_minutes_per_day();

  int quality_sum = 0;
  for (const auto& pf : layout_->placed_kitchen_furniture()) {
    if (pf && pf->furniture()->category() == FurnitureCategory::KITCHENWARE) {
      stats.oven_count++;
      quality_sum += pf->furniture()->quality();
      stats.total_value = stats.total_value + pf->furniture()->price();
      stats.total_capacity += pf->furniture()->capacity();

      int bake_time = pf->furniture()->bake_time();
      int capacity = pf->furniture()->capacity();
      if (bake_time > 0) {
        stats.capacity_per_day += (capacity * operating_minutes) / bake_time;
      }
    }
  }

  if (stats.oven_count > 0) {
    stats.avg_quality = quality_sum / stats.oven_count;
  }

  auto service_data = calculate_service_time_data();
  stats.avg_bake_time = service_data.avg_oven_time;

  if (stats.capacity_per_day > 0) {
    int demand = stats_.yesterday.orders_generated;
    if (demand > 0) {
      stats.utilization_percent = (demand * 100) / stats.capacity_per_day;
      stats.utilization_percent = std::min(stats.utilization_percent, 100);
    }
  }

  return stats;
}

FurnitureStats Restaurant::calculate_furniture_stats() const {
  FurnitureStats stats;

  if (!layout_) {
    return stats;
  }

  int quality_sum = 0;
  int quality_count = 0;

  for (const auto& pf : layout_->placed_furniture()) {
    if (pf) {
      auto category = pf->furniture()->category();

      if (category == FurnitureCategory::CHAIR ||
          category == FurnitureCategory::TABLE) {
        stats.total_value = stats.total_value + pf->furniture()->price();

        if (category == FurnitureCategory::CHAIR) {
          quality_sum += pf->furniture()->quality();
          quality_count++;

          if (pf->linked_furniture_id().has_value()) {
            stats.seat_count += pf->furniture()->capacity();
          }
        }
      }
    }
  }

  if (quality_count > 0) {
    stats.avg_quality = quality_sum / quality_count;
  }

  auto service_data = calculate_service_time_data();
  int seating_time = static_cast<int>(service_data.avg_seating_time);

  if (seating_time > 0 && stats.seat_count > 0) {
    stats.customers_per_day =
        (operating_minutes_per_day() * stats.seat_count) / seating_time;
  }

  if (stats.customers_per_day > 0) {
    int demand = stats_.yesterday.orders_generated;
    if (demand > 0) {
      stats.seating_utilization = (demand * 100) / stats.customers_per_day;
      stats.seating_utilization = std::min(stats.seating_utilization, 100);
    }
  }

  return stats;
}

int Restaurant::calculate_stock_capacity(
    const core::food::PlayerRecipes& recipes) const {
  // Iterative bottleneck detection: finds the limiting ingredient and scales
  // down pizza weights until no ingredient exceeds stock.
  const auto& menu_entries = menu_.entries();
  const int pizza_count = static_cast<int>(menu_entries.size());

  if (pizza_count == 0) {
    return 0;
  }

  // Initialize pizza weights based on predicted daily orders
  std::vector<float> pizza_weights(pizza_count);
  float total_demand = 0.0f;

  for (int i = 0; i < pizza_count; i++) {
    const auto& entry = menu_entries[i];

    int orders = get_predicted_daily_orders(entry.recipe_id);

    if (orders == 0) {
      auto it = stats_.yesterday.pizza_sales.find(entry.recipe_id);
      orders = (it != stats_.yesterday.pizza_sales.end())
                   ? it->second.count_sold
                   : 0;
    }

    pizza_weights[i] = static_cast<float>(orders * 2);
    total_demand += pizza_weights[i];

    Logger::debug("[Stock Capacity] Pizza {} (recipe {}): orders={}, weight={}",
                  i, entry.recipe_id.value, orders, pizza_weights[i]);
  }

  // No demand data: default weight of 100 per pizza
  if (total_demand < 1.0f) {
    Logger::debug("[Stock Capacity] No demand data, using default weights");
    for (int i = 0; i < pizza_count; i++) {
      pizza_weights[i] = 100.0f;
    }
    total_demand = 100.0f * static_cast<float>(pizza_count);
  }

  Logger::debug("[Stock Capacity] Initial total demand: {}", total_demand);

  // Cache ingredient requirements per pizza
  std::vector<core::food::IngredientDemand> pizza_ingredients(pizza_count);
  for (int i = 0; i < pizza_count; i++) {
    const auto* recipe = recipes.find(menu_entries[i].recipe_id);
    if (recipe) {
      auto requirements = get_recipe_requirements_grams(*recipe);
      for (const auto& [ingredient, grams] : requirements) {
        pizza_ingredients[i][ingredient] = grams;
      }
    }
  }

  constexpr int MAX_ITERATIONS = 100;
  int iteration = 0;
  for (; iteration < MAX_ITERATIONS; iteration++) {
    core::food::IngredientArray<float> ingredient_demand{};

    for (int pizza_idx = 0; pizza_idx < pizza_count; pizza_idx++) {
      for (int ing_idx = 0; ing_idx < max_ingredient_count; ing_idx++) {
        int grams_needed = pizza_ingredients[pizza_idx][ing_idx];
        if (grams_needed > 0) {
          ingredient_demand[ing_idx] += grams_needed * pizza_weights[pizza_idx];
        }
      }
    }

    float max_shortfall = 0.0f;
    int bottleneck_ingredient = -1;

    for (int ing_idx = 0; ing_idx < max_ingredient_count; ing_idx++) {
      if (ingredient_demand[ing_idx] > 0) {
        auto stock =
            static_cast<float>(inventory_.all_stocks()[ing_idx].quantity);
        float shortfall = ingredient_demand[ing_idx] - stock;

        if (shortfall > max_shortfall) {
          max_shortfall = shortfall;
          bottleneck_ingredient = ing_idx;
        }
      }
    }

    if (max_shortfall <= 0 || bottleneck_ingredient < 0) {
      Logger::debug("[Stock Capacity] No bottleneck found after {} iterations",
                    iteration);
      break;
    }

    float stock = static_cast<float>(
        inventory_.all_stocks()[bottleneck_ingredient].quantity);

    Logger::debug("[Stock Capacity] Iteration {}: Bottleneck ingredient {} - "
                  "demand: {:.1f}g, stock: {:.1f}g, shortfall: {:.1f}g",
                  iteration, bottleneck_ingredient,
                  ingredient_demand[bottleneck_ingredient], stock,
                  max_shortfall);

    if (stock < 1.0f) {
      Logger::debug("[Stock Capacity] No stock for ingredient {}, zeroing "
                    "affected pizzas",
                    bottleneck_ingredient);
      for (int pizza_idx = 0; pizza_idx < pizza_count; pizza_idx++) {
        if (pizza_ingredients[pizza_idx][bottleneck_ingredient] > 0) {
          pizza_weights[pizza_idx] = 0.0f;
        }
      }
    }
    else {
      float demand_for_bottleneck = ingredient_demand[bottleneck_ingredient];
      float scale =
          (demand_for_bottleneck - max_shortfall) / demand_for_bottleneck;

      Logger::debug("[Stock Capacity] Scaling pizzas using ingredient {} by "
                    "factor {:.3f}",
                    bottleneck_ingredient, scale);

      for (int pizza_idx = 0; pizza_idx < pizza_count; pizza_idx++) {
        if (pizza_ingredients[pizza_idx][bottleneck_ingredient] > 0) {
          pizza_weights[pizza_idx] *= scale;
        }
      }
    }
  }

  float total_capacity = 0.0f;
  for (int i = 0; i < pizza_count; i++) {
    total_capacity += pizza_weights[i];
  }

  int final_capacity = static_cast<int>(total_capacity);

  Logger::debug(
      "[Stock Capacity] {} - Raw capacity: {}, Bottleneck iterations: {}",
      name_, final_capacity, iteration);

  return final_capacity;
}

int Restaurant::calculate_weighted_average_oven_time() const {
  if (!layout_) {
    return 0;
  }

  int numerator = 0;
  int denominator = 0;

  for (const auto& pf : layout_->placed_kitchen_furniture()) {
    if (pf && pf->furniture()->category() == FurnitureCategory::KITCHENWARE) {
      int bake_time = pf->furniture()->bake_time();
      int oven_capacity = pf->furniture()->capacity();

      if (bake_time > 0 && oven_capacity > 0) {
        numerator += bake_time * oven_capacity;
        denominator += oven_capacity;
      }
    }
  }

  if (denominator == 0) {
    return 0;
  }

  return numerator / denominator;
}

int Restaurant::calculate_service_time_minutes(ConsumerType demographic) const {
  const int operating_minutes = operating_minutes_per_day();

  int total_chef_time = 0;
  int chef_count = 0;
  for (const auto& cook : cooks_) {
    if (cook.has_value() && !cook->is_away()) {
      total_chef_time += cook->calculate_prep_time_minutes(operating_minutes);
      chef_count++;
    }
  }
  const int avg_chef_time =
      (chef_count > 0) ? (total_chef_time / chef_count) : 0;

  const int oven_time = calculate_weighted_average_oven_time();

  int total_waiter_time = 0;
  int waiter_count = 0;
  for (const auto& waiter : waiters_) {
    if (waiter.has_value() && !waiter->is_away()) {
      total_waiter_time +=
          waiter->calculate_service_time_minutes(operating_minutes);
      waiter_count++;
    }
  }
  const int avg_waiter_time =
      (waiter_count > 0) ? (total_waiter_time / waiter_count) : 0;

  const int demographic_time = get_demographic_service_time(demographic);

  // total = max(chef, oven) + waiter + demographic
  const int kitchen_time = std::max(avg_chef_time, oven_time);
  const int total_time = kitchen_time + avg_waiter_time + demographic_time;

  return total_time;
}

int Restaurant::calculate_service_time_minutes() const {
  // Pure service time WITHOUT eating time.
  // Used for staff screen display and popularity component 0.
  // Formula: max(chef_time, oven_time) + waiter_time
  auto data = calculate_service_time_data();
  return data.service_time_minutes;
}

int Restaurant::calculate_block_throughput(ConsumerType demographic) const {
  int service_time = calculate_service_time_minutes(demographic);

  if (service_time == 0) {
    return 0;
  }

  int seat_count = 0;
  if (layout_) {
    for (const auto& pf : layout_->placed_furniture()) {
      if (pf) {
        seat_count += pf->furniture()->capacity();
      }
    }
  }

  constexpr int BLOCK_DURATION_MINUTES = 10;
  int throughput = (BLOCK_DURATION_MINUTES * seat_count) / service_time;

  return throughput;
}

int Restaurant::calculate_block_throughput() const {
  // Uses seating time (service + eating) since throughput is about
  // how many customers can cycle through seats per 10-minute block.
  auto data = calculate_service_time_data();
  int seating_time = static_cast<int>(data.avg_seating_time);

  if (seating_time == 0) {
    return 0;
  }

  int seat_count = 0;
  if (layout_) {
    for (const auto& pf : layout_->placed_furniture()) {
      if (pf) {
        seat_count += pf->furniture()->capacity();
      }
    }
  }

  constexpr int BLOCK_DURATION_MINUTES = 10;
  return (BLOCK_DURATION_MINUTES * seat_count) / seating_time;
}

void Restaurant::end_of_day(GameState& game_state) {
  auto* player = game_state.active_player();
  assert(player);
  const auto& date = game_state.time().current_date();

  // Pay monthly rent on the last day of the month
  if (player->is_human_controlled() && date.is_end_of_month() &&
      property_.is_rented()) {
    Money monthly_rent = property_.get_monthly_rent();

    if (monthly_rent.cents > 0) {
      player->financials().transfer(date, Account::Cash, Account::Rents,
                                    monthly_rent, restaurant_id_,
                                    AccountingScope::Operational);
      Logger::info("[Restaurant] {} - Paid monthly rent: {}", name_,
                   monthly_rent);
    }
  }

  // AI players use a simplified pipeline: range, reputation, competition,
  // revenue. They skip full order fulfillment and ingredient consumption.
  if (!player->is_human_controlled()) {
    if (!wants_open_ || forced_days_remaining_ > 0) {
      return;
    }

    economy_.update_restaurant_range();

    economy_.total_popularity(sync_reputation_and_total_popularity());
    economy_.calculate_competitive_strength(*this);
    process_daily_reputation_deltas();

    economy_.calculate_demand_with_competition(*this, game_state);

    ai::process_restaurant_revenue(*this, *player);
    return;
  }

  // Staff processing runs at end of day. Flag resets and absence timer
  // decrements happen for all restaurants (open or closed). Event generation
  // and manager routine are skipped for closed restaurants.
  {
    // Fullscreen levent shown when a staff member masters the top course.
    auto master_levent = [](SkillType skill) {
      switch (skill) {
      case SkillType::SERVING:
        return core::events::LeventId::FullyQualifiedWaiter;
      case SkillType::MANAGEMENT:
        return core::events::LeventId::ManagerIsExpertInBusinessStudies;
      case SkillType::COOKING:
      case SkillType::NONE:
        break;
      }
      return core::events::LeventId::FullyTrainedMasterchef;
    };

    auto reset_daily_staff = [&game_state, &master_levent](Staff& s) {
      s.late(false);
      s.stole_money(false);
      s.wants_to_quit(false);
      s.demands_raise(false);
      s.tick_days_away();

      // For a training course that just finished we need to apply the effects.
      if (s.on_course() && s.time_until_back() == 0) {
        const auto& ch = s.character();
        CourseResult res = s.complete_course();
        game_state.status().post(StatusMsg{
            .payload = compat::format(
                "{} {} has {} been on the {} training course.", ch.first_name(),
                ch.last_name(), res.success ? "successfully" : "not",
                res.course_name),
            .severity = StatusSeverity::Info,
            .scope = StatusScope::Restaurant,
        });
        if (res.mastered) {
          game_state.deliver_message(master_levent(res.skill));
        }
      }
    };
    for (auto& cook : cooks_) {
      if (cook) {
        reset_daily_staff(*cook);
      }
    }
    for (auto& waiter : waiters_) {
      if (waiter) {
        reset_daily_staff(*waiter);
      }
    }
    if (manager_) {
      reset_daily_staff(*manager_);
    }

    core::restaurant::StaffEvents staff_events(this, &game_state);
    staff_events.daily_processing();
    pending_staff_events_ = staff_events.take_pending_dialogs();
  }

  // Inspection countdown: only ticks while the restaurant is open.
  // When it reaches 0 an inspection is queued for delivery next morning.
  if (wants_open() && !is_force_closed() && inspection_days_remaining_ > 0) {
    inspection_days_remaining_--;
    if (inspection_days_remaining_ == 0) {
      pending_inspection_ = first_inspection_pending_;
      first_inspection_pending_ = false;
      inspection_days_remaining_ = engine::random::rand_max_n(60) + 60;
    }
  }

  if (!wants_open_ || forced_days_remaining_ > 0) {
    stats_.end_of_day(0);
    return;
  }

  int old_range_tiles = economy_.range_tiles();
  bool range_increased = economy_.update_restaurant_range();
  if (range_increased) {
    Logger::info("[Economy] {} - Range increased to {} tiles (raw: {})", name_,
                 economy_.range_tiles(), economy_.range_raw());
    game_state.status().post_direct(StatusMsg{
        .payload =
            compat::format("The range of restaurant {} rises from {} to {}.",
                           name_, old_range_tiles, economy_.range_tiles()),
        .scope = StatusScope::Global,
    });
  }

  // Sync popularity with current reputation, then decay for next day
  economy_.total_popularity(sync_reputation_and_total_popularity());
  process_daily_reputation_deltas();

  // Original game bug: sabotage contamination never decays, so multiple
  // sabotage attacks stack permanently. Fix: decay by 1 toward 0 each day.
  if (game_state.options().fix_legacy_bugs && sabotage_contamination_ != 0) {
    sabotage_contamination_ += (sabotage_contamination_ < 0) ? 1 : -1;
  }

  economy_.calculate_demand_with_competition(*this, game_state);
  calculate_daily_pizza_sales_distribution(game_state);
  eod_batch_order_fulfillment(game_state);

  core::restaurant::ManagerRoutine manager_routine(*this, game_state);
  manager_routine.run_daily();
}

void Restaurant::eod_batch_order_fulfillment(class GameState& game_state) {
  auto* player = game_state.active_player();
  if (!player) {
    fatal("No active player in eod_batch_order_fulfillment");
  }

  const auto& recipes = player->recipes();
  const auto& menu_entries = menu_.entries();

  int oven_capacity = calculate_daily_oven_throughput();
  int chef_capacity = calculate_daily_chef_throughput();
  int waiter_capacity = calculate_daily_waiter_throughput();

  int total_orders = 0;
  int total_fulfilled = 0;
  int lost_to_ingredients = 0;
  int lost_to_capacity = 0;
  Money total_revenue{0};
  Money total_ingredient_cost{0};

  int capacity_used = 0;

  for (const auto& entry : menu_entries) {
    int predicted = get_predicted_daily_orders(entry.recipe_id);
    total_orders += predicted;
  }

  // Staff shortage augmentation: if the manager is present and staff_shortage
  // is ActYourself, the manager fills in for the deficient role.
  if (manager_ && !manager_->is_away()) {
    const auto& settings = manager_settings_;
    if (settings.staff_shortage == ManagerReaction::ActYourself) {
      int manager_capacity = manager_->pizzas_per_day();
      if (chef_capacity < total_orders && chef_capacity <= waiter_capacity) {
        chef_capacity += manager_capacity;
        manager_filled_in_as_ = core::character::JobRole::CHEF;
      }
      else if (waiter_capacity < total_orders) {
        waiter_capacity += manager_capacity;
        manager_filled_in_as_ = core::character::JobRole::WAITER;
      }
    }
    else if (settings.staff_shortage == ManagerReaction::CallMe) {
      if (chef_capacity < total_orders) {
        manager_staff_shortage_ = core::character::JobRole::CHEF;
      }
      if (waiter_capacity < total_orders) {
        manager_staff_shortage_ = core::character::JobRole::WAITER;
      }
    }
  }

  int effective_capacity =
      std::min({oven_capacity, chef_capacity, waiter_capacity});

  // Shortage warnings (direct status messages, independent of manager).
  // only the lowest actually fires.
  constexpr int status_color_yellow = 16;
  std::string shortage_msg;
  int remaining_demand = total_orders;

  if (remaining_demand > 0) {
    // Supplies: check if ingredient stock covers total demand
    auto demand_grams = calculate_daily_ingredient_demand_grams(recipes);
    float supply_ratio = 1.0f;
    for (int i = 0; i < max_ingredient_count; ++i) {
      int needed = demand_grams[i];
      if (needed <= 0) {
        continue;
      }
      int available =
          inventory_.get_stock(static_cast<core::food::IngredientType>(i))
              .quantity;
      if (available < needed) {
        float ratio =
            static_cast<float>(available) / static_cast<float>(needed);
        supply_ratio = std::min(ratio, supply_ratio);
      }
    }
    if (supply_ratio < 1.0f) {
      shortage_msg = "There aren't enough supplies to satisfy demand!";
      remaining_demand =
          static_cast<int>(static_cast<float>(remaining_demand) * supply_ratio);
      remaining_demand = std::max(remaining_demand, 1);
    }

    // Ovens
    if (oven_capacity < remaining_demand) {
      shortage_msg = "Too few ovens to satisfy present demand!";
      remaining_demand = oven_capacity;
    }

    // Cooks (possibly augmented by manager)
    if (chef_capacity < remaining_demand) {
      shortage_msg = "Too few cooks to satisfy present demand!";
      remaining_demand = chef_capacity;
    }

    // Waiters (same)
    if (waiter_capacity < remaining_demand) {
      shortage_msg = "Too few waiters to satisfy present demand!";
      remaining_demand = waiter_capacity;
    }
  }

  if (!shortage_msg.empty()) {
    if (ConfigManager::instance()->options().qol_improvements) {
      shortage_msg = compat::format("{}: {}", name_, shortage_msg);
    }
    game_state.status().post_direct(StatusMsg{
        .payload = std::move(shortage_msg),
        .severity = StatusSeverity::Warning,
        .scope = StatusScope::Restaurant,
        .color = status_color_yellow,
    });
  }

  for (const auto& entry : menu_entries) {
    int predicted = get_predicted_daily_orders(entry.recipe_id);
    if (predicted == 0) {
      continue;
    }

    const auto* recipe = recipes.find(entry.recipe_id);
    if (!recipe) {
      lost_to_ingredients += predicted;
      stats_.today.orders_lost += predicted;
      continue;
    }

    auto requirements = get_recipe_requirements_grams(*recipe);

    for (int order_idx = 0; order_idx < predicted; order_idx++) {
      stats_.today.orders_generated++;
      stats_.today.pizza_sales[entry.recipe_id].record_demand();

      stats_.today.record_order(entry.price);

      if (capacity_used >= effective_capacity) {
        lost_to_capacity++;
        stats_.today.orders_lost++;
        stats_.today.lost_due_to_capacity++;
        continue;
      }

      if (!inventory_.can_fulfill_recipe(requirements)) {
        lost_to_ingredients++;
        stats_.today.orders_lost++;
        stats_.today.lost_due_to_ingredients++;
        continue;
      }

      if (!inventory_.consume_recipe(requirements)) {
        lost_to_ingredients++;
        stats_.today.orders_lost++;
        stats_.today.lost_due_to_ingredients++;
        continue;
      }

      Money ingredient_cost{0};
      for (const auto& [ingredient_type, qty_grams] : requirements) {
        const auto& stock = inventory_.get_stock(ingredient_type);
        Money this_cost =
            Money{(stock.weighted_avg_cost.cents * qty_grams) / 1000};
        ingredient_cost = ingredient_cost + this_cost;

        stats_.today.ingredients_used_grams += qty_grams;

        int ing_id = static_cast<int>(ingredient_type);
        for (int cat_idx = 0; cat_idx < 4; ++cat_idx) {
          auto category = static_cast<IngredientCategory>(cat_idx);
          if (ing_id >= core::food::CategoryRegistry::start_id(category) &&
              ing_id < core::food::CategoryRegistry::end_id(category)) {
            stats_.today.ingredient_costs_by_category[cat_idx] =
                stats_.today.ingredient_costs_by_category[cat_idx] + this_cost;
            break;
          }
        }
      }

      total_fulfilled++;
      capacity_used++;
      stats_.today.orders_fulfilled++;

      Money order_price = entry.price;
      total_revenue = total_revenue + order_price;
      total_ingredient_cost = total_ingredient_cost + ingredient_cost;

      stats_.today.revenue = stats_.today.revenue + order_price;
      stats_.today.ingredient_costs =
          stats_.today.ingredient_costs + ingredient_cost;
      stats_.today.record_sale(entry.recipe_id, order_price, ingredient_cost);
    }
  }

  if (total_orders > 0) {
    fulfillment_rate_ = (total_fulfilled * 100) / total_orders;
  }
  else {
    fulfillment_rate_ = 100;
  }

  player->financials().transfer(
      game_state.time().current_date(), Account::Sales, Account::Cash,
      stats_.today.revenue, restaurant_id_, AccountingScope::Operational);

  stats_.end_of_day(effective_capacity);
  calculate_pizza_quality_rating(recipes);

  Logger::info(
      "[Economy] {} - End of Day: {} orders ({} fulfilled, {} lost: "
      "capacity={}, ingredients={}), Revenue: ${:.2f}, Costs: ${:.2f}, "
      "Profit: ${:.2f}, Capacity: {}/{} ({}%)",
      name_, total_orders, total_fulfilled,
      lost_to_capacity + lost_to_ingredients, lost_to_capacity,
      lost_to_ingredients, total_revenue.cents / 100.0,
      total_ingredient_cost.cents / 100.0,
      (total_revenue.cents - total_ingredient_cost.cents) / 100.0,
      capacity_used, effective_capacity, fulfillment_rate_);
}

int Restaurant::calculate_average_cook_quality() const {
  // Weighted average of cooking skill, weighted by productivity so that
  // higher-output chefs have more influence on quality.
  // Original: +0x82h = average cooking skill (0-100 range).
  int total_quality_weighted = 0;
  int total_productivity = 0;

  for (const auto& cook_opt : cooks_) {
    if (!cook_opt.has_value() || cook_opt->is_away()) {
      continue;
    }
    const auto& cook = *cook_opt;
    int productivity = cook.character().pizzas_per_day();
    int cooking_skill = cook.character().cooking();
    total_productivity += productivity;
    total_quality_weighted += cooking_skill * productivity;
  }

  if (total_productivity == 0) {
    return 0;
  }

  return total_quality_weighted / total_productivity;
}

int Restaurant::calculate_average_oven_quality() const {
  if (!layout_) {
    return 0;
  }

  int total_quality_weighted = 0;
  int total_capacity = 0;

  for (const auto& pf : layout_->placed_kitchen_furniture()) {
    if (!pf || pf->furniture()->bake_time() == 0) {
      continue;
    }
    int quality = pf->furniture()->quality();
    int bake_time = pf->furniture()->bake_time();
    // Invert bake_time so faster ovens contribute more
    int capacity_weight = (bake_time > 0) ? (100 / bake_time) : 100;
    total_capacity += capacity_weight;
    total_quality_weighted += quality * capacity_weight;
  }

  if (total_capacity == 0) {
    return 0;
  }

  return total_quality_weighted / total_capacity;
}

int Restaurant::calculate_ingredient_quality_for_pizza(
    const core::food::Pizza& pizza) const {
  int total_quality_weighted = 0;
  int total_weight_grams = 0;

  for (const auto& topping_ptr : pizza.toppings()) {
    if (!topping_ptr) {
      continue;
    }
    const auto* ingredient = topping_ptr->ingredient();
    if (!ingredient) {
      continue;
    }

    core::food::IngredientType ing_type = topping_ptr->ingredient_type();
    const auto& stock = inventory_.get_stock(ing_type);
    int weight_grams = topping_ptr->weight();

    total_quality_weighted += stock.weighted_avg_quality * weight_grams;
    total_weight_grams += weight_grams;
  }

  if (total_weight_grams == 0) {
    return 0;
  }

  return total_quality_weighted / total_weight_grams;
}

void Restaurant::calculate_pizza_quality_rating(
    const core::food::PlayerRecipes& recipes) {
  // Combines stock, chef, oven, and per-pizza ingredient quality.
  // Each base component is divided by 4. Per pizza: if the weakest link
  // (min of staff and ingredient quality) is < 16, it drags the score down
  // via averaging. Final rating is the mean across all menu pizzas.

  if (menu_.entries().empty()) {
    pizza_quality_rating_ = 0;
    pizza_quality_factors_ = {};
    return;
  }

  int base_stock = inventory_.average_stock_quality() / 4;
  int base_cook = calculate_average_cook_quality() / 4;
  int base_oven = calculate_average_oven_quality() / 4;
  int base_contamination = sabotage_contamination_ / 4;

  pizza_quality_factors_.ingredients = base_stock;
  pizza_quality_factors_.chef = base_cook;
  pizza_quality_factors_.oven = base_oven;
  pizza_quality_factors_.contamination = base_contamination;

  int base_sum = base_stock + base_cook + base_oven + base_contamination;

  // Min of RAW (not /4) staff quality, used as threshold against ingredient
  // quality
  int min_staff = std::min(calculate_average_cook_quality(),
                           calculate_average_oven_quality());

  int total_quality = 0;
  int pizza_count = 0;

  for (const auto& entry : menu_.entries()) {
    const auto* recipe = recipes.find(entry.recipe_id);
    if (!recipe) {
      continue;
    }

    int ingredient_quality =
        calculate_ingredient_quality_for_pizza(recipe->pizza());

    int min_quality = std::min(min_staff, ingredient_quality);

    // If min_quality < 16, use averaged path; otherwise use base_sum directly
    int pizza_quality;
    if (min_quality < 16) {
      pizza_quality = (base_sum + min_quality) / 2;
    }
    else {
      pizza_quality = base_sum;
    }

    total_quality += pizza_quality;
    pizza_count++;
  }

  if (pizza_count == 0) {
    pizza_quality_rating_ = 0;
    return;
  }

  pizza_quality_rating_ = std::clamp(total_quality / pizza_count, 0, 100);
}

std::optional<bool> Restaurant::update_star_rating() {
  // Mayor awards/removes stars based on pizza quality rating.
  // Gain threshold: quality > stars * 16 + 24
  // Loss threshold: quality < stars * 16 + 8

  int award_threshold = (star_rating_ * 16) + 24;
  int loss_threshold = (star_rating_ * 16) + 8;

  if (pizza_quality_rating_ > award_threshold && star_rating_ < 5) {
    star_rating_++;
    Logger::info("[Restaurant] {} awarded star! Now {} stars (quality {} > "
                 "threshold {})",
                 name_, star_rating_, pizza_quality_rating_, award_threshold);
    return true;
  }
  if (pizza_quality_rating_ < loss_threshold && star_rating_ > 0) {
    star_rating_--;
    Logger::info("[Restaurant] {} lost a star! Now {} stars (quality {} < "
                 "threshold {})",
                 name_, star_rating_, pizza_quality_rating_, loss_threshold);
    return false;
  }
  Logger::info("[Restaurant] {} didn't change star rating ({})! (quality {}, "
               "up threshold {}, down threshold {})",
               name_, star_rating_, pizza_quality_rating_, award_threshold,
               loss_threshold);
  return std::nullopt;
}

core::food::IngredientDemand
Restaurant::calculate_daily_ingredient_demand_grams(
    const core::food::PlayerRecipes& recipes) const {
  core::food::IngredientDemand demand_grams{};

  const auto& menu_entries = menu_.entries();
  if (menu_entries.empty()) {
    Logger::debug("[Restaurant] {} - No menu items, returning zero demand",
                  name_);
    return demand_grams;
  }

  int total_predicted = 0;
  for (const auto& entry : menu_entries) {
    const auto* recipe = recipes.find(entry.recipe_id);
    if (!recipe) {
      Logger::debug("[Restaurant] {} - Recipe not found for id {}", name_,
                    entry.recipe_id.value);
      continue;
    }

    int predicted_orders = get_predicted_daily_orders(entry.recipe_id);

    // Default to yesterday's sales or 10 if no forecast yet
    if (predicted_orders == 0) {
      auto it = stats_.yesterday.pizza_sales.find(entry.recipe_id);
      predicted_orders = (it != stats_.yesterday.pizza_sales.end())
                             ? it->second.count_sold
                             : 10;
    }

    total_predicted += predicted_orders;

    const auto& pizza = recipe->pizza();

    for (const auto& topping : pizza.toppings()) {
      int weight = topping->weight();
      demand_grams[topping->ingredient_type()] += weight * predicted_orders;
    }
  }

  if (id().value == 0) {
    Logger::debug("[Restaurant] {} - Ingredient demand calculated for {} "
                  "predicted pizzas across {} menu items",
                  name_, total_predicted, menu_entries.size());
  }

  return demand_grams;
}

std::vector<std::pair<core::food::IngredientType, int>>
Restaurant::get_recipe_requirements_grams(const core::food::Recipe& recipe) {
  core::food::IngredientDemand ingredient_grams{};

  const auto& pizza = recipe.pizza();
  for (const auto& topping_ptr : pizza.toppings()) {
    if (!topping_ptr) {
      continue;
    }

    ingredient_grams[topping_ptr->ingredient_type()] += topping_ptr->weight();
  }

  std::vector<std::pair<core::food::IngredientType, int>> requirements;
  for (int i = 0; i < max_ingredient_count; i++) {
    if (ingredient_grams[i] > 0) {
      // Original uses units of 10 grams
      int rounded = ((ingredient_grams[i] + 9) / 10) * 10;
      requirements.emplace_back(static_cast<core::food::IngredientType>(i),
                                rounded);
    }
  }

  return requirements;
}

Popularity Restaurant::calculate_menu_popularity(
    const core::food::PlayerRecipes& recipes) const {
  return economy_.calculate_menu_popularity(*this, recipes);
}

Popularity Restaurant::calculate_furniture_appeal_popularity() const {
  return economy_.calculate_furniture_appeal_popularity(*this);
}

int Restaurant::calculate_trend_appeal(const City& city) const {
  int total = 0;
  const auto& trends = city.ingredient_trends();

  for (int category = 0; category < NUM_INGREDIENT_CATEGORIES; category++) {
    auto ingredient_id = top_ingredients_[category];
    if (!ingredient_id) {
      continue;
    }

    total += trends[*ingredient_id];
  }

  return std::clamp(total, -125, 125);
}

void Restaurant::start_of_day(GameState& game_state) {
  for (const auto ingredient_type : inventory_.apply_daily_degradation()) {
    game_state.status().post(StatusMsg{
        .payload =
            compat::format("{}: The supplies of {} were rotten and had to be "
                           "thrown away.",
                           name_, core::food::to_string(ingredient_type)),
        .scope = StatusScope::Restaurant,
        .delay_blocks = 3,
    });
  }

  // Tick down forced closures. On re-open you get the reputation hit.
  if (forced_days_remaining_ > 0) {
    --forced_days_remaining_;
    if (forced_days_remaining_ == 0) {
      add_reputation_damage(-30, 15);
    }
  }

  decrement_protection_timer();

  recalculate_economy_metrics(game_state);

  if (wants_open_ && forced_days_remaining_ == 0) {
    int total_customers = 0;
    for (auto demo : ALL_CONSUMER_TYPES) {
      total_customers += property_.potential_customers()[demo];
    }

    Logger::info(
        "[Economy] {} opened - Range raw: {}, Radius: {}, Total potential "
        "customers: {}",
        name_, economy_.range_raw(), economy_.range_tiles(), total_customers);

    if (total_customers == 0) {
      Logger::warn("[Economy] {} - Zero customers! Check property location or "
                   "range setting.",
                   name_);
    }
  }
}

int Restaurant::get_predicted_daily_orders(core::RecipeId recipe_id) const {
  auto it = predicted_daily_orders_.find(recipe_id);
  if (it != predicted_daily_orders_.end()) {
    return it->second;
  }
  return 0;
}

void Restaurant::update_menu_factor(const core::food::PlayerRecipes& recipes,
                                    const City& city,
                                    const District& district) {
  const auto& menu_entries = menu_.entries();
  const int pizza_count = static_cast<int>(menu_entries.size());

  if (pizza_count == 0) {
    for (auto demo : ALL_CONSUMER_TYPES) {
      economy_.menu_factor()[demo] = 1;
    }
    return;
  }

  int total_serving_quality = 0;
  int waiter_count = 0;
  for (const auto& waiter : waiters_) {
    if (waiter.has_value() && !waiter->is_away()) {
      total_serving_quality += waiter->character().serving();
      waiter_count++;
    }
  }
  int avg_serving_quality =
      waiter_count > 0 ? total_serving_quality / waiter_count : 0;

  for (int demographic = 0; demographic < 6; demographic++) {
    ConsumerType demo_type = static_cast<ConsumerType>(demographic);

    std::vector<float> pizza_demand_scores(pizza_count + 1, 0.0f);
    std::vector<bool> already_picked(pizza_count + 1, false);

    for (int pizza_idx = 0; pizza_idx < pizza_count; pizza_idx++) {
      const auto& entry = menu_entries[pizza_idx];
      int demand = core::restaurant::PizzaDemand::calculate(
          entry, recipes, city, district, demo_type);
      pizza_demand_scores[pizza_idx] = static_cast<float>(demand);
      already_picked[pizza_idx] = false;
    }
    pizza_demand_scores[pizza_count] = 0.0f;

    int top5_sum = 0;
    for (int pick_iteration = 0; pick_iteration < 5; pick_iteration++) {
      int best_idx = pizza_count;
      for (int search_idx = 0; search_idx < pizza_count; search_idx++) {
        if (pizza_demand_scores[search_idx] > pizza_demand_scores[best_idx] &&
            !already_picked[search_idx]) {
          best_idx = search_idx;
        }
      }
      top5_sum += static_cast<int>(pizza_demand_scores[best_idx]);
      already_picked[best_idx] = true;
    }

    int avg_top5 = top5_sum / 5;
    int serving_bonus = avg_serving_quality >> 3;
    economy_.menu_factor()[demo_type] =
        std::clamp(avg_top5 + serving_bonus, 1, 255);
  }
}

void Restaurant::recalculate_economy_metrics(GameState& game_state) {
  auto* player = game_state.active_player();
  if (!player || !player->is_human_controlled()) {
    return;
  }
  const auto& city = property_.city();
  const auto& district = property_.district();
  calculate_popularity(*player, city, district);
  economy_.total_popularity(popularity_.total());
  update_demographics(player->recipes());
}

void Restaurant::open_for_business(GameState& game_state) {
  auto* player = game_state.active_player();
  if (player && player->is_human_controlled()) {
    update_menu_factor(player->recipes(), property_.city(),
                       property_.district());
  }
  wants_open(true);
  recalculate_economy_metrics(game_state);
}

void Restaurant::close_for_business() { wants_open(false); }

void Restaurant::calculate_daily_pizza_sales_distribution(
    GameState& game_state) {
  auto* player = game_state.active_player();
  if (!player) {
    return;
  }

  const auto& recipes = player->recipes();
  const auto& menu_entries = menu_.entries();
  const int pizza_count = static_cast<int>(menu_entries.size());

  if (pizza_count == 0) {
    for (auto demo : ALL_CONSUMER_TYPES) {
      economy_.menu_factor()[demo] = 1;
    }
    return;
  }

  const City& city = property_.city();
  const District& district = property_.district();

  update_menu_factor(recipes, city, district);

  std::vector<float> pizza_total_sales(pizza_count, 0.0f);
  predicted_daily_orders_.clear();

  for (int demographic = 0; demographic < 6; demographic++) {
    ConsumerType demo_type = static_cast<ConsumerType>(demographic);

    std::vector<float> pizza_demand_scores(pizza_count + 1,
                                           0.0f); // +1 sentinel
    float total_demand = 0.0f;

    for (int pizza_idx = 0; pizza_idx < pizza_count; pizza_idx++) {
      const auto& entry = menu_entries[pizza_idx];

      int demand = core::restaurant::PizzaDemand::calculate(
          entry, recipes, city, district, demo_type);

      pizza_demand_scores[pizza_idx] = static_cast<float>(demand);
      total_demand += static_cast<float>(demand);
    }

    // Distribute customers proportionally by pizza demand
    if (total_demand > 0.0f) {
      auto customer_count =
          static_cast<float>(economy_.customer_counts()[demo_type]);

      for (int pizza_idx = 0; pizza_idx < pizza_count; pizza_idx++) {
        float sales_contribution =
            customer_count * pizza_demand_scores[pizza_idx] / total_demand;
        pizza_total_sales[pizza_idx] += sales_contribution;
      }
    }
  }

  int total_predicted = 0;

  for (int pizza_idx = 0; pizza_idx < pizza_count; pizza_idx++) {
    const auto& entry = menu_entries[pizza_idx];
    float sales = pizza_total_sales[pizza_idx];

    if (sales > 250.0f) {
      int random_bonus = engine::random::rand_max_n(5);
      sales = static_cast<float>(250 + random_bonus);
    }

    int predicted = static_cast<int>(sales);
    predicted_daily_orders_[entry.recipe_id] = predicted;
    total_predicted += predicted;
  }

  Logger::debug("[Restaurant] {} - Sales distribution: {} total across {} "
                "pizzas, menu_factor=[{},{},{},{},{},{}]",
                name_, total_predicted, pizza_count,
                economy_.menu_factor()[ConsumerType::Children],
                economy_.menu_factor()[ConsumerType::YoungPeople],
                economy_.menu_factor()[ConsumerType::BlueCollar],
                economy_.menu_factor()[ConsumerType::WhiteCollar],
                economy_.menu_factor()[ConsumerType::BusinessPeople],
                economy_.menu_factor()[ConsumerType::SeniorCitizens]);
}

void Restaurant::calculate_popularity(const Player& player, const City& city,
                                      const District& district) {
  auto service_data = calculate_service_time_data();
  service_time_minutes_ = service_data.service_time_minutes;

  popularity_ = RestaurantPopularity{};
  int pizzas_on_menu = static_cast<int>(menu_.entries().size());
  int empty_slots = core::restaurant::Menu::MAXIMUM_PIZZAS - pizzas_on_menu;

  // WaitingTime: (18 - service_time) * district_factor / 50
  if (service_time_minutes_ > 0 && district.service_quality_factor() > 0) {
    int raw_bonus =
        (18 - service_time_minutes_) * district.service_quality_factor();
    popularity_.waiting_time = std::clamp(raw_bonus / 50, -127, 127);
  }

  // PizzaPrices: seems disabled in the original
  popularity_.pizza_prices = 0;

  // RecipesOnMenu: penalize < 3 canonical pizzas, reward exceeding district
  // desired counts for both user and canonical pizzas
  int component2_score = 0;
  int total_pizza_count = static_cast<int>(menu_.count_total());
  int canonical_pizza_count =
      static_cast<int>(menu_.count_canonical(player.recipes()));
  int user_pizza_count = total_pizza_count - canonical_pizza_count;

  if (canonical_pizza_count < 3) {
    int penalty = (3 - canonical_pizza_count) * 20;
    component2_score -= penalty;
  }

  int user_desire = district.minimum_desired_user_pizza_count();
  if (user_pizza_count > user_desire) {
    int bonus = user_pizza_count - user_desire;
    component2_score += bonus;
  }

  int canonical_desire = district.minimum_desired_canonical_pizza_count();
  int adjustment = (canonical_pizza_count - canonical_desire) * 2;
  component2_score += adjustment;
  popularity_.recipes_on_menu = component2_score;

  int total_ad_effectiveness =
      player.total_advertising_effectiveness(city.id());
  popularity_.advertising = total_ad_effectiveness / 2;

  // StarRating: stars^2 + (stars > 0 ? 4 : 0)
  int star_bonus = star_rating_ * star_rating_;
  if (star_rating_ > 0) {
    star_bonus += 4;
  }
  popularity_.star_rating = std::min(star_bonus, 127);

  // Furnishing: also seems disabled in the original
  popularity_.furnishing = 0;

  // DemandFulfillment: penalty for unmet demand
  int fulfillment_penalty = -((100 - fulfillment_rate_) / 8);
  popularity_.demand_fulfillment = fulfillment_penalty;

  // UnexpectedEvents: last non-zero penalty from the 4 event slots
  int last_nonzero_penalty = 0;
  for (int i = 0; i < 4; i++) {
    if (event_reputation_penalties_[i] != 0) {
      last_nonzero_penalty = event_reputation_penalties_[i];
    }
  }
  popularity_.unexpected_events = std::clamp(last_nonzero_penalty, -127, 127);
}

int Restaurant::total_popularity() const { return popularity_.total(); }

int Restaurant::sync_reputation_and_total_popularity() {
  int last_nonzero = 0;
  for (int i = 0; i < 4; i++) {
    if (event_reputation_penalties_[i] != 0) {
      last_nonzero = event_reputation_penalties_[i];
    }
  }
  popularity_.unexpected_events = std::clamp(last_nonzero, -127, 127);
  return total_popularity();
}

Money Restaurant::purchase_furniture(const Furniture* furniture, int quantity) {
  Money total_cost = furniture->price() * quantity;
  stats_.today.furniture_purchases =
      stats_.today.furniture_purchases + total_cost;
  return total_cost;
}

Money Restaurant::purchase_ingredients(core::food::IngredientType type,
                                       int quantity_kg, int quality,
                                       Money price_per_kg) {
  Money total_cost = Money{price_per_kg.cents * quantity_kg};
  int current_grams = inventory_.get_stock(type).quantity;
  int headroom_kg = (max_ingredient_stock_kg * 1000 - current_grams) / 1000;
  int capped_kg = std::min(quantity_kg, std::max(headroom_kg, 0));
  inventory_.add_ingredient(type, capped_kg * 1000, quality, price_per_kg);

  int ing_id = static_cast<int>(type);
  for (int cat_idx = 0; cat_idx < 4; ++cat_idx) {
    auto category = static_cast<IngredientCategory>(cat_idx);
    if (ing_id >= core::food::CategoryRegistry::start_id(category) &&
        ing_id < core::food::CategoryRegistry::end_id(category)) {
      stats_.today.ingredient_purchases_by_category[cat_idx] =
          stats_.today.ingredient_purchases_by_category[cat_idx] + total_cost;
      Logger::debug("[{}] Ingredient purchase tracked: {} (cat {}) = {}", name_,
                    ing_id, cat_idx, total_cost);
      break;
    }
  }

  stats_.today.ingredient_purchases =
      stats_.today.ingredient_purchases + total_cost;

  return total_cost;
}
