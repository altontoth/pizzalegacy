/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/MayorOffice.h"
#include "core/finance/BankAccountRegistry.h"

void MayorOffice::register_accounts(BankAccountRegistry& registry) {
  registry.register_account(public_bank_account_);
  registry.register_account(private_bank_account_);
}

void MayorOffice::reset_accounts(BankAccountRegistry& registry) {
  registry.close_account(public_bank_account_);
  registry.close_account(private_bank_account_);

  public_bank_account_ = engine::random::rand_max_n(999'999);
  private_bank_account_ = engine::random::rand_max_n(99'999'999);

  registry.register_account(public_bank_account_);
  registry.register_account(private_bank_account_);
}

void MayorOffice::end_of_day(const GameDate& date,
                             BankAccountRegistry& registry) {
  if (date.day() == 1) {
    reset_accounts(registry);
  }
}
