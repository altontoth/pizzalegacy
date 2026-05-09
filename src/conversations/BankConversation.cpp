/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "conversations/BankConversation.h"

#include <cmath>
#include <magic_enum/magic_enum.hpp>

#include "conversations/BankLine.h"
#include "core/character/Actor.h"
#include "core/character/Player.h"
#include "engine/Math.h"

constexpr int MAX_NUMBER_OF_LOANS = 6;

BankConversation::BankConversation(GameState& state, const Actor* actor,
                                   const BankSettings& bank_settings)
    : BaseConversation(compute_start_line(state, actor), state, "t/bank.e",
                       actor),
      bank_settings_(bank_settings) {}

/* This advances only the lines that don't require user choices */
bool BankConversation::advance() {
  switch (current_line_) {
  case BankLine::ACCOUNT_OVERDRAWN:
  case BankLine::HELLO_MAY_I_HELP_YOU:
  case BankLine::SORRY_ALREADY_HAVE_LOAN:
  case BankLine::GOOD_LUCK_WITH_INVESTMENTS:
  case BankLine::YOU_DONT_HAVE_ENOUGH_TO_REPAY:
  case BankLine::YOU_DONT_HAVE_LOAN_WITH_US:
  case BankLine::TRANSFER_SUCCEEDED:
  case BankLine::TRANSFER_FAILED:
  case BankLine::SORRY_WE_DONT_HAVE_INVESTMENT_FOR_YOU:
    current_line(BankLine::I_NEED_LOAN);
    break;
  default:
    std::cerr << "I don't know how to advance without choice from state "
              << magic_enum::enum_name(current_line_) << "\n";
    return false;
    break;
  }
  return true;
}

bool BankConversation::advance_line_choice(BankLine choice) {
  Logger::info("Advance with choice {}",
               std::string(magic_enum::enum_name(choice)));
  // Do something with the choice
  switch (choice) {
  case BankLine::I_NEED_LOAN:
    if (state_.active_player()->number_of_loans() > MAX_NUMBER_OF_LOANS) {
      current_line(BankLine::TOO_MANY_LOANS);
    }
    else if (state_.active_player()->has_loan_with_bank(
                 state_.active_city_id(), core::BankId{actor_->id()})) {
      current_line(BankLine::SORRY_ALREADY_HAVE_LOAN);
    }
    else {
      proposed_loan_ = propose_loan();
      current_line(BankLine::LOAN_ARRANGEMENT);
    }
    break;
  case BankLine::I_WANT_TO_TRANSFER:
    wants_cheque_ = true;
    break;
  case BankLine::I_WANT_TO_REPAY:
    if (!state_.active_player()->has_loan_with_bank(
            state_.active_city_id(), core::BankId{actor_->id()})) {
      current_line(BankLine::YOU_DONT_HAVE_LOAN_WITH_US);
    }
    else {
      // Find the loan
      auto& loans =
          state_.active_player()->loans_in_city(state_.active_city_id());
      auto it = std::ranges::find_if(loans, [this](const Loan& l) {
        return l.bank_id() == core::BankId{actor_->id()};
      });
      repaying_loan_ = &(*it);

      // Check if player has enough money
      if (state_.active_player()->cash_balance() <
          Money{repaying_loan_->repay_amount()}) {
        current_line(BankLine::YOU_DONT_HAVE_ENOUGH_TO_REPAY);
      }
      else {
        current_line(BankLine::ARE_YOU_SURE_YOU_WANT_TO_REPAY);
      }
    }
    break;
  case BankLine::I_WANT_TO_INVEST: {
    auto* player = state_.active_player();
    if (player->cash_balance() < 20'000_usd ||
        player->number_of_investments() >= Player::max_investment_count) {
      current_line(BankLine::SORRY_WE_DONT_HAVE_INVESTMENT_FOR_YOU);
    }
    else {
      proposed_investment_ = propose_investment();
      current_line(BankLine::LATEST_INVESTMENT_TIP);
    }
    break;
  }
  default:
    std::cerr << "I don't know how to advance with choice " << choice
              << " from state " << current_line_ << "\n";
    return false;
    break;
  }
  return true;
}

