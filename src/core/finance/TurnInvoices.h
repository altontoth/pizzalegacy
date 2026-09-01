/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "core/finance/Money.h"

#include <string>
#include <vector>

class GameState;
class Player;
class Restaurant;

// Data and money transfers for the periodic invoices shown around the turn
// boundary. collect_* compute the amounts; apply_* post the transfers. The
// popup builders in views/ui/finance/TurnInvoicePopups.h only render these.
namespace core::finance {

struct StaffWages {
  struct Entry {
    std::string name;
    Money payment;
  };
  std::vector<Entry> entries;
  Money total;
};
StaffWages collect_staff_wages(const Restaurant& rest);
// Applying staff wages is GameState::deduct_staff_wages_for_restaurant(),
// which also handles the bankruptcy consequence.

// Posts the sales revenue and ingredient cost of the day that just ended.
void apply_daily_sales(GameState& gs, Restaurant& rest);

struct ElectricitySlotInvoice {
  core::RestaurantId restaurant_id;
  Money electricity;
  Money slot_income;
  Money total; // electricity - slot_income
};
ElectricitySlotInvoice collect_electricity_slot_invoice(const Restaurant& rest);
void apply_electricity_slot_invoice(GameState& gs, Player& player,
                                    const ElectricitySlotInvoice& invoice);

struct MonthlyCosts {
  struct Rent {
    core::RestaurantId restaurant_id;
    Money amount;
  };
  struct DepotRent {
    core::CityId city_id;
    Money amount;
  };
  Money loan_repayment;
  Money loan_interest;
  Money insurance_premiums;
  std::vector<Rent> rents;
  std::vector<DepotRent> depot_rents;
  Money total_rent;
  Money investment_returns;
  Money total; // repayment, interest, insurance and rent
};
MonthlyCosts collect_monthly_costs(const GameState& gs, const Player& player);
void apply_monthly_costs(GameState& gs, Player& player,
                         const MonthlyCosts& costs);

struct WeeklyCityProfits {
  struct Entry {
    CityId city_id;
    Money profit;
    bool is_active;
  };
  std::vector<Entry> entries;
  Money total; // non-active cities only; the active city is settled live
};
WeeklyCityProfits collect_weekly_city_profits(const GameState& gs,
                                              const Player& player);
void apply_weekly_city_profits(GameState& gs, Player& player,
                               const WeeklyCityProfits& profits);

// Everything the end-of-week report chain posts across its popups: wages for
// each of the player's restaurants in the active city, their electricity
// invoice when the month ends with the week, then the other cities' profits.
// A run with no presenter shows none of those popups, so it posts them here.
void apply_weekly_settlement(GameState& gs, core::PlayerId player_id,
                             const std::vector<Restaurant*>& restaurants,
                             bool with_electricity);

} // namespace core::finance
