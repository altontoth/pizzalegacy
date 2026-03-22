/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/time/GameDate.h"
#include "engine/Random.h"

class BankAccountRegistry;

class MayorOffice {
public:
  void register_accounts(BankAccountRegistry& registry);
  void reset_accounts(BankAccountRegistry& registry);
  void end_of_day(const GameDate& date, BankAccountRegistry& registry);

  int public_bank_account() const { return public_bank_account_; }
  int private_bank_account() const { return private_bank_account_; }

private:
  int public_bank_account_{engine::random::rand_max_n(999'999)};
  int private_bank_account_{engine::random::rand_max_n(99'999'999)};
};
