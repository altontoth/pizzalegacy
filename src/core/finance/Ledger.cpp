/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/finance/Ledger.h"

void Ledger::post(const JournalEntry& je) {
  Money sum = 0_usd;
  for (auto& p : je.lines) {
    sum += p.amount;
  }
  if (sum != 0_usd) {
    throw std::logic_error("Unbalanced JE");
  }
  journal_.push_back(je);
}

Money Ledger::balance(Account acct,
                      std::optional<core::RestaurantId> restaurant,
                      std::optional<AccountingScope> scope) const {
  Money total = 0_c;

  for (const auto& je : journal_) {
    for (const auto& line : je.lines) {
      if (line.account == acct &&
          (!restaurant || line.restaurant_id == restaurant) &&
          (!scope || line.scope == *scope)) {
        total += line.amount;
      }
    }
  }

  return total;
}

std::vector<const JournalEntry*> Ledger::between(GameDate from,
                                                 GameDate to) const noexcept {

  std::vector<const JournalEntry*> results;

  for (const auto& je : journal_) {
    if (je.date >= from && je.date <= to) {
      results.push_back(&je);
    }
  }

  return results;
}

Money Ledger::balance_between(Account acct, GameDate from, GameDate to,
                              std::optional<core::RestaurantId> restaurant,
                              std::optional<AccountingScope> scope) const {
  Money total = 0;
  for (const auto& entry : journal_) {
    if (entry.date >= from && entry.date <= to) {
      for (const auto& posting : entry.lines) {
        if (posting.account == acct &&
            (!restaurant || posting.restaurant_id == restaurant) &&
            (!scope || posting.scope == *scope)) {
          total += posting.amount;
        }
      }
    }
  }
  return total;
}
