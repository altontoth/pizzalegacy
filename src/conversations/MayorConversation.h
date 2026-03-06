/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "conversations/BaseConversation.h"
#include "conversations/MayorLine.h"
#include "core/city/MayorOffice.h"
#include "core/state/GameState.h"

enum class MayorConversationType {
  QualityInspection,
  FirstQualityInspection,
  MostBeautifulRestaurantAward,
  PizzaOfTheYearCompetition,
  BestInvestorInTownAward,
  BestImitatorAward,
  GoldenBowTieAward,
  TransAtlanticTradeAward,
  DonationAcknowledge,
  PizzaOfTheYearWinner,
  PhoneCall,
};

class MayorConversation : public BaseConversation<MayorLine> {
public:
  MayorConversation(GameState&, const Actor*, MayorOffice& mo,
                    MayorConversationType mt);
  Emotion emotion() const override;
  std::vector<ConversationLine<MayorLine>> lines() const override;
  bool advance() override;
  bool advance_line_choice(MayorLine) override;
  bool advance_proposal(bool) override;
  bool has_ended() const override;

private:
  static MayorLine compute_start_line(const GameState& state, const Actor*,
                                      MayorConversationType);

  MayorOffice& mayor_office_;
  // MayorConversationType type_;
};
