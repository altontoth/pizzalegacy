/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <optional>
#include <string>
#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/time/GameDate.h"
#include "core/time/GameTime.h"

enum class MafiaMissionType { Package, Attack, Weapon, Laundering, Investment };

struct MissionLocation {
  std::string description;
  int tile_index = -1;
  GameDate date{1995, 3, 1};
  int hour = 0;
  int minute = 0;

  int block_of_day() const { return (hour * 6) + (minute / 10); }

  // True when the current time is within 10 minutes of this location's time.
  bool is_active(const GameTime& time) const {
    return time.current_date() == date &&
           std::abs(time.current_block_of_day() - block_of_day()) <= 1;
  }

  // True when the time window has passed.
  bool has_passed(const GameTime& time) const {
    return time.current_date() > date ||
           (time.current_date() == date &&
            time.current_block_of_day() > block_of_day() + 1);
  }
};

struct MafiaMission {
  MafiaMissionType mission_type = MafiaMissionType::Package;
  bool active = false;
  int risk_pct = 0;
  Money reward{0};
  GameDate deadline{1995, 3, 1};
  int deadline_hour = 0;
  int deadline_minute = 0;
  bool completed = false;
  bool pickup_done = false;
  bool busted =
      false; // Pre-rolled on acceptance; if true, pickup triggers police
  int item_index = 0;
  int quantity = 0;
  Money cash_amount{0};

  // Which mafia clan gave the mission: the index of the actor within the
  // city's mafia actors (city->actors(ActorType::MAFIA)).
  int clan = 0;

  // The city the job was taken in (and the only one you can do it).
  core::CityId city{0};

  // Package/Weapon: pickup and dropoff locations with date/time
  MissionLocation pickup;
  MissionLocation dropoff;

  // Laundering: 4 bank account numbers and per-account completion flags
  std::array<int, 4> bank_accounts{};
  std::array<bool, 4> launder_done{};

  // Attack (I think unused in original): contamination target percentage
  int contamination_pct = 0;

  // Investment: profit displayed separately from reward
  Money profit{0};
};

struct MafiaLoan {
  Money amount{0};
  GameDate due_date{1995, 3, 1};
  // Mafia clan owed (ordinal within the city's mafia actors), so the repayment
  // call comes from the right face.
  int clan = 0;
};

struct MafiaPlayerState {
  std::optional<MafiaMission> active_mission;
  std::optional<MafiaLoan> loan;
};
