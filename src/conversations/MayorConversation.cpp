/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "conversations/MayorConversation.h"

#include <magic_enum/magic_enum.hpp>

#include "conversations/MayorLine.h"
#include "core/character/Actor.h"
#include "core/character/Player.h"
#include "core/character/SocialStatus.h"
#include "engine/FormatCompat.h"

MayorConversation::MayorConversation(GameState& state, const Actor* actor,
                                     MayorOffice& mo, MayorConversationType mt,
                                     std::optional<InspectionOutcome> outcome,
                                     std::optional<int> award_winner_player_id,
                                     std::string restaurant_name)
    : BaseConversation(compute_start_line(state, actor, mt, mo), state,
                       "t/bg.e", actor),
      mayor_office_(mo), inspection_outcome_(outcome),
      award_winner_player_id_(award_winner_player_id),
      restaurant_name_(std::move(restaurant_name)) {

  // Pre-format appointment date string for the phone-call path.
  if (mt == MayorConversationType::PhoneCall) {
    int appt = mo.appointment_day();
    if (appt > 0) {
      auto month = state.time().current_date().month_name();
      appointment_date_str_ = compat::format("{} {}", month, appt);
    }
  }
}

MayorLine MayorConversation::outcome_result_line() const {
  if (!inspection_outcome_) {
    return MayorLine::QUALITY_NOT_CHANGED;
  }
  const auto& o = *inspection_outcome_;
  if (o.star_gained && o.is_first) {
    return MayorLine::NOT_BAD_FOR_A_START_FIRST_STAR;
  }
  if (o.star_gained) {
    return MayorLine::CONGRATULATIONS_NEW_STAR;
  }
  if (o.star_lost && o.is_first) {
    return MayorLine::RESTAURANT_CLOSED_FOR_A_WEEK;
  }
  if (o.star_lost) {
    return MayorLine::QUALITY_FALLEN_STAR_REMOVED;
  }
  if (o.is_first) {
    return MayorLine::YOU_NEED_TO_WORK_ON_YOUR_QUALITY_NO_STAR;
  }
  return MayorLine::QUALITY_NOT_CHANGED;
}

bool MayorConversation::active_player_won() const {
  if (!award_winner_player_id_.has_value() || *award_winner_player_id_ < 0) {
    return false;
  }
  auto* winner = state_.players()[*award_winner_player_id_];
  return winner != nullptr && winner == state_.active_player();
}

