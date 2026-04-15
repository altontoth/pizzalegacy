/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <vector>

#include "conversations/BaseConversation.h"
#include "conversations/MafiaLine.h"
#include "core/character/MafiaPlayerState.h"
#include "core/common/Ids.h"
#include "core/state/GameState.h"

class MafiaConversation : public BaseConversation<MafiaLine> {
public:
  MafiaConversation(
      GameState&, const Actor*, MafiaMode,
      std::optional<core::RestaurantId> restaurant_id = std::nullopt,
      bool first_contact = false);
  Emotion emotion() const override;
  std::vector<ConversationLine<MafiaLine>> lines() const override;
  bool advance() override;
  bool advance_line_choice(MafiaLine) override;
  bool advance_proposal(bool) override;
  bool has_ended() const override;
  bool back_allowed() const override;

private:
  static MafiaLine compute_start_line(GameState& state, const Actor*,
                                      MafiaMode mm,
                                      std::optional<core::RestaurantId>,
                                      bool first_contact);
  std::optional<MafiaMission> generate_mission(int rank);

  void apply_npc_effects(GameState& state, MafiaMode mm);
  std::string protection_offer_text() const;

  std::optional<core::RestaurantId> restaurant_id_;
  int clan_danger_ = 0;
  int protection_price_ = 0;
  int loan_amount_ = 0;
  int loan_repayment_ = 0;
  int tip_price_ = 0;
  int mission_risk_ = 0;
  std::optional<MafiaMission> pending_mission_;
  std::optional<MafiaLine> tip_reveal_;
  std::string tip_opener_text_;
  std::string tip_result_text_;
  mutable bool game_over_pending_ = false;
  mutable bool star_message_pending_ = false;
};
