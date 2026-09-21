/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/state/GameRules.h"

#include <string>
#include <string_view>
#include <vector>
#include "core/food/PizzaJudging.h"
#include "engine/FormatCompat.h"

namespace {
std::string on_off(bool value) { return value ? "on" : "off"; }
} // namespace

std::vector<std::string> rule_differences(const GameRules& theirs,
                                          const GameRules& ours) {
  std::vector<std::string> lines;
  auto add_difference = [&](std::string_view label,
                            const std::string& their_value,
                            const std::string& our_value) {
    if (their_value != our_value) {
      lines.push_back(compat::format("{}: {}", label, their_value));
    }
  };
  add_difference(rule_labels::fix_legacy_bugs, on_off(theirs.fix_legacy_bugs),
                 on_off(ours.fix_legacy_bugs));
  add_difference(rule_labels::qol_improvements, on_off(theirs.qol_improvements),
                 on_off(ours.qol_improvements));
  add_difference(rule_labels::mafia_shakedowns_target_all_restaurants,
                 on_off(theirs.mafia_shakedowns_target_all_restaurants),
                 on_off(ours.mafia_shakedowns_target_all_restaurants));
  add_difference(rule_labels::demand_fulfillment_popularity,
                 on_off(theirs.demand_fulfillment_popularity),
                 on_off(ours.demand_fulfillment_popularity));
  add_difference(rule_labels::win_condition, to_string(theirs.win_condition),
                 to_string(ours.win_condition));
  add_difference(rule_labels::pizza_judging, to_string(theirs.pizza_judging),
                 to_string(ours.pizza_judging));
  add_difference(rule_labels::police, on_off(theirs.police),
                 on_off(ours.police));
  return lines;
}
