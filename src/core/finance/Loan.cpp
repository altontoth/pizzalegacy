/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/finance/Loan.h"

std::ostream& operator<<(std::ostream& os, const Loan& loan) {
  os << "Loan:\n"
     << "  Bank ID: " << loan.bank_id_ << "\n"
     << "  Start Amount: $" << loan.start_amount_ << "\n"
     << "  Interest Rate: " << static_cast<int>(loan.interest_percentage_)
     << "%\n"
     << "  Start Months: " << loan.start_months_ << "\n"
     << "  Months Left: " << loan.months_left_ << "\n"
     << "  Monthly Principal: $" << loan.monthly_principal_ << "\n"
     << "  Monthly Interest: $" << loan.monthly_interest_ << "\n"
     << "  Monthly Payment: $" << loan.monthly_repayment() << "\n";
  return os;
}
