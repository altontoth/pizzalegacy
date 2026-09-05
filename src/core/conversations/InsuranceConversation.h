/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <vector>

#include "core/character/Actor.h"
#include "core/character/InsurancePlayerState.h"
#include "core/conversations/BaseConversation.h"
#include "core/conversations/InsuranceLine.h"
#include "core/finance/Money.h"
#include "core/state/GameState.h"

class InsuranceConversation : public BaseConversation<InsuranceLine> {
public:
  InsuranceConversation(GameState&, const Actor*, const InsuranceSettings& is,
                        core::ActorId actor_id);
  Emotion emotion() const override;
  std::vector<ConversationLine<InsuranceLine>> lines() const override;
  bool advance() override;
  bool advance_line_choice(InsuranceLine) override;
  bool advance_proposal(bool) override;
  bool has_ended() const override;

private:
  static InsuranceLine compute_start_line(const GameState& state, const Actor*);
  int risk_score() const;
  bool execute_payout(bool may_increase_premium);
  InsuranceLine type_specific_excuse() const;
  InsuranceLine type_specific_counter_arg() const;
  InsuranceLine type_specific_rebuttal() const;

  const InsuranceSettings& insurance_settings_;
  core::ActorId actor_id_;
  InsuranceType selected_type_{InsuranceType::THEFT};
  Money pending_premium_{0};
  Money pending_max_coverage_{0};
};
