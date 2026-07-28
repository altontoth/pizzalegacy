/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/state/MilestoneTracker.h"

#include <algorithm>
#include <array>
#include <unordered_set>

#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/District.h"
#include "core/city/Property.h"
#include "core/common/Ids.h"
#include "core/events/LeventId.h"
#include "core/finance/Money.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/util/Constants.h"

namespace {

// Cities the player holds a restaurant in, whether or not it ever opened.
int owned_city_count(GameState& gs, core::PlayerId pid) {
  std::array<bool, NUMBER_OF_CITIES> cities = {};
  for (const auto* rest : gs.restaurants_for(pid)) {
    const int city = rest->property().city().id().value;
    if (city >= 0 && city < NUMBER_OF_CITIES) {
      cities[city] = true;
    }
  }
  return static_cast<int>(std::count(cities.begin(), cities.end(), true));
}

} // namespace

void MilestoneTracker::check_milestone_events(Player& player,
                                              bool is_end_of_week) {
  using core::events::LeventId;
  const auto& date = gs_->time().current_date();
  const auto pid = player.id();
  auto& message_manager = gs_->message_manager();

  // Asset milestones (cash on hand)
  const Money cash = player.cash_balance();
  if (cash >= 100'000_usd) {
    message_manager.schedule_message(LeventId::HasAssetsOf100K, pid, date);
  }
  if (cash >= 500'000_usd) {
    message_manager.schedule_message(LeventId::YouHaveAssetsOf500K, pid, date);
  }
  if (cash >= 1'000'000_usd) {
    message_manager.schedule_message(LeventId::YouHaveBecomeMillionaire, pid,
                                     date);
  }
  if (cash >= 2'000'000_usd) {
    message_manager.schedule_message(LeventId::MakesASecondMillion, pid, date);
  }
  if (cash >= 10'000'000_usd) {
    message_manager.schedule_message(LeventId::MillionaireTenTimesOver, pid,
                                     date);
  }
  if (cash >= 20'000'000_usd) {
    message_manager.schedule_message(LeventId::MillionaireTwentyTimesOver, pid,
                                     date);
  }

  // Per-restaurant daily stats for active city restaurants.
  // We can use yesterday because stats_.end_of_day() has already archived
  // today.
  int total_daily_pizzas = 0;
  Money total_daily_revenue;
  int64_t total_ingredients_today = 0;

  const auto active_city_id = gs_->active_city_id();
  auto city_restaurants = gs_->restaurants_in_city(pid, active_city_id);
  for (auto* rest : city_restaurants) {
    if (!rest) {
      continue;
    }
    const auto& yesterday = rest->stats().yesterday;
    total_daily_pizzas += yesterday.orders_fulfilled;
    total_daily_revenue = total_daily_revenue + yesterday.revenue;
    total_ingredients_today += yesterday.ingredients_used_grams;

    if (yesterday.orders_fulfilled >= 1'000) {
      message_manager.schedule_message(
          LeventId::Has1000CustomersPerDayInOneRestaurant, pid, date);
    }

    int st = rest->service_time_minutes();
    if (st > 0 && st <= 4) {
      message_manager.schedule_message(
          LeventId::AverageServiceTimeLessThan4Minutes, pid, date);
    }
  }

  if (total_ingredients_today >= 1000LL * 1000) {
    message_manager.schedule_message(LeventId::YouHaveUsed1000KiloOfIngredients,
                                     pid, date);
  }
  if (total_daily_pizzas >= 1'000) {
    message_manager.schedule_message(LeventId::Sold1KPizzasPerDay, pid, date);
  }
  if (total_daily_pizzas >= 10'000) {
    message_manager.schedule_message(LeventId::Sold10KPizzasPerDay, pid, date);
  }
  if (total_daily_revenue >= 10'000_usd) {
    message_manager.schedule_message(LeventId::AchievedTurnover10KPerDay, pid,
                                     date);
  }
  if (total_ingredients_today >= 5'000LL * 1000) {
    message_manager.schedule_message(LeventId::Used5000KgIngredientsInOneDay,
                                     pid, date);
  }

  // District coverage: fire if player has a restaurant in every district
  auto* city = gs_->active_city();
  if (city && !city->districts().empty()) {
    const auto& all_districts = city->districts();
    std::unordered_set<core::DistrictId, core::DistrictId::Hasher> covered;
    for (auto* rest : city_restaurants) {
      if (rest && rest->property().city().id() == active_city_id) {
        covered.insert(rest->property().district().id());
      }
    }
    if (covered.size() >= all_districts.size()) {
      message_manager.schedule_message(LeventId::RepresentedInAllAreas, pid,
                                       date);
    }
  }

  if (!is_end_of_week) {
    return;
  }

  // Weekly loss milestone
  const auto week_start = date.add_days(-6);
  const Money week_receipts =
      player.financials().total_receipts(week_start, date);
  const Money week_expenditures =
      player.financials().total_expenditures(week_start, date);
  const Money week_net = week_receipts - week_expenditures;
  if (week_net < -1'000'000_usd) {
    message_manager.schedule_message(LeventId::LosesOneMillionInOneWeek, pid,
                                     date);
  }

  // Weekly profit per restaurant and per city
  Money city_weekly_profit;
  int perfect_delivery_count = 0;
  for (auto* rest : city_restaurants) {
    if (!rest || rest->property().city().id() != active_city_id) {
      continue;
    }
    const Money wp = rest->last_weekly_profit();
    city_weekly_profit = city_weekly_profit + wp;

    if (wp >= 25'000_usd) {
      message_manager.schedule_message(
          LeventId::Has25KWeeksProfitInOneRestaurant, pid, date);
    }
    if (wp >= 50'000_usd) {
      message_manager.schedule_message(
          LeventId::Has50KWeeksProfitInOneRestaurant, pid, date);
    }

    // Perfect deliveries: no orders lost in past week. The original gives
    // you this even if you didn't have any orders, let's optionally prevent
    // that.
    const auto week_days = rest->stats().last_week();
    bool perfect_week = !week_days.empty();
    for (const auto& day : week_days) {
      if (day.orders_lost > 0 ||
          (gs_->options().fix_legacy_bugs && day.orders_generated == 0)) {
        perfect_week = false;
        break;
      }
    }
    if (perfect_week) {
      perfect_delivery_count++;
      message_manager.schedule_message(LeventId::AbleToMeet100PctDeliveries,
                                       pid, date);
    }
  }

  if (perfect_delivery_count >= 5) {
    message_manager.schedule_message(
        LeventId::PerfectDeliveriesFor1WeekAcross5Restaurants, pid, date);
  }

  if (city_weekly_profit >= 500'000_usd) {
    message_manager.schedule_message(LeventId::Has500KWeeksProfitsInOneCity,
                                     pid, date);
  }
}

