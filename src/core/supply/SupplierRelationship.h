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
  // 0-100, affects discount
  uint8_t relationship_score{0};
  uint8_t frustration{0};
  uint8_t last_delivery_days{28};
  uint16_t last_payment_freq{0};
  // set to 30 on missed delivery, counts down daily
  uint8_t delivery_issues_flag{0};
  // unpaid amount owed to this supplier
  Money outstanding_debt{0};
  // order value from the previous accepted deal
  Money last_contract_sum{0};
  // spending_rank at time of missed payment; decays daily
  uint8_t missed_payment_penalty{0};
};