bool MayorConversation::advance() {
  switch (current_line_) {
  // Inspection flow: intro -> result -> sign-off -> thank-you
  case MayorLine::FOOD_QUALITY_CHECK:
    current_line(outcome_result_line());
    break;
  case MayorLine::NOT_BAD_FOR_A_START_FIRST_STAR:
  case MayorLine::CONGRATULATIONS_NEW_STAR:
  case MayorLine::RESTAURANT_CLOSED_FOR_A_WEEK:
  case MayorLine::QUALITY_FALLEN_STAR_REMOVED:
  case MayorLine::QUALITY_NOT_CHANGED:
  case MayorLine::YOU_NEED_TO_WORK_ON_YOUR_QUALITY_NO_STAR:
    if (inspection_outcome_) {
      auto* player = state_.active_player();
      if (player) {
        if (inspection_outcome_->star_gained) {
          state_.adjust_social_score(*player, 40);
        }
        else if (inspection_outcome_->star_lost) {
          state_.adjust_social_score(*player, -30);
        }
      }
    }
    current_line(MayorLine::ILL_LOOK_AGAIN_WHEN_I_CAN);
    break;

  // Ceremony "goes to" lines: advance to the award-specific name screen.
  case MayorLine::THE_AWARD_FOR_MOST_BEAUTIFUL_RESTAURANT_GOES_TO:
    current_line(MayorLine::AWARD_WINNER_NAME_EXCLAIM);
    break;
  case MayorLine::BEST_INVESTOR_IN_TOWN_WENT_TO:
    current_line(MayorLine::AWARD_WINNER_NAME);
    break;
  case MayorLine::GOLDEN_BOW_TIE_GOES_TO:
    current_line(MayorLine::AWARD_WINNER_NAME_EXCLAIM);
    break;
  case MayorLine::THE_TRANS_ATLANTIC_TRADE_AWARD_WENT_TO:
    current_line(MayorLine::AWARD_WINNER_NAME);
    break;

  // Winner name screens: route to the award-specific congratulation. Only
  // reached when the active player won (other-player paths are terminal).
  case MayorLine::AWARD_WINNER_NAME:
    switch (previous_line_) {
    case MayorLine::BEST_INVESTOR_IN_TOWN_WENT_TO:
      current_line(MayorLine::CONGRATS_THROUGH_WISE_INVESTMENTS_MADE_FINE_NAME);
      break;
    case MayorLine::THE_TRANS_ATLANTIC_TRADE_AWARD_WENT_TO:
      current_line(MayorLine::CONGRATULATIONS_TRANS_ATLANTIC_TRADE_AWARD);
      break;
    default:
      std::cerr << "MayorConversation::advance() unexpected previous_line_ at "
                   "AWARD_WINNER_NAME\n";
      return false;
    }
    break;
  case MayorLine::AWARD_WINNER_NAME_EXCLAIM:
    switch (previous_line_) {
    case MayorLine::THE_AWARD_FOR_MOST_BEAUTIFUL_RESTAURANT_GOES_TO:
      current_line(MayorLine::YOU_HAVE_PROVED_YOU_CAN_DECORATE_TASTEFULLY);
      break;
    case MayorLine::GOLDEN_BOW_TIE_GOES_TO:
      current_line(MayorLine::CONGRATS_YOU_CAN_PREPARE_PIZZA_VERY_FAST);
      break;
    default:
      std::cerr << "MayorConversation::advance() unexpected previous_line_ at "
                   "AWARD_WINNER_NAME_EXCLAIM\n";
      return false;
    }
    break;
  // AWARD_WINNER_NAME_CONGRATS is always terminal; advance() is never called.

  // Intermediate congratulation lines (player won): advance to final terminal.
  case MayorLine::YOU_HAVE_PROVED_YOU_CAN_DECORATE_TASTEFULLY:
    current_line(MayorLine::YOU_WON_MOST_BEAUTIFUL_RESTAURANT_IN_TOWN);
    break;
  case MayorLine::CONGRATS_THROUGH_WISE_INVESTMENTS_MADE_FINE_NAME:
    current_line(MayorLine::YOU_WON_BEST_INVESTOR_IN_TOWN);
    break;
  case MayorLine::CONGRATULATIONS_TRANS_ATLANTIC_TRADE_AWARD:
    current_line(MayorLine::YOU_WON_TRANS_ATLANTIC_TRADE_AWARD);
    break;

  // Quarterly award greeting: if ceremony show "goes to" line, else fee text
  case MayorLine::HELLO_MOST_BEAUTIFUL_RESTAURANT_IN_TOWN:
    if (award_winner_player_id_.has_value()) {
      current_line(MayorLine::THE_AWARD_FOR_MOST_BEAUTIFUL_RESTAURANT_GOES_TO);
    }
    else {
      current_line(MayorLine::IF_YOU_WISH_TO_TAKE_PART_IN_CONFERMENT);
    }
    break;
  case MayorLine::HELLO_BEST_INVESTOR_IN_TOWN:
    if (award_winner_player_id_.has_value()) {
      current_line(MayorLine::BEST_INVESTOR_IN_TOWN_WENT_TO);
    }
    else {
      current_line(MayorLine::IF_YOU_WISH_TO_TAKE_PART_IN_CONFERMENT);
    }
    break;
  case MayorLine::HELLO_GOLDEN_BOW_TIE:
    if (award_winner_player_id_.has_value()) {
      current_line(MayorLine::GOLDEN_BOW_TIE_GOES_TO);
    }
    else {
      current_line(MayorLine::IF_YOU_WISH_TO_TAKE_PART_IN_CONFERMENT);
    }
    break;
  case MayorLine::HELLO_TRANS_ATLANTIC_TRADE_AWARD:
    if (award_winner_player_id_.has_value()) {
      if (*award_winner_player_id_ >= 0) {
        current_line(MayorLine::THE_TRANS_ATLANTIC_TRADE_AWARD_WENT_TO);
      }
      else {
        current_line(MayorLine::NOBODY_SO_LITTLE_TRADE_WE_WERE_UNABLE);
      }
    }
    else {
      current_line(MayorLine::IF_YOU_WISH_TO_TAKE_PART_IN_CONFERMENT);
    }
    break;

  case MayorLine::WE_HAVE_A_WINNER_MASTER_CHEF_IS:
    current_line(MayorLine::POTY_WINNER);
    break;
  case MayorLine::POTY_WINNER:
    if (active_player_won()) {
      current_line(MayorLine::YOU_WON_PIZZA_OF_THE_YEAR);
    }
    return false;

  case MayorLine::HELLO_BEST_IMITATOR_OF_THE_YEAR:
    current_line(MayorLine::LAST_YEAR_MOST_PIZZA_COMPETITIONS_WON_BY);
    break;
  case MayorLine::LAST_YEAR_MOST_PIZZA_COMPETITIONS_WON_BY:
    if (award_winner_player_id_.has_value() && *award_winner_player_id_ >= 0) {
      current_line(MayorLine::AWARD_WINNER_NAME_CONGRATS);
    }
    else {
      current_line(MayorLine::NONE_OF_OUR_CHAINS_THIS_IS_A_DISGRACE);
    }
    break;

  case MayorLine::VERY_BUSY_MAN_HARDLY_TIME_FOR_PIZZA:
    current_line(MayorLine::IF_YOU_WISH_TO_MAKE_A_DONATION);
    break;

  default:
    std::cerr << "MayorConversation::advance() unhandled state "
              << magic_enum::enum_name(current_line_) << "\n";
    return false;
  }
  return true;
}

