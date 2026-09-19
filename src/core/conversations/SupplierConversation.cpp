/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/conversations/SupplierConversation.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <magic_enum/magic_enum.hpp>
#include <utility>
#include <vector>

#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/conversations/SupplierLine.h"
#include "core/events/MilestoneId.h"
#include "core/supply/ActiveContract.h"
#include "core/supply/FoodSupplier.h"
#include "core/supply/SupplierContract.h"
#include "core/supply/SupplierRelationship.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/Random.h"

SupplierConversation::SupplierConversation(GameState& state,
                                           const FoodSupplier* supplier,
                                           std::vector<IngredientBatch> order,
                                           Money total_order_value)
    : BaseConversation(compute_greeting(state, supplier, total_order_value),
                       state, "t/lief.e", nullptr),
      supplier_(supplier), order_(std::move(order)),
      total_order_value_(total_order_value) {

  owned_actor_ = std::make_unique<Actor>(supplier->actor_type(),
                                         core::ActorId{supplier->id()});
  owned_actor_->head_id(supplier->head_id());
  owned_actor_->name(supplier->name());
  actor_ = owned_actor_.get();

  auto player_id = state_.active_player()->id();
  spending_rank_ = supplier_->spending_rank(player_id);
  negotiation_skill_ = state_.active_player()->negotiation_skill();

  // Demand threshold (how often can you refuse an offer) based on customer tier
  if (spending_rank_ >= 10) {
    demand_limit_ = 4;
  }
  else if (spending_rank_ >= 4) {
    demand_limit_ = 3;
  }
  else {
    demand_limit_ = 2;
  }
  const auto& rel = supplier_->relationship(player_id);
  contract_ = opening_contract_terms(*supplier_, rel, spending_rank_,
                                     total_order_value_);

  if (current_line_ == SupplierLine::HELLO_SUGGEST_CONTRACT_TERMS) {
    int personality = state_.active_player()->character().personality();
    int ripoff_chance = std::max(0, 90 - personality);
    if (engine::random::rand_percentage_chance(ripoff_chance)) {
      if (engine::random::rand_percentage_chance(ripoff_chance)) {
        ripoff_ = true;
        contract_.total_value = contract_.total_value * 2;
      }
    }
  }
  Logger::debug(
      "Player spending rank is {}, rip off is {}, demand threshold is {}",
      spending_rank_, ripoff_, demand_limit_);
}

SupplierLine
SupplierConversation::compute_greeting(const GameState& state,
                                       const FoodSupplier* supplier,
                                       Money total_order_value) {
  auto player_id = state.active_player()->id();
  const auto& rel = supplier->relationship(player_id);
  auto spending_rank = supplier->spending_rank(player_id);

  if (rel.outstanding_debt > total_order_value) {
    return SupplierLine::SETTLE_DEBTS_BEFORE_NEW_CONTRACT;
  }

  if (total_order_value < supplier->minimum_contract_amount()) {
    return SupplierLine::NEVER_ENTER_CONTRACTS_BELOW_TOTAL;
  }

  if (rel.last_contract_sum.cents > total_order_value.cents) {
    return SupplierLine::LAST_TIME_ORDERED_MORE_MAYBE_DO_BUSINESS;
  }

  // Rank-based greeting selection (original thresholds)
  if (spending_rank > 10) {
    return SupplierLine::OFFER_SPECIAL_PRICE_CANT_SAY_NO;
  }
  if (spending_rank >= 6) {
    return SupplierLine::HELLO_GOOD_TO_SEE_YOU_SUGGEST_TERMS;
  }
  if (spending_rank >= 1) {
    return SupplierLine::HELLO_SUGGEST_CONTRACT_TERMS;
  }
  return SupplierLine::PLEASED_TO_SUPPLY_HURRY_UP;
}

bool SupplierConversation::advance() {
  if (current_line_ == SupplierLine::JUSTIFY_YOUR_DEMANDS &&
      !active_argument_options_.empty()) {
    current_line(SupplierLine::PICKING_ARGUMENT);
  }
  return true;
}