bool BankConversation::advance_proposal(bool agree) {
  switch (current_line_) {
  case BankLine::LOAN_ARRANGEMENT:
    if (agree) {
      state_.active_player()->add_loan(state_.active_city_id(), proposed_loan_,
                                       state_.time().current_date());
      current_line(BankLine::GOOD_LUCK_WITH_INVESTMENTS);
    }
    else {
      current_line(BankLine::TOO_BAD_MAYBE_ASK_ANOTHER_BANK_FOR_CREDIT);
    }
    break;
  case BankLine::ARE_YOU_SURE_YOU_WANT_TO_REPAY:
    if (agree) {
      Money amount{repaying_loan_->repay_amount()};
      // Money already checked in advance_line_choice, so this won't fail
      (void)state_.active_player()->spend_cash(state_.time().current_date(),
                                               Account::Loan, amount);
      repaying_loan_->repay();
    }
    current_line(BankLine::I_NEED_LOAN);
    break;
  case BankLine::LATEST_INVESTMENT_TIP:
    if (agree) {
      (void)state_.active_player()->spend_cash(state_.time().current_date(),
                                               Account::Investments,
                                               proposed_investment_.amount());
      state_.active_player()->add_investment(proposed_investment_);
      current_line(BankLine::YOUR_MONEY_IS_SAFE_WITH_US);
    }
    else {
      current_line(BankLine::I_NEED_LOAN);
    }
    break;
  default:
    // If there is no choice then just advance.
    return advance();
    break;
  }
  return true;
}

Emotion BankConversation::emotion() const {
  switch (current_line_) {
  default:
    return Emotion::NEUTRAL;
  }
}

