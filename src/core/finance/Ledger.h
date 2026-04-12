/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/time/GameDate.h"

enum class AccountingScope : uint8_t {
  Capital,    // Balance sheet only (initial investments, property purchases)
  Operational // Both balance sheet and monthly overview (ongoing expenses)
};

enum class Account {
  // Profit & Loss:
  Sales,
  CoinMachines,
  Missions,
  Staff,
  Rents,
  Stock,
  Machines,
  Furniture,
  Protection,
  Repayment,
  Investments,
  Others,
  // Balance sheet:
  Cash,
  Capital,
  ProfitAndLoss,
  Loan,
  Property,
  Weapons,
  // Off the books:
  FinesAndBribes,
  // Don't quite understand these yet:
  Debts,
  DeliveryDebts,
  // Not in the original, added for debug console:
  Cheat,
};

// Free function for converting Account to string
inline std::string to_string(Account account) {
  switch (account) {
  case Account::Property:
    return "property";
  case Account::Staff:
    return "staff";
  case Account::Rents:
    return "rents";
  case Account::Stock:
    return "stock";
  case Account::Protection:
    return "protection";
  case Account::Repayment:
    return "repayment";
  case Account::Others:
    return "others";
  case Account::Sales:
    return "sales";
  case Account::CoinMachines:
    return "coin machines";
  case Account::Missions:
    return "missions";
  case Account::Machines:
    return "machines";
  case Account::Furniture:
    return "furniture";
  case Account::Cash:
    return "cash";
  case Account::Capital:
    return "capital";
  case Account::ProfitAndLoss:
    return "profit and loss";
  case Account::Loan:
    return "loan";
  case Account::FinesAndBribes:
    return "fines and bribes";
  case Account::Weapons:
    return "weapons";
  case Account::DeliveryDebts:
    return "delivery debts";
  case Account::Debts:
    return "debts";
  case Account::Cheat:
    return "cheat";
  case Account::Investments:
    return "investments";
  }
  return "unknown";
}

// Free function for converting string to Account
inline std::optional<Account> string_to_account(const std::string& str) {
  // Static map for efficient lookup
  static const std::unordered_map<std::string, Account> account_map = {
      {"property", Account::Property},
      {"staff", Account::Staff},
      {"rents", Account::Rents},
      {"stock", Account::Stock},
      {"protection", Account::Protection},
      {"repayment", Account::Repayment},
      {"others", Account::Others},
      {"sales", Account::Sales},
      {"coin machines", Account::CoinMachines},
      {"missions", Account::Missions},
      {"machines", Account::Machines},
      {"furniture", Account::Furniture},
      {"cash", Account::Cash},
      {"capital", Account::Capital},
      {"profit and loss", Account::ProfitAndLoss},
      {"profit", Account::ProfitAndLoss}, // Alias for compatibility
      {"loan", Account::Loan},
      {"fines and bribes", Account::FinesAndBribes},
      {"weapons", Account::Weapons},
      {"delivery debts", Account::DeliveryDebts},
      {"debts", Account::Debts},
      {"cheat", Account::Cheat}};

  auto it = account_map.find(str);
  if (it != account_map.end()) {
    return it->second;
  }
  return std::nullopt;
}

struct Posting {
  Account account;
  Money amount; // plus = debit, minus = credit
  std::optional<core::RestaurantId> restaurant_id;
  AccountingScope scope;

  // Constructor with restaurant_id and scope
  Posting(Account acc, Money amt,
          std::optional<core::RestaurantId> rest_id = std::nullopt,
          AccountingScope sc = AccountingScope::Operational)
      : account(acc), amount(amt), restaurant_id(rest_id), scope(sc) {}
};

struct JournalEntry {
  GameDate date;
  std::vector<Posting> lines;
};

class Ledger {
  friend std::ostream& operator<<(std::ostream& os, const Ledger& ledger) {
    for (const auto& entry : ledger.journal_) {
      os << entry.date << ":\n";
      for (const auto& line : entry.lines) {
        os << "  " << std::setw(16) << to_string(line.account) << "  "
           << std::setw(10) << line.amount;
        if (line.restaurant_id.has_value()) {
          os << "  (restaurant #" << line.restaurant_id.value() << ")";
        }
        os << "\n";
      }
    }
    return os;
  }

public:
  void post(const JournalEntry& je);
  Money balance(Account acct,
                std::optional<core::RestaurantId> restaurant = std::nullopt,
                std::optional<AccountingScope> scope = std::nullopt) const;
  std::vector<const JournalEntry*> between(GameDate from,
                                           GameDate to) const noexcept;
  Money
  balance_between(Account acct, GameDate from, GameDate to,
                  std::optional<core::RestaurantId> restaurant = std::nullopt,
                  std::optional<AccountingScope> scope = std::nullopt) const;

  const std::vector<JournalEntry>& entries() const noexcept { return journal_; }

private:
  std::vector<JournalEntry> journal_;
};