bool SupplierConversation::advance_line_choice(SupplierLine choice) {
  Logger::debug("SupplierConversation::advance_line_choice: {} => {}",
                magic_enum::enum_name(current_line_),
                magic_enum::enum_name(choice));

  if (current_line_ == SupplierLine::PICKING_ARGUMENT) {
    handle_argument_pick(choice);
    return true;
  }

  switch (choice) {
  case SupplierLine::DISCOUNT_TOO_SMALL:
  case SupplierLine::TOO_EXPENSIVE:
  case SupplierLine::PENALTY_CLAUSE_EXCESSIVE:
  case SupplierLine::FEES_TOO_HIGH:
  case SupplierLine::TOTAL_IS_WRONG:
  case SupplierLine::DELIVERIES_UNRELIABLE:
  case SupplierLine::TERM_TOO_SHORT:
  case SupplierLine::TERM_TOO_LONG:
    handle_demand(choice);
    break;

  case SupplierLine::SECOND_THOUGHTS: {
    if (engine::random::rand_percentage_chance(30) &&
        contract_.discount_score < 50) {
      int bonus = (spending_rank_ > 6) ? 8 : 5;
      contract_.discount_score = static_cast<uint8_t>(
          std::min<int>(contract_.discount_score + bonus, 60));
      current_line(SupplierLine::INCREASE_DISCOUNT_A_LITTLE);
    }
    else {
      current_line(SupplierLine::HOPE_DO_BUSINESS_NEXT_TIME);
    }
    break;
  }

  default:
    Logger::warn("SupplierConversation::advance_line_choice: unhandled {}",
                 magic_enum::enum_name(choice));
    return false;
  }
  return true;
}

bool SupplierConversation::advance_proposal(bool agree) {
  Logger::debug("SupplierConversation::advance_proposal: {}: {}",
                magic_enum::enum_name(current_line_), agree ? 1 : 0);
  if (!is_proposal()) {
    return advance();
  }
  if (agree) {
    auto player_id = state_.active_player()->id();
    auto& rel = supplier_->relationship(player_id);

    std::vector<ActiveContract::Item> items;
    for (const auto& batch : order_) {
      if (batch.count > 0) {
        items.push_back({batch.ingredient_type, batch.count});
      }
    }
    sign_contract(*state_.active_player(), *supplier_, rel, contract_,
                  total_order_value_, state_.active_city_id(),
                  state_.active_restaurant()->id(), std::move(items));

    if (contract_.discount_score >= 50) {
      state_.record_milestone(
          core::events::MilestoneId::ManagedToNegotiate50PctDiscount,
          state_.active_player()->id());
    }

    contract_accepted_ = true;
    current_line(SupplierLine::OK_WILL_DO_OUR_BEST_DELIVER_ON_TIME);
  }
  else {
    // Player wants to negotiate
    if (spending_rank_ < 1) {
      current_line(SupplierLine::MAYBE_DO_BUSINESS_ANOTHER_TIME);
    }
    else {
      pick_demand_options();
    }
  }
  return true;
}

