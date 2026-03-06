/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <vector>

#include "conversations/ArmsDealerLine.h"
#include "conversations/BaseConversation.h"
#include "core/state/GameState.h"

class ArmsDealerConversation : public BaseConversation<ArmsDealerLine> {
public:
  ArmsDealerConversation(GameState&, const Actor*);
  virtual Emotion emotion() const override;
  virtual std::vector<ConversationLine<ArmsDealerLine>> lines() const override;
  virtual bool advance() override;
  virtual bool advance_line_choice(ArmsDealerLine) override;
  virtual bool advance_proposal(bool) override;
  virtual bool has_ended() const override;

private:
  enum class PurchaseResult {
    SUCCESS,
    NOT_ENOUGH_MONEY,
    ALREADY_AT_MAX,
    NOW_AT_MAX,
  };
  enum class SaleResult {
    SUCCESS,
    NOT_ENOUGH_STOCK,
  };
  static ArmsDealerLine compute_start_line(const GameState& state,
                                           const Actor*);
  constexpr core::item::Item to_arms_dealer_item(ArmsDealerLine line) const;
  constexpr int piece_count(ArmsDealerLine l);
  PurchaseResult make_purchase(int count, core::item::Item, Money price_per);
  SaleResult make_sale(int requested, core::item::Item id, Money price_per);
  WeaponMarket& market_;
  core::item::Item selected_item_;
  Money price_{0};
};