void MilestoneTracker::check_market_share_milestones(const City& city) {
  using core::events::LeventId;
  const auto& pcts = city.control_map().player_percentages();
  const auto& date = gs_->time().current_date();

  auto domination_event = static_cast<LeventId>(
      static_cast<int>(LeventId::DominatesMarketInParis) + city.id().value);

  for (int p = 0; p < NUMBER_OF_PLAYERS; ++p) {
    auto pid = core::PlayerId{p};
    if (!gs_->player(pid)) {
      continue;
    }
    float pct = pcts[p];
    // Original game bug: checks 5% but displays "10% market share" message
    float first_threshold = gs_->options().fix_legacy_bugs ? 10.0f : 5.0f;
    if (pct >= 50) {
      gs_->message_manager().schedule_message(
          LeventId::Achieved50PctMarketShareInCity, pid, date);
      gs_->message_manager().schedule_message(domination_event, pid, date);
    }
    if (pct >= 20) {
      gs_->message_manager().schedule_message(
          LeventId::Achieved20PctMarketShareInCity, pid, date);
    }
    if (pct >= first_threshold) {
      gs_->message_manager().schedule_message(
          LeventId::Achieved10PctMarketShareInCity, pid, date);
    }
  }
}

void MilestoneTracker::check_world_domination() {
  auto* p = gs_->active_player();
  if (!p || !p->is_human_controlled()) {
    return;
  }
  if (p->status() != PlayerStatus::Active) {
    return;
  }

  constexpr float win_threshold = 5.0f;

  if (!gs_->options().fix_legacy_bugs) {
    // Original: only Paris (city 0) needs >= 5% market share.
    auto* paris = gs_->city(core::CityId{0});
    if (!paris ||
        paris->control_map().player_percentage(p->id().value) < win_threshold) {
      return;
    }
  }
  else {
    // Fixed: must have >= 5% in all 10 cities.
    auto cities = gs_->initialized_cities();
    if (static_cast<int>(cities.size()) < NUMBER_OF_CITIES) {
      return;
    }
    for (auto* c : cities) {
      if (c->control_map().player_percentage(p->id().value) < win_threshold) {
        return;
      }
    }
  }

  gs_->trigger_win_sequence(p->id());
}