void SupplierConversation::handle_demand(SupplierLine demand) {
  int demand_idx = static_cast<int>(demand) -
                   static_cast<int>(SupplierLine::DISCOUNT_TOO_SMALL);
  assert(demand_idx >= 0 &&
         demand_idx < static_cast<int>(session_demand_counts_.size()));

  session_demand_counts_[demand_idx]++;
  int repeat_count = session_demand_counts_[demand_idx];

  // Repeat detection
  if (repeat_count == 2) {
    current_line(SupplierLine::YOU_JUST_SAID_THAT);
    return;
  }
  if (repeat_count >= 3) {
    auto player_id = state_.active_player()->id();
    auto& rel = supplier_->relationship(player_id);
    rel.frustration =
        static_cast<uint8_t>(std::min<int>(rel.frustration + 3, 255));
    current_line(SupplierLine::KEEP_REPEATING_TAKE_ME_FOR_A_RIDE);
    return;
  }

  switch (demand) {
  case SupplierLine::DISCOUNT_TOO_SMALL:
  case SupplierLine::TOO_EXPENSIVE: {
    {
      auto player_id = state_.active_player()->id();
      if (supplier_->relationship(player_id).total_spent <
          supplier_->minimum_contract_amount() * 2) {
        contract_.charge = contract_.charge * 2;
        current_line(SupplierLine::HARDLY_BUY_ANYTHING_YET_DARE_TO_DEMAND);
        return;
      }
    }

    // Count total discount demands
    int demand_count = session_demand_counts_[0] + session_demand_counts_[1];

    // Too many discount demands relative to customer size
    if (demand_count * 2 > spending_rank_) {
      auto player_id = state_.active_player()->id();
      auto& rel = supplier_->relationship(player_id);
      rel.frustration =
          static_cast<uint8_t>(std::min<int>(rel.frustration + 1, 255));
      current_line(SupplierLine::NO_REASON_FOR_SPECIAL_CONDITIONS);
      return;
    }

    // Hard cap: too many demands or discount already maxed
    if (demand_count >= demand_limit_ || contract_.discount_score >= 50) {
      current_line(SupplierLine::NO_REASON_FOR_SPECIAL_CONDITIONS);
      return;
    }

    // Skill check: (negotiation_skill - 20)%
    if (engine::random::rand_percentage_chance(
            std::max(0, negotiation_skill_ - 20))) {
      contract_.discount_score =
          static_cast<uint8_t>(std::min<int>(contract_.discount_score + 5, 60));
      current_line(SupplierLine::I_THINK_I_CAN_HELP);
      return;
    }

    trigger_justify_demands(JustifyContext::DISCOUNT);
    return;
  }

  case SupplierLine::PENALTY_CLAUSE_EXCESSIVE: {
    if (spending_rank_ < 2) {
      current_line(SupplierLine::PENALTY_CLAUSE_JUSTIFIED);
      return;
    }

    {
      auto player_id = state_.active_player()->id();
      const auto& rel = supplier_->relationship(player_id);
      if (spending_rank_ < rel.missed_payment_penalty) {
        current_line(
            SupplierLine::ARE_YOU_TRYING_TO_BE_FUNNY_YOU_COULDNT_PAY_MY_BILL);
        return;
      }
    }

    if (!engine::random::rand_percentage_chance(negotiation_skill_)) {
      if (!state_.rules().fix_legacy_bugs) {
        contract_.penalty = contract_.penalty / 2;
      }
      current_line(SupplierLine::NO_REASON_FOR_SPECIAL_CONDITIONS);
      return;
    }

    if (!engine::random::rand_percentage_chance(30)) {
      current_line(SupplierLine::IM_SORRY_CANNOT_ALTER_CONDITIONS);
      return;
    }

    trigger_justify_demands(JustifyContext::PENALTY);
    return;
  }

  case SupplierLine::FEES_TOO_HIGH: {
    // If no charge exists, punish for complaining about nothing
    if (contract_.charge == 0_usd) {
      auto player_id = state_.active_player()->id();
      auto& rel = supplier_->relationship(player_id);
      rel.frustration =
          static_cast<uint8_t>(std::min<int>(rel.frustration + 5, 255));
      contract_.charge = 100_usd;
      current_line(SupplierLine::THERE_WERE_NO_FEES_INCLUDED_BUT_IF_YOU_WANT);
      return;
    }

    if (engine::random::rand_percentage_chance(negotiation_skill_)) {
      contract_.charge = contract_.charge / 2;
      current_line(SupplierLine::ALRIGHT_LET_YOU_OFF_SOME_FEES);
      return;
    }

    trigger_justify_demands(JustifyContext::FEES);
    return;
  }

  case SupplierLine::TOTAL_IS_WRONG: {
    if (ripoff_) {
      contract_.total_value = contract_.total_value / 2;
      ripoff_ = false;
      current_line(SupplierLine::SORRY_MY_MISTAKE_YOUR_CONDITIONS);
      return;
    }
    // Player is wrong - show angry face but continue negotiation
    current_line(SupplierLine::DISGRACE_TRY_LIES_ON_SOMEBODY_ELSE);
    break;
  }

  case SupplierLine::DELIVERIES_UNRELIABLE: {
    auto player_id = state_.active_player()->id();
    const auto& rel = supplier_->relationship(player_id);
    if (rel.delivery_issues_flag == 0) {
      current_line(SupplierLine::WE_ALWAYS_DELIVERED_ON_TIME);
      return;
    }
    // From DOS version of Pizza Connection: if player has a payment default on
    // record, supplier deflects instead of admitting fault and giving a
    // discount (I think the jump was missing in PT).
    if (rel.missed_payment_penalty > 0) {
      current_line(SupplierLine::OTOH_YOU_WERE_BEHIND_ON_PAYMENTS);
      return;
    }
    contract_.discount_score =
        static_cast<uint8_t>(std::min<int>(contract_.discount_score + 5, 60));
    current_line(SupplierLine::ADMIT_SHORTAGES_MORE_DISCOUNT);
    break;
  }

  case SupplierLine::TERM_TOO_SHORT: {
    contract_.delivery_days =
        static_cast<uint8_t>(std::min<int>(contract_.delivery_days + 14, 255));
    current_line(SupplierLine::MEET_YOU_HALF_WAY_ON_TERM);
    break;
  }

  case SupplierLine::TERM_TOO_LONG: {
    if (contract_.delivery_days <= 7) {
      current_line(SupplierLine::WEEK_IS_SHORTEST_TERM);
      return;
    }
    if (contract_.delivery_days >= 84) {
      current_line(SupplierLine::THREE_MONTHS_LONGEST_TERM);
      return;
    }
    contract_.delivery_days =
        static_cast<uint8_t>(std::max<int>(contract_.delivery_days - 7, 7));
    current_line(SupplierLine::MEET_YOU_HALF_WAY_ON_TERM);
    break;
  }

  default:
    current_line(SupplierLine::JUSTIFY_YOUR_DEMANDS);
    break;
  }
}

