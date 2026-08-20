/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/events/CareerLog.h"

#include "engine/Logger.h"
#include "resources/util/FileUtils.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

namespace core::events {

using ZW = ZwBackground;
constexpr std::array<EventData, max_milestone_id + 1> milestone_data = {{
    // unused
    {ZwBackground::NONE, 0},

    {ZW::NONE, 500},        // [1]  Laughing stock
    {ZW::NONE, 1000},       // [2]  Failure
    {ZW::POSITION, 2000},   // [3]  Worker
    {ZW::POSITION, 5000},   // [4]  Small Entrepreneur
    {ZW::POSITION, 7000},   // [5]  Restaurateur
    {ZW::POSITION, 20000},  // [6]  Businessman
    {ZW::POSITION, 40000},  // [7]  Self-made Man
    {ZW::POSITION, 65000},  // [8]  Hero
    {ZW::POSITION, 85000},  // [9]  Star
    {ZW::POSITION, 120000}, // [10] Freeman
    {ZW::POSITION, 250000}, // [11] Mayor
    {ZW::POSITION, 400000}, // [12] Senator
    {ZW::POSITION, 600000}, // [13] President
    {ZW::POSITION, 800000}, // [14] Tycoon

    {ZW::NONE, 500},          // [15] as good as dead
    {ZW::NONE, 1000},         // [16] loser
    {ZW::KIDNAPPED, 2700},    // [17] Amateur
    {ZW::KIDNAPPED, 4500},    // [18] Gambler
    {ZW::KIDNAPPED, 8000},    // [19] Pickpocket
    {ZW::KIDNAPPED, 28000},   // [20] Thug
    {ZW::KIDNAPPED, 42000},   // [21] Gangster
    {ZW::KIDNAPPED, 75400},   // [22] Racketeer
    {ZW::KIDNAPPED, 200000},  // [23] Professional
    {ZW::KIDNAPPED, 540000},  // [24] Killer
    {ZW::KIDNAPPED, 870000},  // [25] Boss
    {ZW::KIDNAPPED, 1000000}, // [26] Mafia Big Wig
    {ZW::KIDNAPPED, 0},       // [27] Godfather

    // UI header lines
    {ZW::NONE, 0}, // [28] Career
    {ZW::NONE, 0}, // [29] Target career

    {ZW::RESTAURANT, 2000},   // [30] Opens 1st restaurant
    {ZW::NONE, 500},          // [31] Opens 2nd restaurant
    {ZW::NONE, 700},          // [32] Takes out 1st loan
    {ZW::BEGGING, 500},       // [33] In financial difficulties
    {ZW::NONE, 5000},         // [34] Has 5 restaurants
    {ZW::BALANCE, 500},       // [35] 1000 pizzas sold in one day
    {ZW::BALANCE, 800},       // [36] 10000 pizzas sold in one week
    {ZW::BALANCE, 1000},      // [37] 10% market share
    {ZW::BALANCE, 3000},      // [38] 20% market share
    {ZW::BALANCE, 5000},      // [39] 50% market share
    {ZW::BALANCE, 750},       // [40] 1st star awarded
    {ZW::NONE, 1200},         // [41] 1st 2-star restaurant
    {ZW::NONE, 3000},         // [42] 1st 3-star restaurant
    {ZW::NONE, 5000},         // [43] 1st 4-star restaurant
    {ZW::NONE, 8500},         // [44] 1st 5-star restaurant
    {ZW::BALANCE, 4000},      // [45] $10000 turnover in one day
    {ZW::PIZZA, 500},         // [46] 1st pizza created
    {ZW::PIZZA, 50000},       // [47] Pizza #1 in sales charts
    {ZW::RESTAURANT, 300},    // [48] 100% deliveries for a week
    {ZW::RESTAURANT, 70000},  // [49] Service time under 4 minutes
    {ZW::RESTAURANT, 50000},  // [50] 1st restaurant in another city
    {ZW::RESTAURANT, 100000}, // [51] Outlets in all cities
    {ZW::NONE, 8000},         // [52] Assets $100k
    {ZW::RICH, 26000},        // [53] Assets $500k
    {ZW::RICH, 100000},       // [54] Millionaire
    {ZW::NONE, 200000},       // [55] Second million
    {ZW::NONE, 500000},       // [56] Award 'Interior of the Year'
    {ZW::NONE, 100},          // [57] Award 'Biggest Dump of All Time'
    {ZW::NONE, 2000},         // [58] Best Kids' Pizza of the Year
    {ZW::NONE, 2000},         // [59] Best Young Persons' Pizza of the Year
    {ZW::NONE, 2000},         // [60] Best Workers' Pizza of the Year
    {ZW::NONE, 2000},    // [61] Best White-collar Workers' Pizza of the Year
    {ZW::NONE, 2000},    // [62] Best Business Pizza of the Year
    {ZW::NONE, 2000},    // [63] Best Senior Citizens' Pizza of the Year
    {ZW::PIZZA, 500000}, // [64] Best Pizza of the Year
    {ZW::RESTAURANT, 200000},      // [65] Restaurant of the Year
    {ZW::BALANCE, 1000},           // [66] Master Chef fully trained
    {ZW::BALANCE, 1000},           // [67] Waiter fully qualified
    {ZW::BALANCE, 1000},           // [68] Manager: Business Studies expert
    {ZW::NONE, 100000},            // [69] Loses over $1M in one week
    {ZW::NONE, 2000},              // [70] First contact with the Mafia
    {ZW::KIDNAPPED, 4000},         // [71] First Underworld mission
    {ZW::KIDNAPPED, 8000},         // [72] First raid for the Underworld
    {ZW::KIDNAPPED, 16000},        // [73] First money laundering
    {ZW::KIDNAPPED, 36000},        // [74] First arms deal
    {ZW::KIDNAPPED, 64000},        // [75] First shady share dealing
    {ZW::NONE, 10000},             // [76] First bribery
    {ZW::NONE, 200000},            // [77] Earns $1M through shady deals
    {ZW::KIDNAPPED, 2000},         // [78] First fine
    {ZW::EXECUTED, 30000},         // [79] Executed in electric chair (zw4)
    {ZW::PRISON, 20000},           // [80] On way to prison
    {ZW::FORCED_LABOR, 25000},     // [81] 2050 years hard labour
    {ZW::NONE, 100000},            // [82] Own place blows up
    {ZW::KIDNAPPED, 500},          // [83] Mafia beating
    {ZW::NONE, 2000},              // [84] Escaped assassination attempt
    {ZW::SWIM_WITH_FISHES, 50000}, // [85] Killed by mafia
    {ZW::NONE, 0},                 // [86] unused
    {ZW::PIZZA, 2000},             // [87] Won 1st prize in pizza competition
    {ZW::BEGGING, 100},            // [88] Bankrupt: couldn't repay loans
    {ZW::BEGGING, 100},            // [89] Bankrupt: couldn't pay rent
    {ZW::BEGGING, 100},            // [90] Bankrupt: couldn't repay debts
    {ZW::BEGGING, 100},            // [91] Bankrupt: couldn't pay suppliers
    {ZW::NONE, 100},               // [92] Bankrupt: general
    {ZW::NONE, 0},                 // [93] unused
    {ZW::KIDNAPPED, 30000}, // [94] First attack on competitor's restaurant
    {ZW::BALANCE, 15000},   // [95] 50% discount on supply contract
    {ZW::BALANCE, 12000},   // [96] 1000 kg ingredients in one day
    {ZW::KIDNAPPED, 4000},  // [97] $3000 profit through weapons dealing
    {ZW::NONE, 0},          // [98] unused
    {ZW::NONE, 0},          // [99] unused

    // CV-only events, no fullscreen updates for these
    {ZW::NONE, 2000},    // [100] Arrival in Paris
    {ZW::NONE, 1000},    // [101] Arrival in Berlin
    {ZW::NONE, 1500},    // [102] Arrival in Baltimore
    {ZW::NONE, 5000},    // [103] Arrival in Vienna
    {ZW::NONE, 500},     // [104] Arrival in Chicago
    {ZW::NONE, 8000},    // [105] Arrival in New York
    {ZW::NONE, 4000},    // [106] Arrival in Madrid
    {ZW::NONE, 10000},   // [107] Arrival in Rome
    {ZW::NONE, 6000},    // [108] Arrival in Stockholm
    {ZW::NONE, 7000},    // [109] Arrival in London
    {ZW::NONE, 2000000}, // [110] Dominates market in Paris
    {ZW::NONE, 1000000}, // [111] Dominates market in Berlin
    {ZW::NONE, 1500000}, // [112] Dominates market in Baltimore
    {ZW::NONE, 3000000}, // [113] Dominates market in Vienna
    {ZW::NONE, 500000},  // [114] Dominates market in Chicago
    {ZW::NONE, 5000000}, // [115] Dominates market in New York
    {ZW::NONE, 2000000}, // [116] Dominates market in Madrid
    {ZW::NONE, 7000000}, // [117] Dominates market in Rome
    {ZW::NONE, 4000000}, // [118] Dominates market in Stockholm
    {ZW::NONE, 4500000}, // [119] Dominates market in London
    {ZW::NONE, 36000},   // [120] 5000 kg ingredients in one day
    {ZW::NONE, 7000},    // [121] $25k week's profit in one restaurant
    {ZW::NONE, 20000},   // [122] $50k week's profit in one restaurant
    {ZW::NONE, 26000},   // [123] Won 10 pizza competitions
    {ZW::NONE, 1000000}, // [124] Millionaire 10 times over
    {ZW::NONE, 2000000}, // [125] Millionaire 20 times over
    {ZW::NONE, 3000},    // [126] Award 'Most Beautiful Restaurant'
    {ZW::NONE, 10000},   // [127] Award 'Pizza of the Year' (mayor)
    {ZW::NONE, 50000},   // [128] Award 'Best Investor'
    {ZW::NONE, 80000},   // [129] Award 'Best Imitator'
    {ZW::NONE, 20000},   // [130] Award 'Golden Bow-tie'
    {ZW::NONE, 50000},   // [131] Award 'Trans-Atlantic Trade Plaque'
    {ZW::NONE, 10000},   // [132] Central depot full
    {ZW::NONE, 8000},    // [133] 1000 customers/day in one restaurant
    {ZW::NONE, 20000},  // [134] 100% deliveries for 1 week across 5 restaurants
    {ZW::NONE, 150000}, // [135] Represented in all areas
    {ZW::NONE, 2000000}, // [136] Central warehouse in 5 cities
    {ZW::NONE, 5000000}, // [137] Central warehouse in 10 cities
    {ZW::NONE, 10},      // [138] Takes a swim in concrete trunks
    {ZW::NONE, 1000000}, // [139] $500k week's profits in one city
    {ZW::NONE, 0},       // [140] Controls the world
}};

// The original renders these four CV labels from strings in the executable
// instead of leben.e; there are lines for them but those talk about per week
// milestones instead of per day, so presumably some last minute change of mind
// in the original. The last string doesn't have a line in leben.e at all.
using CvLabelOverride = std::pair<MilestoneId, std::string_view>;
constexpr std::array cv_label_overrides = {
    CvLabelOverride{MilestoneId::Sold1KPizzasPerDay,
                    "1000 pizzas sold|in a single day"},
    CvLabelOverride{MilestoneId::Sold10KPizzasPerDay,
                    "10000 pizzas sold|in one week"},
    CvLabelOverride{MilestoneId::Has25KWeeksProfitInOneRestaurant,
                    "$25000 week's profit|in one restaurant"},
    CvLabelOverride{MilestoneId::Has50KWeeksProfitInOneRestaurant,
                    "$50000 week's profit|in one restaurant"},
    CvLabelOverride{MilestoneId::ControlsWorld, "CONTROLS WORLD!!!"},
};

CareerLog::CareerLog()
    : fullscreen_texts_loaded_(false), cv_labels_loaded_(false) {}

bool CareerLog::schedule_message(MilestoneId msg_id, core::PlayerId player_id,
                                 const GameDate& date) {
  if (was_scheduled(msg_id, player_id)) {
    return false;
  }

  auto& messages = player_messages_[player_id];
  messages.emplace_back(date, msg_id);
  return true;
}

bool CareerLog::was_scheduled(MilestoneId msg_id,
                              core::PlayerId player_id) const {
  auto it = player_messages_.find(player_id);
  if (it == player_messages_.end()) {
    return false;
  }

  const auto& messages = it->second;
  return std::ranges::any_of(messages, [msg_id](const ScheduledMessage& msg) {
    return msg.message_id == msg_id;
  });
}

bool CareerLog::was_shown(MilestoneId msg_id, core::PlayerId player_id) const {
  auto it = player_messages_.find(player_id);
  if (it == player_messages_.end()) {
    return false;
  }

  const auto& messages = it->second;
  return std::ranges::any_of(messages, [msg_id](const ScheduledMessage& msg) {
    return msg.message_id == msg_id && msg.shown;
  });
}

bool CareerLog::claim_for_delivery(MilestoneId msg_id, core::PlayerId player_id,
                                   const GameDate& date) {
  if (was_shown(msg_id, player_id)) {
    return false;
  }

  schedule_message(msg_id, player_id, date);
  mark_as_shown(msg_id, player_id);
  return true;
}

const EventData& CareerLog::get_data(MilestoneId msg_id) const {
  return milestone_data[static_cast<int>(msg_id)];
}

bool CareerLog::has_fullscreen(MilestoneId msg_id) const {
  return get_data(msg_id).background != ZwBackground::NONE;
}

std::vector<MilestoneId>
CareerLog::get_pending_for_player(core::PlayerId player_id) const {
  std::vector<MilestoneId> pending;

  auto it = player_messages_.find(player_id);
  if (it == player_messages_.end()) {
    return pending;
  }

  const auto& messages = it->second;
  for (const auto& msg : messages) {
    if (!msg.shown) {
      pending.push_back(msg.message_id);
    }
  }

  return pending;
}

void CareerLog::mark_as_shown(MilestoneId msg_id, core::PlayerId player_id) {
  auto it = player_messages_.find(player_id);
  if (it == player_messages_.end()) {
    return;
  }

  auto& messages = it->second;
  for (auto& msg : messages) {
    if (msg.message_id == msg_id) {
      msg.shown = true;
      break;
    }
  }
}

std::optional<std::string>
CareerLog::fullscreen_text(MilestoneId msg_id) const {
  if (!fullscreen_texts_loaded_) {
    load_fullscreen_texts();
  }

  const auto line = static_cast<size_t>(msg_id) - 1;
  if (line >= fullscreen_texts_.size() || fullscreen_texts_[line].empty()) {
    return std::nullopt;
  }
  return fullscreen_texts_[line];
}

void CareerLog::clear_player_messages(core::PlayerId player_id) {
  player_messages_.erase(player_id);
}

void CareerLog::restore_scheduled(core::PlayerId pid,
                                  const std::vector<CvEntry>& entries) {
  auto& messages = player_messages_[pid];
  for (const auto& entry : entries) {
    messages.emplace_back(entry.date, entry.message_id);
    messages.back().shown = entry.shown;
  }
}

std::vector<CareerLog::CvEntry>
CareerLog::get_cv_entries_for(core::PlayerId player_id) const {
  std::vector<CvEntry> entries;

  auto it = player_messages_.find(player_id);
  if (it == player_messages_.end()) {
    return entries;
  }

  for (const auto& msg : it->second) {
    entries.push_back({msg.date, msg.message_id, msg.shown});
  }

  return entries;
}

void CareerLog::load_fullscreen_texts() const {
  fullscreen_texts_ = fileutils::load_text_lines("t/levent.e");
  fullscreen_texts_loaded_ = true;

  for (size_t id = 1; id < milestone_data.size(); ++id) {
    if (milestone_data[id].background == ZwBackground::NONE) {
      continue;
    }
    if (id > fullscreen_texts_.size() || fullscreen_texts_[id - 1].empty()) {
      Logger::warn("[CareerLog] milestone {} shows a background but levent.e "
                   "has no text for it",
                   id);
    }
  }
}

std::string CareerLog::cv_label(MilestoneId msg_id) const {
  for (const auto& [id, label] : cv_label_overrides) {
    if (id == msg_id) {
      return std::string{label};
    }
  }

  if (!cv_labels_loaded_) {
    load_cv_labels();
  }

  const auto line = static_cast<size_t>(msg_id) - 1;
  if (line >= cv_labels_.size()) {
    return {};
  }
  return cv_labels_[line];
}

void CareerLog::load_cv_labels() const {
  cv_labels_ = fileutils::load_text_lines("t/leben.e");
  cv_labels_loaded_ = true;
}

} // namespace core::events
