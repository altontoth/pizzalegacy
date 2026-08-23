/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/character/Player.h"

struct BalanceEntry {
  Account account;
  Money amount;
};

class BalanceSheet {
  friend std::ostream& operator<<(std::ostream& os, const BalanceSheet& bs) {
    return os;
  }

public:
  BalanceSheet(const Player&, bool fix_legacy_bugs = false);
  std::vector<BalanceEntry> assets() const;
  std::vector<BalanceEntry> liabilities() const;
  // The year's result, gathering every revenue and expense account into the
  // one figure the sheet shows. Negative is a profit, following the ledger's
  // sign, and lands on the liabilities side; a loss shows among the assets.
  Money profit_and_loss() const;
  Money total_assets() const;
  Money total_liabilities() const;

private:
  const Player& player_;
  bool fix_legacy_bugs_;
};
