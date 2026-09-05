/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "core/conversations/BaseConversation.h"
#include "core/conversations/RealtorLine.h"
#include "core/finance/Loan.h"
#include "core/state/GameState.h"

class Property;

class RealtorConversation : public BaseConversation<RealtorLine> {
public:
  RealtorConversation(GameState&, const Actor*, const RealtorSettings& rs);
  Emotion emotion() const override;
  std::vector<ConversationLine<RealtorLine>> lines() const override;
  bool advance() override;
  bool advance_line_choice(RealtorLine) override;
  bool advance_proposal(bool) override;
  bool has_ended() const override;

private:
  static RealtorLine compute_start_line(const GameState& state, const Actor*);
  Property* active_property() const;
  // Hand the premise over at whatever sell_offer_ has reached.
  void complete_sale();
  // One round of haggling over the realtor's offer, options 1 to 4.
  void haggle_sale(int option);
  // Rank 0 is turned away at the door, before any check on the property.
  bool welcome_here() const;
  Money price_{0};
  bool buyer_haggled_already{false};
  Money sell_offer_{0};
  int sell_haggle_count_{0};
  bool ripoff_;
};