std::vector<ConversationLine<BankLine>> BankConversation::lines() const {
  switch (current_line_) {
  case BankLine::ACCOUNT_OVERDRAWN:
    return {
        make_line(BankLine::ACCOUNT_OVERDRAWN),
    };
    break;
  case BankLine::I_NEED_LOAN:
    return {
        make_option(BankLine::I_NEED_LOAN),
        make_option(BankLine::I_WANT_TO_INVEST),
        make_option(BankLine::I_WANT_TO_TRANSFER),
        make_option(BankLine::I_WANT_TO_REPAY),
    };
    break;
  case BankLine::LOAN_ARRANGEMENT: {
    std::ostringstream oss;
    oss << "   $" << proposed_loan_.start_amount().whole() << ", Interest:  ";
    oss << static_cast<int>(proposed_loan_.interest_percentage())
        << "%, Period: ";
    oss << proposed_loan_.start_months() << " months, payment per ";
    oss << "month: $" << proposed_loan_.monthly_repayment().whole() << ".";

    return {
        make_proposal(BankLine::LOAN_ARRANGEMENT, oss.str()),
    };
    break;
  }
  case BankLine::TOO_RISKY_FOR_LOAN:
    return {
        make_line(BankLine::TOO_RISKY_FOR_LOAN),
    };
    break;
  case BankLine::LATEST_INVESTMENT_TIP: {
    return {
        make_proposal(BankLine::LATEST_INVESTMENT_TIP,
                      compat::format(" {}, Period: {} months, interest: {}%",
                                     proposed_investment_.amount(),
                                     proposed_investment_.start_months(),
                                     proposed_investment_.return_rate())),
    };
    break;
  }
  case BankLine::OF_50:
    return {
        make_line(BankLine::OF_50),
    };
    break;
  case BankLine::OF_500:
    return {
        make_line(BankLine::OF_500),
    };
    break;
  case BankLine::OF_5000:
    return {
        make_line(BankLine::OF_5000),
    };
    break;
  case BankLine::AT_10PCT_INTEREST:
    return {
        make_line(BankLine::AT_10PCT_INTEREST),
    };
    break;
  case BankLine::AT_15PCT_INTEREST:
    return {
        make_line(BankLine::AT_15PCT_INTEREST),
    };
    break;
  case BankLine::AT_20PCT_INTEREST:
    return {
        make_line(BankLine::AT_20PCT_INTEREST),
    };
    break;
  case BankLine::LETS_SELL_FIXED_COST:
    return {
        make_line(BankLine::LETS_SELL_FIXED_COST),
    };
    break;
  case BankLine::YOU_DONT_NEED_A_LOAN:
    return {
        make_line(BankLine::YOU_DONT_NEED_A_LOAN),
    };
    break;
  case BankLine::MAXIMUM_100:
    return {
        make_line(BankLine::MAXIMUM_100),
    };
    break;
  case BankLine::MAXIMUM_500:
    return {
        make_line(BankLine::MAXIMUM_500),
    };
    break;
  case BankLine::MAXIMUM_1000:
    return {
        make_line(BankLine::MAXIMUM_1000),
    };
    break;
  case BankLine::MAXIMUM_2000:
    return {
        make_line(BankLine::MAXIMUM_2000),
    };
    break;
  case BankLine::OK_SO_THATS:
    return {
        make_line(BankLine::OK_SO_THATS),
    };
    break;
  case BankLine::ARE_YOU_SURE_YOU_WANT_TO_REPAY: {
    // "OK. So that's $12000. Are you sure, you want to repay this loan?"
    std::ostringstream oss;
    oss << Money{repaying_loan_->repay_amount()}.whole() << ". ";
    return {
        make_concat_proposal(BankLine::OK_SO_THATS, oss.str(),
                             BankLine::ARE_YOU_SURE_YOU_WANT_TO_REPAY),
    };
  }
  case BankLine::YOU_DONT_HAVE_ENOUGH_TO_REPAY:
    return {
        make_line(BankLine::YOU_DONT_HAVE_ENOUGH_TO_REPAY),
    };
    break;
  case BankLine::YOU_DONT_HAVE_LOAN_WITH_US:
    return {
        make_line(BankLine::YOU_DONT_HAVE_LOAN_WITH_US),
    };
    break;
  case BankLine::WE_HAVE_SOLD_YOUR_LOANS:
    return {
        make_line(BankLine::WE_HAVE_SOLD_YOUR_LOANS),
    };
    break;
  case BankLine::TOTAL_COMES_TO:
    return {
        make_line(BankLine::TOTAL_COMES_TO),
    };
    break;
  case BankLine::BUSINESS_MEN_ARE_CLIENTS_OFFER_DEAL:
    return {
        make_line(BankLine::BUSINESS_MEN_ARE_CLIENTS_OFFER_DEAL),
    };
    break;
  case BankLine::ADVERTISE_FOR_US_FOR_10K:
    return {
        make_line(BankLine::ADVERTISE_FOR_US_FOR_10K),
    };
    break;
  case BankLine::TOO_MANY_LOANS:
    return {
        make_line(BankLine::TOO_MANY_LOANS),
    };
    break;
  case BankLine::GOOD_LUCK_WITH_INVESTMENTS:
    return {
        make_line(BankLine::GOOD_LUCK_WITH_INVESTMENTS),
    };
    break;
  case BankLine::TOO_BAD_MAYBE_ASK_ANOTHER_BANK_FOR_CREDIT:
    return {
        make_line(BankLine::TOO_BAD_MAYBE_ASK_ANOTHER_BANK_FOR_CREDIT),
    };
    break;
  case BankLine::SORRY_WE_DONT_HAVE_INVESTMENT_FOR_YOU:
    return {
        make_line(BankLine::SORRY_WE_DONT_HAVE_INVESTMENT_FOR_YOU),
    };
    break;
  case BankLine::YOUR_MONEY_IS_SAFE_WITH_US:
    return {
        make_line(BankLine::YOUR_MONEY_IS_SAFE_WITH_US),
    };
    break;
  case BankLine::HELLO_MAY_I_HELP_YOU:
    return {
        make_text_line(BankLine::HELLO_MAY_I_HELP_YOU, actor_->name() + ". "),
    };
    break;
  case BankLine::SORRY_ALREADY_HAVE_LOAN:
    return {
        make_line(BankLine::SORRY_ALREADY_HAVE_LOAN),
    };
    break;
  case BankLine::TRANSFER_SUCCEEDED:
    return {
        make_line(BankLine::TRANSFER_SUCCEEDED),
    };
    break;
  case BankLine::TRANSFER_FAILED:
    return {
        make_line(BankLine::TRANSFER_FAILED),
    };
    break;
  default:
    std::cerr << "Get Lines Not yet implemented for line "
              << lines_.at(static_cast<int>(current_line_)) << "\n";
    break;
  }
  return {};
}

