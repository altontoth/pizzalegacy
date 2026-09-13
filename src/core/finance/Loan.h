/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include "core/common/Ids.h"
#include "core/finance/Money.h"

class Loan {
  friend std::ostream& operator<<(std::ostream& os, const Loan& loan);

public:
  Loan() = default;
  Loan(core::BankId bank_id, Money start_amount, uint8_t interest_percentage,
       uint8_t start_months)
      : bank_id_(bank_id), start_amount_(start_amount),
        interest_percentage_(interest_percentage), start_months_(start_months),
        months_left_(start_months) {
    Money total_interest = start_amount * interest_percentage / 100;
    monthly_principal_ = (start_amount / start_months).truncated_to_dollars();
    monthly_interest_ = (total_interest / start_months).truncated_to_dollars();
  }

  // Restore full state from save data (bypasses payment recalculation)
  static Loan from_save(core::BankId bank_id, Money start_amount,
                        uint8_t interest_percentage, int start_months,
                        int months_left, Money monthly_principal,
                        Money monthly_interest) {
    Loan loan;
    loan.bank_id_ = bank_id;
    loan.start_amount_ = start_amount;
    loan.interest_percentage_ = interest_percentage;
    loan.start_months_ = start_months;
    loan.months_left_ = months_left;
    loan.monthly_principal_ = monthly_principal;
    loan.monthly_interest_ = monthly_interest;
    return loan;
  }

  void end_of_month() {
    if (months_left_ > 0) {
      months_left_ -= 1;
    }
  }

  Money monthly_repayment() const {
    return monthly_principal_ + monthly_interest_;
  }

  Money monthly_interest() const { return monthly_interest_; }

  Money monthly_principal() const { return monthly_principal_; }

  core::BankId bank_id() const { return bank_id_; }

  bool is_settled() const { return months_left_ == 0; }
  int months_left() const { return months_left_; }

  Money repay_amount(bool settle_rounding) const {
    return repay_principal(settle_rounding) +
           (months_left_ * monthly_interest_);
  }
  Money repay_principal(bool settle_rounding) const {
    if (settle_rounding) {
      return remaining_principal();
    }
    return months_left_ * monthly_principal_;
  }

  Money principal_due(bool settle_rounding) const {
    if (settle_rounding && months_left_ == 1) {
      return remaining_principal();
    }
    return monthly_principal_;
  }

  // Includes the dollars lost by rounding the monthly principal down.
  Money remaining_principal() const {
    return start_amount_ -
           ((start_months_ - months_left_) * monthly_principal_);
  }

  void repay() { months_left_ = 0; }

  Money start_amount() const { return start_amount_; }

  int start_months() const { return start_months_; }

  uint8_t interest_percentage() const { return interest_percentage_; }

private:
  core::BankId bank_id_{-1};
  Money start_amount_{0};
  uint8_t interest_percentage_{0};
  int start_months_{0};
  int months_left_{0};
  Money monthly_principal_{0};
  Money monthly_interest_{0};
};
