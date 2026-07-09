/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

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
};
