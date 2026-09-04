/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/finance/TurnInvoices.h"

#include "core/character/Player.h"
#include "core/character/Staff.h"
#include "core/city/City.h"
#include "core/city/Property.h"
#include "core/finance/Ledger.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"

namespace core::finance {

StaffWages collect_staff_wages(const Restaurant& rest) {
  StaffWages wages;
  auto add = [&wages](const Staff& staff) {
    wages.entries.push_back(
        {staff.character().first_name() + " " + staff.character().last_name(),
         staff.payment()});
    wages.total += staff.payment();
  };
  for (const auto& cook : rest.cooks()) {
    if (cook) {
      add(*cook);
    }
  }
  for (const auto& waiter : rest.waiters()) {
    if (waiter) {
      add(*waiter);
    }
  }
  if (rest.manager()) {
    add(*rest.manager());
  }
  return wages;
}

void apply_daily_sales(GameState& gs, Restaurant& rest) {
  Player* owner = gs.restaurant_owner(rest);
  if (!owner || !owner->is_human_controlled() || !rest.wants_open() ||
      rest.is_force_closed()) {
    return;
  }
  const auto& day = rest.stats().yesterday;
  const auto& date = gs.time().current_date();
  owner->financials().transfer(date, Account::Sales, Account::Cash, day.revenue,
                               rest.id(), AccountingScope::Operational);
  // The ingredients that were used during the day leave the stock account.
  owner->financials().transfer(date, Account::Stock, Account::ProfitAndLoss,
                               day.ingredient_costs, rest.id(),
                               AccountingScope::Operational);
  owner->add_turnover(rest.property().city().id(), day.revenue);
  gs.status().refresh_header();
}

ElectricitySlotInvoice
collect_electricity_slot_invoice(const Restaurant& rest) {
  Money electricity = rest.calculate_electricity_cost();
  Money slot_income = rest.calculate_slot_machine_income();
  return {rest.id(), electricity, slot_income, electricity - slot_income};
}

void apply_electricity_slot_invoice(GameState& gs, Player& player,
                                    const ElectricitySlotInvoice& invoice) {
  const auto& date = gs.time().current_date();
  if (invoice.electricity.cents > 0) {
    player.financials().transfer(date, Account::Cash, Account::Machines,
                                 invoice.electricity, invoice.restaurant_id,
                                 AccountingScope::Operational);
  }
  if (invoice.slot_income.cents > 0) {
    player.financials().transfer(date, Account::CoinMachines, Account::Cash,
                                 invoice.slot_income, invoice.restaurant_id,
                                 AccountingScope::Operational);
  }
  gs.status().refresh_header();
}

MonthlyCosts collect_monthly_costs(const GameState& gs, const Player& player) {
  MonthlyCosts costs;

  auto loan_costs = player.monthly_loan_costs();
  costs.loan_repayment = loan_costs.repayment;
  costs.loan_interest = loan_costs.interest;
  costs.insurance_premiums = player.monthly_insurance_premiums();

  // Collect rent from all rented restaurants
  for (auto* rest : gs.restaurants_for(player.id())) {
    if (rest && rest->property().is_rented()) {
      Money rent = rest->property().get_monthly_rent();
      if (rent.cents > 0) {
        costs.rents.push_back({rest->id(), rent});
        costs.total_rent += rent;
      }
    }
  }

  // Collect rent from all rented depots
  for (auto* depot : gs.real_estate().depots_for(player.id())) {
    if (depot && depot->is_rented()) {
      Money rent = depot->get_monthly_rent();
      if (rent.cents > 0) {
        costs.depot_rents.push_back({depot->city().id(), rent});
        costs.total_rent += rent;
      }
    }
  }

  costs.investment_returns = player.monthly_investment_returns();
  costs.investment_payout = player.monthly_investment_payout();
  costs.total = costs.loan_repayment + costs.loan_interest +
                costs.insurance_premiums + costs.total_rent -
                costs.investment_returns - costs.investment_payout;
  return costs;
}

void apply_monthly_costs(GameState& gs, Player& player,
                         const MonthlyCosts& costs) {
  const auto& date = gs.time().current_date();
  player.apply_monthly_loan_payments(date);
  player.apply_monthly_insurance_premiums(date);
  player.apply_monthly_investment_payouts(date);
  for (const auto& rent : costs.rents) {
    player.financials().transfer(date, Account::Cash, Account::Rents,
                                 rent.amount, rent.restaurant_id,
                                 AccountingScope::Operational);
  }
  for (const auto& rent : costs.depot_rents) {
    player.financials().transfer(date, Account::Cash, Account::Rents,
                                 rent.amount, AccountingScope::Operational);
  }
  gs.status().refresh_header();
}

WeeklyCityProfits collect_weekly_city_profits(const GameState& gs,
                                              const Player& player) {
  WeeklyCityProfits profits;
  auto restaurants = gs.restaurants_for(player.id());
  for (auto* c : gs.visited_cities(player)) {
    core::CityId city_id = c->id();
    bool is_active = (city_id == gs.active_city_id());

    Money city_profit;
    if (!is_active) {
      for (auto* rest : restaurants) {
        if (rest && rest->property().city().id() == city_id) {
          city_profit = city_profit + rest->last_weekly_profit();
        }
      }
      profits.total = profits.total + city_profit;
    }

    profits.entries.push_back({city_id, city_profit, is_active});
  }
  return profits;
}

void apply_weekly_city_profits(GameState& gs, Player& player,
                               const WeeklyCityProfits& profits) {
  const auto& date = gs.time().current_date();
  for (const auto& entry : profits.entries) {
    if (entry.is_active || entry.profit.cents == 0) {
      continue;
    }
    if (entry.profit.cents > 0) {
      player.financials().transfer(date, Account::Sales, Account::Cash,
                                   entry.profit, AccountingScope::Operational);
    }
    else {
      player.financials().transfer(date, Account::Cash, Account::Sales,
                                   -entry.profit, AccountingScope::Operational);
    }
  }
  gs.status().refresh_header();
}

void apply_weekly_settlement(GameState& gs, core::PlayerId player_id,
                             const std::vector<Restaurant*>& restaurants,
                             bool with_electricity) {
  Player* player = gs.player(player_id);
  if (!player) {
    return;
  }
  const auto* active_city = gs.active_city();
  for (Restaurant* rest : restaurants) {
    if (!rest || !active_city ||
        rest->property().city().id() != active_city->id()) {
      continue;
    }
    gs.deduct_staff_wages_for_restaurant(rest->id());
    if (with_electricity) {
      apply_electricity_slot_invoice(gs, *player,
                                     collect_electricity_slot_invoice(*rest));
    }
  }
  apply_weekly_city_profits(gs, *player,
                            collect_weekly_city_profits(gs, *player));
}

} // namespace core::finance
