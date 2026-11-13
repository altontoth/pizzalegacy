/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "core/character/Player.h"
#include "core/city/Property.h"
#include "core/finance/TurnInvoices.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"

namespace views::finance_review {
struct CityEstimate {
  Money daily_ads{0}, remaining_ads{0}, weekly_wages{0}, daily_operating{0};
  int restaurants{0};
  bool has_history{false};
};
inline CityEstimate estimate(const GameState& state, const Player& player,
                             core::CityId city) {
  CityEstimate result;
  for (const auto* rest : state.restaurants_for(player.id())) {
    if (!rest || rest->property().city().id() != city) continue;
    ++result.restaurants;
    const auto wages = core::finance::collect_staff_wages(*rest).total;
    result.weekly_wages += wages;
    const auto days = rest->stats().last_week();
    if (!days.empty()) {
      result.has_history = true;
      Money total{0};
      for (const auto& day : days) total += day.gross_profit;
      result.daily_operating += total / static_cast<int>(days.size());
    }
    result.daily_operating -= wages / 7;
    if (rest->property().is_rented())
      result.daily_operating -= rest->property().get_monthly_rent() / 30;
  }
  const auto it = player.get_campaigns().find(city);
  if (it != player.get_campaigns().end()) {
    for (const auto& campaign : it->second) {
      if (!campaign || campaign->days_remaining <= 0) continue;
      result.daily_ads += campaign->rate.cost;
      result.remaining_ads += campaign->rate.cost * campaign->days_remaining;
    }
  }
  return result;
}
} // namespace views::finance_review
