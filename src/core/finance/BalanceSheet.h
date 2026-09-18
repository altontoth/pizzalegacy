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
  explicit BalanceSheet(const Player&);
  std::vector<BalanceEntry> assets() const;
  std::vector<BalanceEntry> liabilities() const;
  // This year's profit or loss as the ledger holds it. It is a credit, so a
  // profit comes out negative and shows among the liabilities, while a loss
  // shows among the assets.
  Money profit_and_loss() const;
  Money total_assets() const;
  Money total_liabilities() const;

private:
  const Player& player_;
};
