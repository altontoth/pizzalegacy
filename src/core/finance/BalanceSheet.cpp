/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/finance/BalanceSheet.h"

#include "BalanceSheet.h"

BalanceSheet::BalanceSheet(const Player& player, bool fix_legacy_bugs)
    : player_(player), fix_legacy_bugs_(fix_legacy_bugs) {}

std::vector<BalanceEntry> BalanceSheet::assets() const {
  std::vector<BalanceEntry> result;
  const auto& financials = player_.financials();

  result.push_back({Account::Property, financials.balance(Account::Property)});
  result.push_back({Account::Machines, financials.balance(Account::Machines)});
  result.push_back(
      {Account::Furniture, financials.balance(Account::Furniture)});
  result.push_back({Account::Weapons, financials.balance(Account::Weapons)});
  result.push_back({Account::Stock, financials.balance(Account::Stock)});
  result.push_back({Account::Cash, financials.balance(Account::Cash)});

  if (fix_legacy_bugs_) {
    result.push_back(
        {Account::Investments, financials.balance(Account::Investments)});
  }

  // Sum all revenue/expense accounts not shown on balance sheet
  // (excluding Machines, Furniture, Stock which are shown as assets)
  Money other_accounts = financials.balance(Account::Sales) +
                         financials.balance(Account::CoinMachines) +
                         financials.balance(Account::Missions) +
                         financials.balance(Account::Staff) +
                         financials.balance(Account::Rents) +
                         financials.balance(Account::Protection) +
                         financials.balance(Account::Repayment) +
                         financials.balance(Account::Others) +
                         financials.balance(Account::FinesAndBribes);

  // In legacy mode, investments are lumped into P&L (original behavior).
  // With fix_legacy_bugs, investments are shown as a separate asset line.
  if (!fix_legacy_bugs_) {
    other_accounts += financials.balance(Account::Investments);
  }

  // Add to ProfitAndLoss for balance sheet display
  Money total_profit_and_loss =
      financials.balance(Account::ProfitAndLoss) + other_accounts;

  // If positive (loss), show on asset side
  if (total_profit_and_loss.cents > 0) {
    result.push_back({Account::ProfitAndLoss, total_profit_and_loss});
  }

  return result;
}

std::vector<BalanceEntry> BalanceSheet::liabilities() const {
  std::vector<BalanceEntry> result;
  const auto& financials = player_.financials();

  // Negate balances since liabilities are stored as negative (credit) in ledger
  // but should display as positive on balance sheet.
  // Cheat is folded into Capital so debug money doesn't unbalance the sheet.
  Money capital = -financials.balance(Account::Capital) -
                  financials.balance(Account::Cheat);
  result.push_back({Account::Capital, capital});
  result.push_back({Account::Loan, -financials.balance(Account::Loan)});
  result.push_back({Account::Debts, -financials.balance(Account::Debts)});
  result.push_back(
      {Account::DeliveryDebts, -financials.balance(Account::DeliveryDebts)});

  // Sum all revenue/expense accounts not shown on balance sheet
  // (excluding Machines, Furniture, Stock which are shown as assets)
  Money other_accounts = financials.balance(Account::Sales) +
                         financials.balance(Account::CoinMachines) +
                         financials.balance(Account::Missions) +
                         financials.balance(Account::Staff) +
                         financials.balance(Account::Rents) +
                         financials.balance(Account::Protection) +
                         financials.balance(Account::Repayment) +
                         financials.balance(Account::Others) +
                         financials.balance(Account::FinesAndBribes);

  if (!fix_legacy_bugs_) {
    other_accounts += financials.balance(Account::Investments);
  }

  // Add to ProfitAndLoss for balance sheet display
  Money total_profit_and_loss =
      financials.balance(Account::ProfitAndLoss) + other_accounts;

  // If negative (profit), show on liabilities side
  if (total_profit_and_loss.cents < 0) {
    result.push_back({Account::ProfitAndLoss, -total_profit_and_loss});
  }

  return result;
}

Money BalanceSheet::total_assets() const {
  Money total = 0_c;
  for (const auto& entry : assets()) {
    total += entry.amount;
  }
  return total;
}

Money BalanceSheet::total_liabilities() const {
  Money total = 0_c;
  for (const auto& entry : liabilities()) {
    total += entry.amount;
  }
  return total;
}
