/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <vector>
#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/marketing/NewspaperAdFormat.h"

constexpr int NUMBER_OF_POSTERS = 9;
constexpr int NUMBER_OF_TV_ADS = 9;

struct AdvertisingRate {
  int effectiveness{0}; // percentage
  Money cost{0};
};

enum class AdvertisingChannel {
  FLYERS,
  POSTERS,
  NEWSPAPER_ADS,
  TV_ADS,
};

struct AdvertisingCampaign {
  AdvertisingChannel channel;
  core::CityId city_id;
  int days_remaining;
  AdvertisingRate rate;

  // Optional specific data
  std::optional<int> flyer_count;
  std::optional<int> poster_id;
  std::optional<NewspaperAdFormat> newspaper_ad_format;
  std::vector<std::string> newspaper_lines;
  std::optional<int> tv_ad_id;
  // Enhanced contracts are stored separately from the legacy city slots.
  bool enhanced{false};
  std::optional<core::RestaurantId> target_restaurant;
  int awareness{0}; // residual demand bonus, in percentage points
};

inline bool local_advertising(AdvertisingChannel channel) {
  return channel == AdvertisingChannel::FLYERS ||
         channel == AdvertisingChannel::POSTERS;
}

// Prices are fixed at purchase. Existing contracts retain their agreed rate.
inline AdvertisingRate enhanced_advertising_rate(AdvertisingChannel channel,
                                                AdvertisingRate original,
                                                int locations) {
  const int extra = std::max(0, locations - 1);
  switch (channel) {
  case AdvertisingChannel::FLYERS:
    return {original.effectiveness, Money{original.cost.cents / 5}};
  case AdvertisingChannel::POSTERS:
    return {original.effectiveness, Money::from_dollars(
        std::clamp(40 + (original.effectiveness - 30) * 80 / 29, 40, 120))};
  case AdvertisingChannel::NEWSPAPER_ADS:
    return {original.effectiveness, Money::from_dollars(
        std::clamp(static_cast<int>(original.cost.whole()) / 5, 100, 400) + 50 * extra)};
  case AdvertisingChannel::TV_ADS:
    return {original.effectiveness, Money::from_dollars(
        std::clamp(300 + (original.effectiveness - 50) * 700 / 49, 300, 1000) + 100 * extra)};
  }
  return original;
}

inline int enhanced_advertising_awareness(const AdvertisingCampaign& c) {
  switch (c.channel) {
  case AdvertisingChannel::FLYERS: return std::clamp(c.rate.effectiveness * 3, 0, 30);
  case AdvertisingChannel::POSTERS: return std::clamp(c.rate.effectiveness / 2, 0, 30);
  case AdvertisingChannel::NEWSPAPER_ADS: return std::clamp(c.rate.effectiveness, 0, 40);
  case AdvertisingChannel::TV_ADS: return std::clamp(c.rate.effectiveness / 2, 0, 50);
  }
  return 0;
}
