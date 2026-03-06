/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <iomanip>
#include <sstream>
#include <string>

class MoneyFormatter {
public:
  MoneyFormatter(std::string symbol, bool postfix)
      : symbol_(std::move(symbol)), postfix_(postfix) {}

  std::string format(int64_t cents) const {
    std::ostringstream ss;
    int64_t abs = std::abs(cents);
    int64_t dollars = abs / 100;
    int64_t remainder = abs % 100;

    if (!postfix_) {
      ss << symbol_;
    }
    ss << (cents < 0 ? "-" : "") << dollars;
    if (remainder > 0) {
      ss << '.' << std::setw(2) << std::setfill('0') << remainder;
    }
    // else {
    //   ss << ".-";
    // }

    if (postfix_) {
      ss << ' ' << symbol_;
    }
    return ss.str();
  }

private:
  std::string symbol_;
  bool postfix_;
};
