/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <optional>
#include <string>
#include "core/finance/Money.h"

enum class InsuranceType { THEFT = 0, WAREHOUSE, FIRE, LIABILITY, LIFE };

struct InsurancePolicy {
  int actor_id = -1;
  std::string company_name;
  Money monthly_premium{0};
  Money max_coverage{0};
  bool is_active() const { return actor_id >= 0; }
};

struct InsurancePlayerState {
  std::array<InsurancePolicy, 5> policies{};
  std::optional<InsuranceType> pending_claim{};
};
