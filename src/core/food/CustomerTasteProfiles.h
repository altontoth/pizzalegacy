/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/Demographics.h"

namespace core::food {

// all values 0 - 9
struct TasteProfile {
  int fat = 0;
  int sweet = 0;
  int salty = 0;
  int hot = 0;
  int spicy = 0; // display-only (not used in scoring)
};

using CustomerTasteProfiles = DemographicArray<TasteProfile>;

// Default taste profiles
inline CustomerTasteProfiles default_customer_taste_profiles() {
  CustomerTasteProfiles p;
  // clang-format off
  p[ConsumerType::Children]       = {.fat=3,  .sweet=10, .salty=3, .hot=1,  .spicy=2};
  p[ConsumerType::YoungPeople]    = {.fat=5,  .sweet=6,  .salty=5, .hot=3,  .spicy=4};
  p[ConsumerType::BlueCollar]     = {.fat=7,  .sweet=1,  .salty=5, .hot=10, .spicy=9};
  p[ConsumerType::WhiteCollar]    = {.fat=6,  .sweet=3,  .salty=9, .hot=6,  .spicy=7};
  p[ConsumerType::BusinessPeople] = {.fat=4,  .sweet=4,  .salty=8, .hot=1,  .spicy=7};
  p[ConsumerType::SeniorCitizens] = {.fat=8,  .sweet=0,  .salty=7, .hot=10, .spicy=9};
  // clang-format on
  return p;
}

} // namespace core::food
