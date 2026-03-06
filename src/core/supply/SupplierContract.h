/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include "core/finance/Money.h"

struct SupplierContract {
  uint8_t discount_score{0}; // 0-60, discount percentage
  uint8_t delivery_days{28}; // Delivery period in days
  Money charge{0};           // Service charge/fee
  Money penalty{0};          // Late delivery penalty amount
  Money total_value{0};      // Total contract value (sum)
};
