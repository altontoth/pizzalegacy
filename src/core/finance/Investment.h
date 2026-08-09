/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include "core/common/Ids.h"
#include "core/finance/Money.h"

// Investment payout sequence:
//  1. Month you invest: slot created with months_left = N (default 2)
//  2. End of that month: interest paid, months_left decremented to N-1
//  3. Repeat until months_left reaches 0
//  4. Next end-of-month after that: months_left==0 detected, principal
//     returned, slot cleared
// So N months of interest are always paid, but calendar time until principal
// payout is N to N+1 months depending on when in the month you invested.
// Investing on day 1 of the month means your money is tied up longer than if
// you invest on the 30th of the month.
//
// With fix_legacy_bugs: final interest + principal are paid together in step 3
// when months_left hits 0, and the slot is cleared immediately. No extra month
// of waiting for the principal.
class Investment {
public:
  Investment() = default;
  Investment(core::BankId bank_id, core::CityId city_id, Money amount,
             uint8_t return_rate, int months)
      : bank_id_(bank_id), city_id_(city_id), amount_(amount),
        return_rate_(return_rate), months_(months), months_left_(months) {
    Money total_return = amount * return_rate / 100;
    monthly_return_ = (total_return / months).truncated_to_dollars();
  }

  static Investment from_save(core::BankId bank_id, core::CityId city_id,
                              Money amount, int return_rate, int months,
                              int months_left, Money monthly_return) {
    Investment inv;
    inv.bank_id_ = bank_id;
    inv.city_id_ = city_id;
    inv.amount_ = amount;
    inv.return_rate_ = return_rate;
    inv.months_ = months;
    inv.months_left_ = months_left;
    inv.monthly_return_ = monthly_return;
    return inv;
  }

  void end_of_month() {
    if (months_left_ > 0) {
      months_left_ -= 1;
    }
  }

  core::BankId bank_id() const { return bank_id_; }
  core::CityId city_id() const { return city_id_; }
  Money amount() const { return amount_; }
  int return_rate() const { return return_rate_; }
  int start_months() const { return months_; }
  int months_left() const { return months_left_; }
  Money monthly_return() const { return monthly_return_; }
  bool is_matured() const { return months_left_ == 0; }

private:
  core::BankId bank_id_{-1};
  core::CityId city_id_{-1};
  Money amount_{0};
  int return_rate_{0};
  int months_{0};
  int months_left_{0};
  Money monthly_return_{0};
};
