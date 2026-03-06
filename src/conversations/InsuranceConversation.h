/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "conversations/BaseConversation.h"
#include "conversations/InsuranceLine.h"
#include "core/finance/Loan.h"
#include "core/state/GameState.h"

class InsuranceConversation : public BaseConversation<InsuranceLine> {
public:
  InsuranceConversation(GameState&, const Actor*, const InsuranceSettings& bs);
  Emotion emotion() const override;
  std::vector<ConversationLine<InsuranceLine>> lines() const override;
  bool advance() override;
  bool advance_line_choice(InsuranceLine) override;
  bool advance_proposal(bool) override;
  bool has_ended() const override;

private:
  Loan propose_loan() const;
  int round_to_nearest(int value, int rounding_unit) const;
  static InsuranceLine compute_start_line(const GameState& state, const Actor*);
  Loan proposed_loan_{};
  Loan* repaying_loan_{nullptr};
  const InsuranceSettings& bank_settings_;
};
