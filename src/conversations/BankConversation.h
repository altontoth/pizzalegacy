/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "conversations/BankLine.h"
#include "conversations/BaseConversation.h"
#include "core/finance/Loan.h"
#include "core/state/GameState.h"

class BankConversation : public BaseConversation<BankLine> {
public:
  BankConversation(GameState&, const Actor*, const BankSettings& bs);
  Emotion emotion() const override;
  std::vector<ConversationLine<BankLine>> lines() const override;
  bool advance() override;
  bool advance_line_choice(BankLine) override;
  bool advance_proposal(bool) override;
  bool has_ended() const override;
  PopupRequest popup_request() const override;
  void on_cheque_result(int amount, int account) override;

private:
  Loan propose_loan() const;
  int round_to_nearest(int value, int rounding_unit) const;
  static BankLine compute_start_line(const GameState& state, const Actor*);
  Loan proposed_loan_{};
  Loan* repaying_loan_{nullptr};
  const BankSettings& bank_settings_;
  bool wants_cheque_{false};
};
