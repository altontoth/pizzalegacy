/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include "core/finance/Money.h"

struct SupplierRelationship {
  Money total_spent{0};
  int contract_count{0};
  uint8_t relationship_score{0}; // 0-100, affects discount
  uint8_t frustration{0};
  uint8_t last_delivery_days{28};
  uint16_t last_payment_freq{0};
  uint8_t credit_history{0}; // 0 = no issues
};