bool BankConversation::has_ended() const {
  switch (current_line_) {
  case BankLine::YOUR_MONEY_IS_SAFE_WITH_US:
    return true;
  default:
    return false;
  }
}

Loan BankConversation::propose_loan() const {
  auto player = state_.active_player()->character();

  // Calculate base credit score (management + personality + experience + age) *
  // 100
  int base_credit_score = player.management();
  base_credit_score += player.personality();
  base_credit_score += player.experience();
  base_credit_score += player.age();
  base_credit_score *= 100;

  // Add unknown asset modifier (popularity_multiplier * 5)
  int asset_modifier = 5 * player.popularity_multiplier();

  // Add player modifiers (this would come from save game data)
  int capital = 0;            // TODO dword_83E8C - player's capital/assets
  int net_profit_or_loss = 0; // TODO dword_83E7C - player's profit/loss
  if (net_profit_or_loss >= 0) {
    capital += net_profit_or_loss;
  }

  // Calculate total creditworthiness
  int total_creditworthiness = base_credit_score + asset_modifier + capital;
  int creditworthiness_score = total_creditworthiness / 100;

  // Calculate loan amount using bank's loan multiplier
  int loan_amount =
      creditworthiness_score * bank_settings_.loan_amount_multiplier * 8;

  // Apply difficulty scaling
  int difficulty_level = state_.difficulty_level();
  loan_amount >>= difficulty_level; // Right shift by difficulty level

  // Round to nearest 1000
  loan_amount = math::round_up_to_next_power10<3>(loan_amount);

  int base_interest_rate = state_.global_interest_rate();
  int interest_rate =
      base_interest_rate + bank_settings_.interest_rate_modifier;

  // Loan term is always 12 months
  const int months = 12;

  return Loan(core::BankId{actor_->id()}, Money{loan_amount * 100},
              interest_rate, months);
}

Investment BankConversation::propose_investment() const {
  auto* player = state_.active_player();
  auto cash = player->cash_balance();
  Money amount = math::round_up_to_next_power10<3>(cash * 0.5);
  if (cash > 3'000'000'000_usd) {
    amount = 2'000'000'000_usd;
  }
  // Investments are hardcoded to 8% interest and 2 months
  return {core::BankId{actor_->id()}, state_.active_city_id(), amount, 8, 2};
}

int BankConversation::round_to_nearest(int value, int rounding_unit) const {
  return static_cast<int>(
      std::round(static_cast<double>(value) / rounding_unit) * rounding_unit);
}

BankLine BankConversation::compute_start_line(const GameState& state,
                                              const Actor*) {
  auto* player = state.active_player();
  if (player && player->cash_balance().cents < 0) {
    return BankLine::ACCOUNT_OVERDRAWN;
  }
  return BankLine::HELLO_MAY_I_HELP_YOU;
}

BankConversation::PopupRequest BankConversation::popup_request() const {
  return wants_cheque_ ? PopupRequest::CHEQUE : PopupRequest::NONE;
}

void BankConversation::on_cheque_result(int amount, int account) {
  wants_cheque_ = false;

  bool ok = amount > 0 && account > 0 &&
            state_.bank_transfer(account, Money{amount * 100},
                                 *state_.active_player());
  current_line(ok ? BankLine::TRANSFER_SUCCEEDED : BankLine::TRANSFER_FAILED);
}