void SupplierConversation::trigger_justify_demands(JustifyContext ctx) {
  justify_context_ = ctx;
  // Pool: enum 7-26 = LIEF.E lines 8-27
  std::array<SupplierLine, 20> pool;
  for (int i = 0; i < 20; ++i) {
    pool[i] = static_cast<SupplierLine>(7 + i);
  }
  for (size_t i = 0; i < 4; i++) {
    size_t j =
        i + engine::random::rand_max_n(static_cast<int>(pool.size() - i));
    std::swap(pool[i], pool[j]);
  }
  active_argument_options_.assign(pool.begin(), pool.begin() + 4);
  current_line(SupplierLine::JUSTIFY_YOUR_DEMANDS);
}

void SupplierConversation::handle_argument_pick(SupplierLine choice) {
  assert(justify_context_.has_value());
  active_argument_options_.clear();
  // LIEF.E line = enum + 1; the original uses this directly as a % chance
  int chance = static_cast<int>(choice) + 1;
  bool success = engine::random::rand_percentage_chance(chance);

  switch (*justify_context_) {
  case JustifyContext::DISCOUNT:
    if (success) {
      int bonus =
          engine::random::rand_percentage_chance(negotiation_skill_) ? 10 : 5;
      contract_.discount_score = static_cast<uint8_t>(
          std::min<int>(contract_.discount_score + bonus, 60));
      current_line(SupplierLine::OK_GOOD_CUSTOMER_LETS_TALK);
    }
    else {
      contract_.discount_score =
          static_cast<uint8_t>(std::max<int>(contract_.discount_score - 4, 0));
      current_line(SupplierLine::DOESNT_IMPRESS_ME);
    }
    break;
  case JustifyContext::PENALTY:
    if (success) {
      contract_.penalty = contract_.penalty / 2;
      current_line(SupplierLine::I_THINK_I_CAN_HELP);
    }
    else {
      current_line(SupplierLine::IM_SORRY_CANNOT_ALTER_CONDITIONS);
    }
    break;
  case JustifyContext::FEES:
    if (success) {
      contract_.charge = contract_.charge / 2;
      current_line(SupplierLine::I_THINK_I_CAN_HELP);
    }
    else {
      current_line(SupplierLine::NO_REASON_FOR_SPECIAL_CONDITIONS);
    }
    break;
  }
}

