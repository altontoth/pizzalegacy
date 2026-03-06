/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/GameTypes.h"
#include "core/common/Ids.h"

#include <string>
#include <vector>

namespace init {
struct PlayerSlot {
  std::string name;
  bool is_selected{false};
  bool is_human{true};
  int character_id{};
  std::string character_name;
};

enum class RestaurantSize { NONE, SMALL, BIG };
enum class DifficultyLevel { PUSHOVER, EASY, NORMAL, DIFFICULT, IMPOSSIBLE };

struct Config {
  GameMode mode = GameMode::FREEMODE;
  RestaurantSize size = RestaurantSize::NONE;
  DifficultyLevel difficulty = DifficultyLevel::PUSHOVER;
  std::string chosen_character;
  core::CityId city_id{0};
  std::vector<PlayerSlot> players;
};

} // namespace init
