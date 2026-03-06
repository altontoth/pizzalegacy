/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <optional>
#include "core/common/Ids.h"
#include "core/finance/Money.h"

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

struct NewspaperAd {
  std::array<int, 5> sentence_choices; // indices of selected phrases
  float effectiveness;                 // derived from choices
};

struct AdvertisingCampaign {
  AdvertisingChannel channel;
  core::CityId city_id;
  int days_remaining;
  AdvertisingRate rate;

  // Optional specific data
  std::optional<int> flyer_count;
  std::optional<int> poster_id;
  // std::optional<NewspaperAd> newspaper_data;
  std::optional<int> tv_ad_id;
};