void SupplierConversation::pick_demand_options() {
  static constexpr std::array all_demands = {
      SupplierLine::DISCOUNT_TOO_SMALL,
      SupplierLine::TOO_EXPENSIVE,
      SupplierLine::PENALTY_CLAUSE_EXCESSIVE,
      SupplierLine::FEES_TOO_HIGH,
      SupplierLine::TOTAL_IS_WRONG,
      SupplierLine::DELIVERIES_UNRELIABLE,
      SupplierLine::TERM_TOO_SHORT,
      SupplierLine::TERM_TOO_LONG,
      SupplierLine::SECOND_THOUGHTS,
  };
  auto shuffled = all_demands;
  for (size_t i = 0; i < 4; i++) {
    size_t j =
        i + engine::random::rand_max_n(static_cast<int>(shuffled.size() - i));
    std::swap(shuffled[i], shuffled[j]);
  }
  active_demand_options_.assign(shuffled.begin(), shuffled.begin() + 4);
  current_line(SupplierLine::DISCOUNT_TOO_SMALL);
}

Emotion SupplierConversation::emotion() const {
  switch (current_line_) {
  case SupplierLine::KEEP_REPEATING_TAKE_ME_FOR_A_RIDE:
  case SupplierLine::DISGRACE_TRY_LIES_ON_SOMEBODY_ELSE:
  case SupplierLine::HOPE_DO_BUSINESS_NEXT_TIME:
    return Emotion::ANGRY;
  case SupplierLine::OK_WILL_DO_OUR_BEST_DELIVER_ON_TIME:
    return Emotion::HAPPY;
  default:
    return Emotion::NEUTRAL;
  }
}

