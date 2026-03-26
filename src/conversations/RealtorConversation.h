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
#include "conversations/RealtorLine.h"
#include "core/finance/Loan.h"
#include "core/state/GameState.h"

class Property;

class RealtorConversation : public BaseConversation<RealtorLine> {
public:
  RealtorConversation(GameState&, const Actor*, const RealtorSettings& rs);
  virtual Emotion emotion() const override;
  virtual std::vector<ConversationLine<RealtorLine>> lines() const override;
  virtual bool advance() override;
  virtual bool advance_line_choice(RealtorLine) override;
  virtual bool advance_proposal(bool) override;
  virtual bool has_ended() const override;

private:
  int round_to_nearest(int value, int rounding_unit) const;
  static RealtorLine compute_start_line(const GameState& state, const Actor*);
  Property* active_property() const;
  Money list_price_{0};
  Money price_{0};
  bool ripoff_{false};
  bool buyer_haggled_already{false};
  Money sell_offer_{0};
  int sell_haggle_count_{0};
  const RealtorSettings& realtor_settings_;
};
