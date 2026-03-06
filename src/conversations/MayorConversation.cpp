/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "conversations/MayorConversation.h"

#include <cmath>
#include <magic_enum/magic_enum.hpp>

#include "conversations/MayorLine.h"
#include "core/character/Actor.h"
#include "core/character/Player.h"

MayorConversation::MayorConversation(GameState& state, const Actor* actor,
                                     MayorOffice& mo, MayorConversationType mt)
    : BaseConversation(compute_start_line(state, actor, mt), state, "t/bg.e",
                       actor),
      mayor_office_(mo) {}

/* This advances only the lines that don't require user choices */
bool MayorConversation::advance() {
  switch (current_line_) {
  case MayorLine::VERY_BUSY_MAN_HARDLY_TIME_FOR_PIZZA:
    current_line(MayorLine::IF_YOU_WISH_TO_MAKE_A_DONATION);
    break;
  default:
    std::cerr << "I don't know how to advance without choice from state "
              << magic_enum::enum_name(current_line_) << "\n";
    return false;
    break;
  }
  return true;
}

bool MayorConversation::advance_line_choice(MayorLine choice) {
  switch (choice) {
  default:
    std::cerr << "I don't know how to advance with choice " << choice
              << " from state " << current_line_ << "\n";
    return false;
    break;
  }
  return true;
}

bool MayorConversation::advance_proposal(bool agree) {
  switch (current_line_) {
  default:
    // If there is no choice then just advance.
    return advance();
    break;
  }
  return true;
}

Emotion MayorConversation::emotion() const {
  switch (current_line_) {
  default:
    return Emotion::NEUTRAL;
  }
}

std::vector<ConversationLine<MayorLine>> MayorConversation::lines() const {
  switch (current_line_) {
  case MayorLine::VERY_BUSY_MAN_HARDLY_TIME_FOR_PIZZA:
    return {
        make_line(MayorLine::VERY_BUSY_MAN_HARDLY_TIME_FOR_PIZZA),
    };
  case MayorLine::IF_YOU_WISH_TO_MAKE_A_DONATION:
    return {
        make_terminal_line(
            current_line_,
            " " + std::to_string(mayor_office_.private_bank_account())),
    };
  default:
    std::cerr << "Get Lines Not yet implemented for line "
              << lines_.at(static_cast<int>(current_line_)) << "\n";
    break;
  }
  return {};
}

bool MayorConversation::has_ended() const { return false; }

MayorLine MayorConversation::compute_start_line(const GameState& /*state*/,
                                                const Actor* /* actor */,
                                                MayorConversationType type) {
  switch (type) {
  case MayorConversationType::QualityInspection:
    [[fallthrough]];
  case MayorConversationType::FirstQualityInspection:
    return MayorLine::FOOD_QUALITY_CHECK;
  case MayorConversationType::BestImitatorAward:
    return MayorLine::HELLO_BEST_IMITATOR_OF_THE_YEAR;
  case MayorConversationType::MostBeautifulRestaurantAward:
    return MayorLine::HELLO_MOST_BEAUTIFUL_RESTAURANT_IN_TOWN;
  case MayorConversationType::PizzaOfTheYearCompetition:
    return MayorLine::HELLO_PIZZA_OF_THE_YEAR;
  case MayorConversationType::BestInvestorInTownAward:
    return MayorLine::HELLO_BEST_INVESTOR_IN_TOWN;
  case MayorConversationType::GoldenBowTieAward:
    return MayorLine::HELLO_GOLDEN_BOW_TIE;
  case MayorConversationType::TransAtlanticTradeAward:
    return MayorLine::HELLO_TRANS_ATLANTIC_TRADE_AWARD;
  case MayorConversationType::DonationAcknowledge:
    return MayorLine::THANK_YOU_FOR_YOUR_DONATION;
  case MayorConversationType::PizzaOfTheYearWinner:
    return MayorLine::HELLO_PIZZA_OF_THE_YEAR;
  case MayorConversationType::PhoneCall:
    return MayorLine::VERY_BUSY_MAN_HARDLY_TIME_FOR_PIZZA;
  }
}