std::vector<ConversationLine<SupplierLine>>
SupplierConversation::lines() const {
  switch (current_line_) {
  case SupplierLine::PLEASED_TO_SUPPLY_HURRY_UP:
  case SupplierLine::HELLO_SUGGEST_CONTRACT_TERMS:
  case SupplierLine::HELLO_GOOD_TO_SEE_YOU_SUGGEST_TERMS:
  case SupplierLine::OFFER_SPECIAL_PRICE_CANT_SAY_NO:
  case SupplierLine::LAST_TIME_ORDERED_MORE_MAYBE_DO_BUSINESS:
    return {make_proposal(current_line_, false)};

  // Demand menu: show the randomly picked options
  case SupplierLine::DISCOUNT_TOO_SMALL: {
    std::vector<ConversationLine<SupplierLine>> result;
    result.reserve(active_demand_options_.size());
    for (auto opt : active_demand_options_) {
      result.push_back(make_option(opt));
    }
    return result;
  }

  case SupplierLine::NEVER_ENTER_CONTRACTS_BELOW_TOTAL:
    return {
        make_line(SupplierLine::NEVER_ENTER_CONTRACTS_BELOW_TOTAL,
                  {.is_proposal = false,
                   .easy_accept = false,
                   .is_terminal = true,
                   .custom_text =
                       compat::format(" {:w}! Goodbye!",
                                      supplier_->minimum_contract_amount())}),
    };
  case SupplierLine::SETTLE_DEBTS_BEFORE_NEW_CONTRACT:
  case SupplierLine::GOODBYE:
    return {make_terminal_line(current_line_)};
  case SupplierLine::JUSTIFY_YOUR_DEMANDS:
    return {make_line(current_line_)};

  case SupplierLine::PICKING_ARGUMENT: {
    std::vector<ConversationLine<SupplierLine>> result;
    result.reserve(active_argument_options_.size());
    for (auto opt : active_argument_options_) {
      result.push_back(make_option(opt));
    }
    return result;
  }

  case SupplierLine::NO_REASON_FOR_SPECIAL_CONDITIONS:
    return {make_proposal(current_line_, true)};

  case SupplierLine::YOU_JUST_SAID_THAT:
  case SupplierLine::HARDLY_BUY_ANYTHING_YET_DARE_TO_DEMAND:
  case SupplierLine::OK_GOOD_CUSTOMER_LETS_TALK:
  case SupplierLine::WE_ALWAYS_DELIVERED_ON_TIME:
  case SupplierLine::ADMIT_SHORTAGES_MORE_DISCOUNT:
  case SupplierLine::OTOH_YOU_WERE_BEHIND_ON_PAYMENTS:
  case SupplierLine::WEEK_IS_SHORTEST_TERM:
  case SupplierLine::MEET_YOU_HALF_WAY_ON_TERM:
  case SupplierLine::DISGRACE_TRY_LIES_ON_SOMEBODY_ELSE:
  case SupplierLine::THATS_NOT_TRUE_LAST_TIME:
  case SupplierLine::THAT_WAS_LONG_AGO_MY_FRIEND:
  case SupplierLine::YOU_ORDERED_MORE_LAST_TIME:
  case SupplierLine::SORRY_MY_MISTAKE_YOUR_CONDITIONS:
  case SupplierLine::DOESNT_IMPRESS_ME:
  case SupplierLine::I_THINK_I_CAN_HELP:
  case SupplierLine::PENALTY_CLAUSE_JUSTIFIED:
  case SupplierLine::ARE_YOU_TRYING_TO_BE_FUNNY_YOU_COULDNT_PAY_MY_BILL:
  case SupplierLine::IM_SORRY_CANNOT_ALTER_CONDITIONS:
  case SupplierLine::THERE_WERE_NO_FEES_INCLUDED_BUT_IF_YOU_WANT:
  case SupplierLine::ALRIGHT_LET_YOU_OFF_SOME_FEES:
  case SupplierLine::INCREASE_DISCOUNT_A_LITTLE:
  case SupplierLine::THREE_MONTHS_LONGEST_TERM:
    return {make_proposal(current_line_, true)};

  case SupplierLine::OK_WILL_DO_OUR_BEST_DELIVER_ON_TIME:
  case SupplierLine::HOPE_DO_BUSINESS_NEXT_TIME:
  case SupplierLine::MAYBE_DO_BUSINESS_ANOTHER_TIME:
  case SupplierLine::KEEP_REPEATING_TAKE_ME_FOR_A_RIDE:
    return {make_terminal_line(current_line_)};

  default:
    Logger::warn("SupplierConversation::lines: unhandled {}",
                 magic_enum::enum_name(current_line_));
    return {};
  }
}

std::optional<ContractTermsDisplay>
SupplierConversation::contract_terms() const {
  // Hide terms for terminal/ending states
  switch (current_line_) {
  case SupplierLine::NEVER_ENTER_CONTRACTS_BELOW_TOTAL:
  case SupplierLine::SETTLE_DEBTS_BEFORE_NEW_CONTRACT:
  case SupplierLine::MAYBE_DO_BUSINESS_ANOTHER_TIME:
  case SupplierLine::GOODBYE:
    return std::nullopt;
  default:
    break;
  }

  return ContractTermsDisplay{
      .sum = contract_.total_value,
      .discount_pct = contract_.discount_score,
      .charge = contract_.charge,
      .penalty = contract_.penalty,
      .term_days = contract_.delivery_days,
  };
}

bool SupplierConversation::has_ended() const {
  switch (current_line_) {
  case SupplierLine::KEEP_REPEATING_TAKE_ME_FOR_A_RIDE:
    return true;
  default:
    return false;
  }
}