void MilestoneTracker::check_restaurant_milestones(const Player& player) {
  using core::events::LeventId;
  auto pid = player.id();
  auto date = gs_->time().current_date();

  const bool fix_bugs = gs_->options().fix_legacy_bugs;

  int opened_count = 0;
  int open_count = 0;
  std::array<bool, NUMBER_OF_CITIES> opened_cities = {};

  for (const auto* rest : gs_->restaurants_for(pid)) {
    if (!rest->ever_opened()) {
      continue;
    }
    opened_count++;
    if (rest->wants_open()) {
      open_count++;
    }
    const int city_val = rest->property().city().id().value;
    if (city_val >= 0 && city_val < NUMBER_OF_CITIES) {
      opened_cities[city_val] = true;
    }
  }

  if (opened_count >= 5) {
    gs_->message_manager().schedule_message(LeventId::HasFiveRestaurants, pid,
                                            date);
  }

  // The original counts restaurants currently open rather than ever opened,
  // and skips events 30, 31 and 50 entirely once a third one is open. Event 50
  // further needs the player to hold restaurants in exactly two cities, which
  // is why it almost never appears in an original game.
  const int count = fix_bugs ? opened_count : open_count;
  if (fix_bugs || open_count < 3) {
    if (count == 1) {
      gs_->message_manager().schedule_message(LeventId::OpenedFirstRestaurant,
                                              pid, date);
    }
    else if (count == 2) {
      gs_->message_manager().schedule_message(LeventId::OpensSecondRestaurant,
                                              pid, date);
    }

    const bool other_city =
        fix_bugs
            ? std::count(opened_cities.begin(), opened_cities.end(), true) >= 2
            : owned_city_count(*gs_, pid) == 2;
    if (other_city) {
      gs_->message_manager().schedule_message(
          LeventId::FirstRestaurantInOtherCity, pid, date);
    }
  }

  gs_->process_pending_messages();
}

void MilestoneTracker::check_restaurant_acquired(const Player& player) {
  using core::events::LeventId;
  if (owned_city_count(*gs_, player.id()) < NUMBER_OF_CITIES) {
    return;
  }
  gs_->message_manager().schedule_message(LeventId::OutletsInEveryWesternCity,
                                          player.id(),
                                          gs_->time().current_date());
}

void MilestoneTracker::check_depot_milestones(const Player& player) {
  using core::events::LeventId;
  int depot_count = 0;
  for (auto* city : gs_->initialized_cities()) {
    if (city->central_depot(player.id()).capacity() > 0) {
      depot_count++;
    }
  }
  const auto& date = gs_->time().current_date();
  if (depot_count >= 10) {
    gs_->message_manager().schedule_message(
        LeventId::CentralWarehouseIn10Cities, player.id(), date);
  }
  if (depot_count >= 5) {
    gs_->message_manager().schedule_message(LeventId::CentralWarehouseIn5Cities,
                                            player.id(), date);
  }
}