bool MayorConversation::advance_line_choice(MayorLine /*choice*/) {
  std::cerr << "MayorConversation::advance_line_choice() not implemented\n";
  return false;
}

bool MayorConversation::advance_proposal(bool /*agree*/) {
  // No yes/no proposals in the mayor conversation; fall back to click-through.
  return advance();
}

Emotion MayorConversation::emotion() const {
  if (current_line_ == MayorLine::POTY_WINNER) {
    return active_player_won() ? Emotion::HAPPY : Emotion::NEUTRAL;
  }
  switch (current_line_) {
  case MayorLine::CONGRATULATIONS_NEW_STAR:
  case MayorLine::NOT_BAD_FOR_A_START_FIRST_STAR:
  case MayorLine::YOU_WON_PIZZA_OF_THE_YEAR:
  case MayorLine::YOU_WON_MOST_BEAUTIFUL_RESTAURANT_IN_TOWN:
  case MayorLine::CONGRATS_THROUGH_WISE_INVESTMENTS_MADE_FINE_NAME:
  case MayorLine::YOU_WON_BEST_INVESTOR_IN_TOWN:
  case MayorLine::CONGRATS_YOU_CAN_PREPARE_PIZZA_VERY_FAST:
  case MayorLine::CONGRATULATIONS_TRANS_ATLANTIC_TRADE_AWARD:
  case MayorLine::YOU_WON_TRANS_ATLANTIC_TRADE_AWARD:
    return Emotion::HAPPY;

  case MayorLine::QUALITY_FALLEN_STAR_REMOVED:
  case MayorLine::RESTAURANT_CLOSED_FOR_A_WEEK:
  case MayorLine::NONE_OF_OUR_CHAINS_THIS_IS_A_DISGRACE:
  case MayorLine::NOBODY_SO_LITTLE_TRADE_WE_WERE_UNABLE:
  case MayorLine::WHO_ARE_YOU_GOODBYE:
    return Emotion::ANGRY;

  default:
    return Emotion::NEUTRAL;
  }
}

