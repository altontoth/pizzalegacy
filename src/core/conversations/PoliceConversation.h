/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "core/conversations/BaseConversation.h"
#include "core/conversations/PoliceLine.h"
#include "core/finance/Money.h"
#include "core/state/GameState.h"

class Restaurant;

class PoliceConversation : public BaseConversation<PoliceLine> {
public:
  PoliceConversation(GameState&, const Actor*,
                     std::optional<ArrestReason> ar = std::nullopt);
  Emotion emotion() const override;
  std::vector<ConversationLine<PoliceLine>> lines() const override;
  bool advance() override;
  bool advance_line_choice(PoliceLine choice) override;
  bool advance_proposal(bool agree) override;
  bool has_ended() const override;
  bool back_allowed() const override { return !arrest_reason_.has_value(); }

  struct MayorTip {
    std::string restaurant;
    int countdown_days;
  };

  // The tip the police will give about the mayor's next visit, or nothing
  // when there is no visit close enough.
  static std::optional<MayorTip> mayor_tip(const GameState& state,
                                           core::PlayerId player);

  static std::optional<std::string>
  investigated_district(const GameState& state);

  static bool warehouse_tip_available(const GameState& state,
                                      core::PlayerId player);

  static void seize_carried_weapons(GameState& state, Player& player);

private:
  static PoliceLine compute_start_line(const GameState& state, const Actor*,
                                       std::optional<ArrestReason> reason);
  void apply_confiscation();
  void go_to_jail(int days);

  std::optional<ArrestReason> arrest_reason_;
  int sentence_days_ = 0;
  Money bribe_or_fine_cost_;
  Money donation_amount_;
  std::array<bool, 4> random_flags_{};
  std::optional<MayorTip> mayor_tip_;
};
