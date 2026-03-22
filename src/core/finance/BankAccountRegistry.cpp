/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/finance/BankAccountRegistry.h"

void BankAccountRegistry::register_player_account(core::PlayerId pid) {
  int number = player_account_number(pid);
  accounts_[number] = BankAccount{number, pid, Money{0}};
}

void BankAccountRegistry::register_account(int number) {
  accounts_[number] = BankAccount{number, std::nullopt, Money{0}};
}

void BankAccountRegistry::close_account(int number) { accounts_.erase(number); }

bool BankAccountRegistry::exists(int number) const {
  return accounts_.contains(number);
}

const BankAccount* BankAccountRegistry::find(int number) const {
  auto it = accounts_.find(number);
  return it != accounts_.end() ? &it->second : nullptr;
}

BankAccount* BankAccountRegistry::find(int number) {
  auto it = accounts_.find(number);
  return it != accounts_.end() ? &it->second : nullptr;
}

void BankAccountRegistry::deposit(int account_number, Money amount) {
  auto it = accounts_.find(account_number);
  if (it != accounts_.end()) {
    it->second.deposited += amount;
  }
}

int BankAccountRegistry::player_account_number(core::PlayerId pid) {
  // Player 0 → 111111, Player 1 → 222222, ..., Player 7 → 888888
  int digit = pid.value + 1;
  return digit * 111111;
}
