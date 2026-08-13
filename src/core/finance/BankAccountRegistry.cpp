/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/finance/BankAccountRegistry.h"

#include <algorithm>

// An account that is already there keeps its deposit total.
void BankAccountRegistry::register_player_account(core::PlayerId pid) {
  int number = player_account_number(pid);
  BankAccount& account = accounts_[number];
  account.number = number;
  account.player_id = pid;
}

void BankAccountRegistry::register_account(int number) {
  BankAccount& account = accounts_[number];
  account.number = number;
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

std::vector<BankAccount> BankAccountRegistry::accounts_sorted() const {
  std::vector<BankAccount> sorted;
  sorted.reserve(accounts_.size());
  for (const auto& [number, account] : accounts_) {
    sorted.push_back(account);
  }
  std::sort(sorted.begin(), sorted.end(),
            [](const BankAccount& a, const BankAccount& b) {
              return a.number < b.number;
            });
  return sorted;
}

void BankAccountRegistry::restore_account(const BankAccount& account) {
  accounts_[account.number] = account;
}

int BankAccountRegistry::player_account_number(core::PlayerId pid) {
  // Player 0 gets 111111, player 1 gets 222222, up to player 7 with 888888.
  int digit = pid.value + 1;
  return digit * 111111;
}