std::vector<ConversationLine<MayorLine>> MayorConversation::lines() const {
  switch (current_line_) {
  // Inspection intro: append restaurant name after file text
  case MayorLine::FOOD_QUALITY_CHECK:
    return {make_line(current_line_,
                      {.custom_text = " " + restaurant_name_ + "."})};
  case MayorLine::HELLO_PIZZA_OF_THE_YEAR:
    return {make_terminal_line(current_line_)};

  case MayorLine::NOT_BAD_FOR_A_START_FIRST_STAR:
  case MayorLine::CONGRATULATIONS_NEW_STAR:
  case MayorLine::RESTAURANT_CLOSED_FOR_A_WEEK:
  case MayorLine::QUALITY_FALLEN_STAR_REMOVED:
  case MayorLine::QUALITY_NOT_CHANGED:
  case MayorLine::YOU_NEED_TO_WORK_ON_YOUR_QUALITY_NO_STAR:
  case MayorLine::ILL_LOOK_AGAIN_WHEN_I_CAN:
  case MayorLine::VERY_BUSY_MAN_HARDLY_TIME_FOR_PIZZA:
  case MayorLine::HELLO_MOST_BEAUTIFUL_RESTAURANT_IN_TOWN:
  case MayorLine::WE_HAVE_A_WINNER_MASTER_CHEF_IS:
  case MayorLine::HELLO_BEST_INVESTOR_IN_TOWN:
  case MayorLine::HELLO_BEST_IMITATOR_OF_THE_YEAR:
  case MayorLine::HELLO_GOLDEN_BOW_TIE:
  case MayorLine::HELLO_TRANS_ATLANTIC_TRADE_AWARD:
  // Ceremony "goes to" lines: always non-terminal. Winner name follows as its
  // own screen (AWARD_WINNER_NAME).
  case MayorLine::THE_AWARD_FOR_MOST_BEAUTIFUL_RESTAURANT_GOES_TO:
  case MayorLine::BEST_INVESTOR_IN_TOWN_WENT_TO:
  case MayorLine::GOLDEN_BOW_TIE_GOES_TO:
  case MayorLine::THE_TRANS_ATLANTIC_TRADE_AWARD_WENT_TO:
  case MayorLine::LAST_YEAR_MOST_PIZZA_COMPETITIONS_WON_BY:
    return {make_line(current_line_)};

  // Winner name screens: each award type uses a distinct punctuation format.
  // Non-terminal only when the active player won and congratulation lines
  // follow; terminal when another player won.
  case MayorLine::AWARD_WINNER_NAME: // "<name>" (Best Investor, Trans-Atlantic)
  case MayorLine::AWARD_WINNER_NAME_EXCLAIM:    // "<name>!" (Most Beautiful,
                                                // Golden Bow Tie)
  case MayorLine::AWARD_WINNER_NAME_CONGRATS: { // "<name>!!! Congratulations!"
                                                // (Best Imitator)
    std::string name;
    if (award_winner_player_id_.has_value() && *award_winner_player_id_ >= 0) {
      auto* winner = state_.players()[*award_winner_player_id_];
      name = winner ? winner->name() : "";
    }
    if (current_line_ == MayorLine::AWARD_WINNER_NAME_EXCLAIM) {
      name += "!";
    }
    else if (current_line_ == MayorLine::AWARD_WINNER_NAME_CONGRATS) {
      name += "!!! Congratulations!";
    }
    bool has_congrats = current_line_ != MayorLine::AWARD_WINNER_NAME_CONGRATS;
    if (has_congrats && active_player_won()) {
      return {make_line(current_line_, {.custom_text = name})};
    }
    return {make_terminal_line(current_line_, name)};
  }

  // Intermediate congratulation lines shown when the active player won.
  case MayorLine::YOU_HAVE_PROVED_YOU_CAN_DECORATE_TASTEFULLY:
  case MayorLine::CONGRATS_THROUGH_WISE_INVESTMENTS_MADE_FINE_NAME:
  case MayorLine::CONGRATULATIONS_TRANS_ATLANTIC_TRADE_AWARD:
    return {make_line(current_line_)};

  case MayorLine::POTY_WINNER: {
    std::string name;
    if (award_winner_player_id_.has_value() && *award_winner_player_id_ >= 0) {
      auto* winner = state_.players()[*award_winner_player_id_];
      name = winner ? winner->name() : "";
    }
    if (active_player_won()) {
      return {make_line(current_line_, {.custom_text = name})};
    }
    return {make_terminal_line(current_line_, name)};
  }

  // Terminal lines (player clicks to dismiss)
  case MayorLine::THANK_YOU_FOR_YOUR_DONATION:
  case MayorLine::YOU_WON_PIZZA_OF_THE_YEAR:
  case MayorLine::YOU_WON_MOST_BEAUTIFUL_RESTAURANT_IN_TOWN:
  case MayorLine::YOU_WON_BEST_INVESTOR_IN_TOWN:
  case MayorLine::CONGRATS_YOU_CAN_PREPARE_PIZZA_VERY_FAST:
  case MayorLine::YOU_WON_TRANS_ATLANTIC_TRADE_AWARD:
  case MayorLine::NONE_OF_OUR_CHAINS_THIS_IS_A_DISGRACE:
  case MayorLine::NOBODY_SO_LITTLE_TRADE_WE_WERE_UNABLE:
  case MayorLine::IM_SORRY_VERY_BUSY_COME_BACK:
  case MayorLine::WHO_ARE_YOU_GOODBYE:
    return {make_terminal_line(current_line_)};

  // Terminal with appended bank account number
  case MayorLine::IF_YOU_WISH_TO_TAKE_PART_IN_CONFERMENT:
    return {make_terminal_line(
        current_line_,
        " " + std::to_string(mayor_office_.public_bank_account()))};
  case MayorLine::IF_YOU_WISH_TO_MAKE_A_DONATION:
    return {make_terminal_line(
        current_line_,
        " " + std::to_string(mayor_office_.private_bank_account()))};

  // Appointment date appended
  case MayorLine::I_DONT_HAVE_TIME_MAKE_APPOINTMENT:
    return {make_terminal_line(current_line_, " " + appointment_date_str_)};

  default:
    std::cerr << "MayorConversation::lines() not implemented for "
              << magic_enum::enum_name(current_line_) << "\n";
    return {};
  }
}

