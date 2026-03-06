/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "conversations/PoliceConversation.h"
#include "core/character/Player.h"

PoliceConversation::PoliceConversation(GameState& state, const Actor* actor,
                                       std::optional<ArrestReason> ar)
    : BaseConversation(compute_start_line(state, actor), state, "t/polizei.e",
                       actor),
      arrest_reason_(ar) {}

/* This advances only the lines that don't require user choices */
bool PoliceConversation::advance() {
  switch (current_line_) {
  case PoliceLine::WELCOME:
    current_line_ = PoliceLine::OPTION_INFO_FOR_SALE;
    break;
  case PoliceLine::ARREST_RECEIVING:
  case PoliceLine::ARREST_BURGLARY:
  case PoliceLine::ARREST_WEAPONS_DEALING:
  case PoliceLine::ARREST_LAUNDERING_MONEY:
  case PoliceLine::ARREST_SHADY_DEALINGS:
  case PoliceLine::ARREST_HOMICIDE:
  case PoliceLine::ARREST_POSESSING_WEAPONS:
    current_line_ = PoliceLine::SENTENCE_JAIL_OR_FINE;
    break;
  default:
    std::cerr << "I don't know how to advance without choice from state "
              << current_line_ << "\n";
    return false;
    break;
  }
  return true;
}

bool PoliceConversation::advance_line_choice(PoliceLine choice) {
  // Do something with the choice
  switch (choice) {
  case PoliceLine::OPTION_INFO_FOR_SALE:
  case PoliceLine::OPTION_DO_YOU_SELL_INFO:
    current_line_ = PoliceLine::RESPONSE_FINE_INSULTING_OFFICIAL;
    state_.active_player()->financials().spend_cash(
        state_.time().current_date(), Account::FinesAndBribes, 2000_usd);
    break;
  case PoliceLine::OPTION_HOW_ARE_YOU:
  case PoliceLine::OPTION_NICE_DAY:
    current_line_ = PoliceLine::RESPONSE_NOT_GOOD_BUSY;
    break;
  case PoliceLine::OPTION_HOW_IS_PETTY_CASH:
  case PoliceLine::OPTION_NEED_MONEY:
    current_line_ = PoliceLine::RESPONSE_BRIBE_FAILED_2_DAYS_JAIL;
    break;
  case PoliceLine::OPTION_LOSE_THIS_WALLET:
  case PoliceLine::OPTION_DONATION:
    current_line_ = PoliceLine::RESPONSE2_YES_THANKS_FOR_CASH;
    break;
  default:
    std::cerr << "I don't know how to advance with choice " << choice
              << " from state " << current_line_ << "\n";
    return false;
    break;
  }
  return true;
}

bool PoliceConversation::advance_proposal(bool agree) {
  switch (current_line_) {
  case PoliceLine::SENTENCE_JAIL_OR_FINE:
    if (agree && state_.active_player()->cash_balance() >= 2000_usd) {
      auto success = state_.active_player()->spend_cash(
          state_.time().current_date(), Account::FinesAndBribes, 2000_usd);
    }
    else {
      state_.active_player()->jail_time(2);
    }
    break;
  default:
    // If there is no choice then just advance.
    return advance();
    break;
  }
  return true;
}

Emotion PoliceConversation::emotion() const {
  switch (current_line_) {
  case PoliceLine::RESPONSE_FINE_INSULTING_OFFICIAL:
    return Emotion::ANGRY;
  default:
    return Emotion::NEUTRAL;
  }
}

std::vector<ConversationLine<PoliceLine>> PoliceConversation::lines() const {
  switch (current_line_) {
  case PoliceLine::WELCOME:
    return {
        make_line(PoliceLine::WELCOME),
    };
  case PoliceLine::OPTION_INFO_FOR_SALE:
    return {
        make_option(engine::random::coin_flip()
                        ? PoliceLine::OPTION_INFO_FOR_SALE
                        : PoliceLine::OPTION_DO_YOU_SELL_INFO),
        make_option(engine::random::coin_flip() ? PoliceLine::OPTION_HOW_ARE_YOU
                                                : PoliceLine::OPTION_NICE_DAY),
        make_option(engine::random::coin_flip()
                        ? PoliceLine::OPTION_HOW_IS_PETTY_CASH
                        : PoliceLine::OPTION_NEED_MONEY),
        make_option(engine::random::coin_flip()
                        ? PoliceLine::OPTION_LOSE_THIS_WALLET
                        : PoliceLine::OPTION_DONATION),
    };
  case PoliceLine::RESPONSE_FINE_INSULTING_OFFICIAL:
    return {
        make_line(PoliceLine::RESPONSE_FINE_INSULTING_OFFICIAL),
    };
    break;
  case PoliceLine::ARREST_RECEIVING:
    return {make_line(PoliceLine::ARREST_RECEIVING)};
    break;
  case PoliceLine::ARREST_BURGLARY:
    return {make_line(PoliceLine::ARREST_BURGLARY)};
    break;
  case PoliceLine::ARREST_WEAPONS_DEALING:
    return {make_line(PoliceLine::ARREST_WEAPONS_DEALING)};
    break;
  case PoliceLine::ARREST_LAUNDERING_MONEY:
    return {make_line(PoliceLine::ARREST_LAUNDERING_MONEY)};
    break;
  case PoliceLine::ARREST_SHADY_DEALINGS:
    return {make_line(PoliceLine::ARREST_SHADY_DEALINGS)};
    break;
  case PoliceLine::ARREST_HOMICIDE:
    return {make_line(PoliceLine::ARREST_HOMICIDE)};
    break;
  case PoliceLine::ARREST_POSESSING_WEAPONS:
    return {make_line(PoliceLine::ARREST_POSESSING_WEAPONS)};
    break;
  case PoliceLine::SENTENCE_JAIL_OR_FINE:
    return {make_proposal(PoliceLine::SENTENCE_JAIL_OR_FINE,
                          "Do you want 2 days jail or a fine of $1000?")};
    break;
  default:
    std::cerr << "Get Lines Not yet implemented for line "
              << lines_.at(static_cast<int>(current_line_)) << std::endl;
    break;
  }
  return {};
}

bool PoliceConversation::has_ended() const {
  switch (current_line_) {
  case PoliceLine::RESPONSE_FINE_INSULTING_OFFICIAL:
  case PoliceLine::RESPONSE_NOT_GOOD_BUSY:
  case PoliceLine::RESPONSE_NOT_NICE_STOP_BOTHERING:
  case PoliceLine::RESPONSE2_YES_THANKS_FOR_CASH:
    return true;
  default:
    return false;
  }
}

PoliceLine PoliceConversation::compute_start_line(const GameState& /* state */,
                                                  const Actor* /*unused*/) {
  return PoliceLine::WELCOME;
}
