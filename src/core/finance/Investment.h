/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include "core/finance/Money.h"

class Investment {
public:
  Investment() = default;
  Investment(Money amount, uint8_t return_rate, int months)
      : amount_(amount), return_rate_(return_rate), months_(months),
        months_left_(months) {
    Money total_return = amount * return_rate / 100;
    monthly_return_ = Money{(total_return / months).whole() * 100};
  }

  void end_of_month() {
    if (months_left_ > 0) {
      months_left_ -= 1;
    }
  }

  Money amount() const { return amount_; }
  uint8_t return_rate() const { return return_rate_; }
  int start_months() const { return months_; }
  int months_left() const { return months_left_; }
  Money monthly_return() const { return monthly_return_; }
  bool is_matured() const { return months_left_ == 0; }

private:
  Money amount_{0};
  int return_rate_{0};
  int months_{0};
  int months_left_{0};
  Money monthly_return_{0};
};