bool MayorConversation::has_ended() const {
  switch (current_line_) {
  case MayorLine::WHO_ARE_YOU_GOODBYE:
  case MayorLine::ILL_LOOK_AGAIN_WHEN_I_CAN:
    return true;
  default:
    return false;
  }
}

MayorLine MayorConversation::compute_start_line(const GameState& state,
                                                const Actor* /*actor*/,
                                                MayorConversationType type,
                                                const MayorOffice& mo) {
  switch (type) {
  case MayorConversationType::QualityInspection:
  case MayorConversationType::FirstQualityInspection:
    return MayorLine::FOOD_QUALITY_CHECK;
  case MayorConversationType::BestImitatorAward:
    return MayorLine::HELLO_BEST_IMITATOR_OF_THE_YEAR;
  case MayorConversationType::MostBeautifulRestaurantAward:
    return MayorLine::HELLO_MOST_BEAUTIFUL_RESTAURANT_IN_TOWN;
  case MayorConversationType::PizzaOfTheYearCompetition:
    return MayorLine::HELLO_PIZZA_OF_THE_YEAR;
  case MayorConversationType::PizzaOfTheYearWinner:
    return MayorLine::WE_HAVE_A_WINNER_MASTER_CHEF_IS;
  case MayorConversationType::BestInvestorInTownAward:
    return MayorLine::HELLO_BEST_INVESTOR_IN_TOWN;
  case MayorConversationType::GoldenBowTieAward:
    return MayorLine::HELLO_GOLDEN_BOW_TIE;
  case MayorConversationType::TransAtlanticTradeAward:
    return MayorLine::HELLO_TRANS_ATLANTIC_TRADE_AWARD;
  case MayorConversationType::DonationAcknowledge:
    return MayorLine::THANK_YOU_FOR_YOUR_DONATION;

  case MayorConversationType::PhoneCall: {
    // Reputation gate: rank < 2 gets the brush-off.
    auto* player = state.active_player();
    if (player) {
      int rank = social_rank(player->social_status().social_score,
                             state.difficulty_level());
      if (rank < 2) {
        return MayorLine::WHO_ARE_YOU_GOODBYE;
      }
    }

    // Appointment check: compare current day to scheduled appointment.
    int appt = mo.appointment_day();
    int today = static_cast<int>(state.time().current_date().day());
    if (appt == 0 || appt < today) {
      // No appointment set, or appointment day has already passed.
      return MayorLine::IM_SORRY_VERY_BUSY_COME_BACK;
    }
    if (appt > today) {
      // Upcoming appointment: show the date.
      return MayorLine::I_DONT_HAVE_TIME_MAKE_APPOINTMENT;
    }
    // Exact match: the meeting day.
    return MayorLine::VERY_BUSY_MAN_HARDLY_TIME_FOR_PIZZA;
  }
  }

  return MayorLine::VERY_BUSY_MAN_HARDLY_TIME_FOR_PIZZA;
}
