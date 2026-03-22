/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <unordered_map>
#include "core/common/Ids.h"
#include "core/finance/Money.h"

// A registered bank account. Player accounts route deposits to a player's
// cash balance; dynamic accounts (mayor bribes, mafia laundering) just
// accumulate a running total that the owning system can query.
struct BankAccount {
  int number;
  std::optional<core::PlayerId> player_id;
  Money deposited{0};
};

class BankAccountRegistry {
public:
  // Fixed player accounts (111111 for player 0, 222222 for player 1, etc.)
  void register_player_account(core::PlayerId pid);

  // Dynamic accounts for mayor, mafia, etc.
  void register_account(int number);
  void close_account(int number);

  bool exists(int number) const;
  const BankAccount* find(int number) const;
  BankAccount* find(int number);

  // Record a deposit into the account (updates deposited total).
  // Does NOT handle debiting the sender — caller is responsible for that.
  void deposit(int account_number, Money amount);

  static int player_account_number(core::PlayerId pid);

private:
  std::unordered_map<int, BankAccount> accounts_;
};
