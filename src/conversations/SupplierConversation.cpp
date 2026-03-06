/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "conversations/SupplierConversation.h"

#include <algorithm>
#include <cstddef>
#include <magic_enum/magic_enum.hpp>

#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/supply/FoodSupplier.h"
#include "core/supply/SupplierRelationship.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"

namespace {

bool is_greeting(SupplierLine line) {
  switch (line) {
  case SupplierLine::HELLO_SUGGEST_CONTRACT_TERMS:
  case SupplierLine::HELLO_GOOD_TO_SEE_YOU_SUGGEST_TERMS:
  case SupplierLine::OFFER_SPECIAL_PRICE_CANT_SAY_NO:
  case SupplierLine::LONG_TIME_NO_SEE_OFFER_CONDITIONS:
  case SupplierLine::LAST_TIME_ORDERED_MORE_MAYBE_DO_BUSINESS:
  case SupplierLine::WHATS_WRONG_HARDLY_ORDERED_OFFER_CONDITIONS:
    return true;
  default:
    return false;
  }
}

} // namespace

SupplierConversation::SupplierConversation(GameState& state,
                                           const FoodSupplier* supplier,
                                           std::vector<IngredientBatch> order,
                                           Money total_order_value)
    : BaseConversation(compute_greeting(state, supplier, total_order_value),
                       state, "t/lief.e", nullptr),
      supplier_(supplier), order_(std::move(order)),
      total_order_value_(total_order_value) {

  // Create a temporary Actor for ActorView rendering
  owned_actor_ =
      std::make_unique<Actor>(supplier->actor_type(), supplier->id());
  owned_actor_->head_id(supplier->head_id());
  owned_actor_->name(supplier->name());
  actor_ = owned_actor_.get();

  auto player_id = state_.active_player()->id();
  customer_tier_ = supplier_->customer_tier(player_id);
  negotiation_skill_ = state_.active_player()->negotiation_skill();

  // Demand threshold based on customer tier
  if (customer_tier_ >= 10) {
    demand_threshold_ = 4;
  }
  else if (customer_tier_ >= 4) {
    demand_threshold_ = 3;
  }
  else {
    demand_threshold_ = 2;
  }
  // Initialize contract terms
  const auto& rel = supplier_->relationship(player_id);
  contract_.total_value = total_order_value_;
  contract_.delivery_days = std::max<uint8_t>(28, rel.last_delivery_days);

  if (rel.contract_count == 0) {
    contract_.discount_score = 0;
    contract_.penalty = total_order_value_ * 5;
  }
  else {
    contract_.discount_score = std::min<int>(rel.relationship_score, 60);
    contract_.penalty = total_order_value_ * 3;
  }

  // Charge: waived for medium+ customers (bh > 3)
  if (customer_tier_ > 3) {
    contract_.charge = Money{0};
  }
  else {
    contract_.charge = Money{static_cast<int64_t>(rel.frustration * 100)} +
                       total_order_value_ / 30;
  }

  if (is_greeting(current_line_) && customer_tier_ <= 5) {
    int personality = state_.active_player()->character().personality();
    int trap_chance = std::max(0, 90 - personality);
    if (engine::random::rand_percentage_chance(trap_chance) &&
        engine::random::rand_percentage_chance(trap_chance)) {
      special_price_offered_ = true;
      contract_.total_value = contract_.total_value * 2;
    }
  }
}

SupplierLine
SupplierConversation::compute_greeting(const GameState& state,
                                       const FoodSupplier* supplier,
                                       Money total_order_value) {
  auto player_id = state.active_player()->id();
  const auto& rel = supplier->relationship(player_id);
  auto customer_tier = supplier->customer_tier(player_id);

  if (rel.credit_history > 0) {
    return SupplierLine::SETTLE_DEBTS_BEFORE_NEW_CONTRACT;
  }

  if (total_order_value < supplier->minimum_contract_amount()) {
    return SupplierLine::NEVER_ENTER_CONTRACTS_BELOW_TOTAL;
  }

  if (rel.contract_count == 0) {
    return SupplierLine::HELLO_SUGGEST_CONTRACT_TERMS;
  }

  if (customer_tier >= 10) {
    return SupplierLine::OFFER_SPECIAL_PRICE_CANT_SAY_NO;
  }

  if (customer_tier >= 4) {
    return SupplierLine::HELLO_GOOD_TO_SEE_YOU_SUGGEST_TERMS;
  }

  if (rel.total_spent > 0_usd && customer_tier <= 1) {
    return SupplierLine::WHATS_WRONG_HARDLY_ORDERED_OFFER_CONDITIONS;
  }

  return SupplierLine::LONG_TIME_NO_SEE_OFFER_CONDITIONS;
}

bool SupplierConversation::advance() { return true; }

bool SupplierConversation::advance_line_choice(SupplierLine choice) {
  Logger::debug("SupplierConversation::advance_line_choice: {} => {}",
                magic_enum::enum_name(current_line_),
                magic_enum::enum_name(choice));

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
      int bonus = (customer_tier_ > 6) ? 8 : 5;
      contract_.discount_score =
          std::min<int>(contract_.discount_score + bonus, 60);
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

  if (agree) {
    auto player_id = state_.active_player()->id();
    auto& rel = const_cast<FoodSupplier*>(supplier_)->relationship(player_id);
    rel.contract_count++;
    rel.total_spent += total_order_value_;
    if (rel.frustration > 0) {
      rel.frustration--;
    }
    rel.last_delivery_days = contract_.delivery_days;

    if (total_order_value_ > 0_usd &&
        !state_.spend_cash(Account::Stock, total_order_value_)) {
      rel.contract_count--;
      rel.total_spent -= total_order_value_;
      current_line(SupplierLine::NEVER_ENTER_CONTRACTS_BELOW_TOTAL);
      return true;
    }

    auto* restaurant = state_.active_restaurant();
    for (const auto& batch : order_) {
      if (batch.count > 0) {
        auto ingredient_type = batch.ingredient_type;
        restaurant->purchase_ingredients(ingredient_type, batch.count,
                                         batch.quality, batch.price_per_unit);
      }
    }

    auto* city = state_.active_city();
    city->record_vendor_purchase(state_.active_player()->id(), supplier_->id(),
                                 total_order_value_);

    current_line(SupplierLine::OK_WILL_DO_OUR_BEST_DELIVER_ON_TIME);
  }
  else {
    // Player wants to negotiate
    if (customer_tier_ < 1) {
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
    auto& rel = const_cast<FoodSupplier*>(supplier_)->relationship(player_id);
    rel.frustration = std::min<int>(rel.frustration + 3, 255);
    current_line(SupplierLine::KEEP_REPEATING_TAKE_ME_FOR_A_RIDE);
    return;
  }

  switch (demand) {
  case SupplierLine::DISCOUNT_TOO_SMALL:
  case SupplierLine::TOO_EXPENSIVE: {
    if (customer_tier_ < 1) {
      contract_.charge = contract_.charge * 2;
      current_line(SupplierLine::HARDLY_BUY_ANYTHING_YET_DARE_TO_DEMAND);
      return;
    }

    // Count total discount demands (both types share a handler)
    int total_disc = session_demand_counts_[0] + session_demand_counts_[1];

    // Too many discount demands relative to customer size
    if (total_disc * 2 > customer_tier_) {
      auto player_id = state_.active_player()->id();
      auto& rel = const_cast<FoodSupplier*>(supplier_)->relationship(player_id);
      rel.frustration = std::min<int>(rel.frustration + 1, 255);
      current_line(SupplierLine::NO_REASON_FOR_SPECIAL_CONDITIONS);
      return;
    }

    // Hard cap: too many demands or discount already maxed
    if (total_disc >= demand_threshold_ || contract_.discount_score >= 50) {
      current_line(SupplierLine::NO_REASON_FOR_SPECIAL_CONDITIONS);
      return;
    }

    // Skill check: (negotiation_skill - 20)%
    if (engine::random::rand_percentage_chance(
            std::max(0, negotiation_skill_ - 20))) {
      contract_.discount_score =
          std::min<int>(contract_.discount_score + 5, 60);
      current_line(SupplierLine::I_THINK_I_CAN_HELP);
      return;
    }

    // Secondary check
    if (engine::random::rand_percentage_chance(50)) {
      int bonus =
          engine::random::rand_percentage_chance(negotiation_skill_) ? 10 : 5;
      contract_.discount_score =
          std::min<int>(contract_.discount_score + bonus, 60);
      current_line(SupplierLine::OK_GOOD_CUSTOMER_LETS_TALK);
      return;
    }

    // Failed: lose some discount
    contract_.discount_score =
        static_cast<uint8_t>(std::max<int>(contract_.discount_score - 4, 0));
    current_line(SupplierLine::DOESNT_IMPRESS_ME);
    break;
  }

  case SupplierLine::PENALTY_CLAUSE_EXCESSIVE: {
    if (customer_tier_ < 2) {
      current_line(SupplierLine::PENALTY_CLAUSE_JUSTIFIED);
      return;
    }

    if (!engine::random::rand_percentage_chance(negotiation_skill_)) {
      current_line(SupplierLine::IM_SORRY_CANNOT_ALTER_CONDITIONS);
      return;
    }

    if (engine::random::rand_percentage_chance(30) &&
        engine::random::rand_percentage_chance(50)) {
      contract_.penalty = contract_.penalty / 2;
      current_line(SupplierLine::I_THINK_I_CAN_HELP);
      return;
    }

    current_line(SupplierLine::IM_SORRY_CANNOT_ALTER_CONDITIONS);
    break;
  }

  case SupplierLine::FEES_TOO_HIGH: {
    // If no charge exists, punish for complaining about nothing
    if (contract_.charge == 0_usd) {
      auto player_id = state_.active_player()->id();
      auto& rel = const_cast<FoodSupplier*>(supplier_)->relationship(player_id);
      rel.frustration = std::min<int>(rel.frustration + 5, 255);
      contract_.charge = 100_usd;
      current_line(SupplierLine::THERE_WERE_NO_FEES_INCLUDED_BUT_IF_YOU_WANT);
      return;
    }

    if (engine::random::rand_percentage_chance(negotiation_skill_)) {
      contract_.charge = contract_.charge / 2;
      current_line(SupplierLine::ALRIGHT_LET_YOU_OFF_SOME_FEES);
      return;
    }

    if (engine::random::rand_percentage_chance(50)) {
      contract_.charge = contract_.charge / 2;
      current_line(SupplierLine::I_THINK_I_CAN_HELP);
      return;
    }

    current_line(SupplierLine::NO_REASON_FOR_SPECIAL_CONDITIONS);
    break;
  }

  case SupplierLine::TOTAL_IS_WRONG: {
    if (special_price_offered_) {
      contract_.total_value = contract_.total_value / 2;
      special_price_offered_ = false;
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
    if (rel.credit_history == 0) {
      current_line(SupplierLine::WE_ALWAYS_DELIVERED_ON_TIME);
      return;
    }
    // Supplier admits problems, gives free discount
    contract_.discount_score = std::min<int>(contract_.discount_score + 5, 60);
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
    size_t j = i + (engine::random::rand_max_n(shuffled.size()));
    std::swap(shuffled[i], shuffled[j]);
  }
  active_demand_options_.assign(shuffled.begin(), shuffled.begin() + 4);
  current_line(SupplierLine::DISCOUNT_TOO_SMALL);
}

Emotion SupplierConversation::emotion() const {
  switch (current_line_) {
  case SupplierLine::KEEP_REPEATING_TAKE_ME_FOR_A_RIDE:
  case SupplierLine::DISGRACE_TRY_LIES_ON_SOMEBODY_ELSE:
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
  case SupplierLine::HELLO_SUGGEST_CONTRACT_TERMS:
  case SupplierLine::HELLO_GOOD_TO_SEE_YOU_SUGGEST_TERMS:
  case SupplierLine::OFFER_SPECIAL_PRICE_CANT_SAY_NO:
  case SupplierLine::LONG_TIME_NO_SEE_OFFER_CONDITIONS:
  case SupplierLine::LAST_TIME_ORDERED_MORE_MAYBE_DO_BUSINESS:
  case SupplierLine::WHATS_WRONG_HARDLY_ORDERED_OFFER_CONDITIONS:
    return {make_proposal(current_line_, true)};

  // Demand menu: show the randomly picked options
  case SupplierLine::DISCOUNT_TOO_SMALL: {
    std::vector<ConversationLine<SupplierLine>> result;
    result.reserve(active_demand_options_.size());
    for (auto opt : active_demand_options_) {
      result.push_back(make_option(opt));
    }
    return result;
  }

  // Rejection/terminal lines
  case SupplierLine::NEVER_ENTER_CONTRACTS_BELOW_TOTAL:
    return {
        make_line(SupplierLine::NEVER_ENTER_CONTRACTS_BELOW_TOTAL,
                  {.is_proposal = true,
                   .easy_accept = false,
                   .custom_text = compat::format(
                       " {:w}", supplier_->minimum_contract_amount())}),
    };
  case SupplierLine::SETTLE_DEBTS_BEFORE_NEW_CONTRACT:
  case SupplierLine::GOODBYE:
    return {make_line(current_line_)};

  // Demand responses: shown as proposals (user can accept updated terms or
  // reject to negotiate further)
  case SupplierLine::JUSTIFY_YOUR_DEMANDS:
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
  case SupplierLine::ARE_YOU_TRYING_TO_BE_FINNY_YOU_COULDNT_PAY_MY_BILL:
  case SupplierLine::IM_SORRY_CANNOT_ALTER_CONDITIONS:
  case SupplierLine::THERE_WERE_NO_FEES_INCLUDED_BUT_IF_YOU_WANT:
  case SupplierLine::ALRIGHT_LET_YOU_OFF_SOME_FEES:
  case SupplierLine::NO_REASON_FOR_SPECIAL_CONDITIONS:
  case SupplierLine::INCREASE_DISCOUNT_A_LITTLE:
  case SupplierLine::THREE_MONTHS_LONGEST_TERM:
    return {make_proposal(current_line_, true)};

  // Terminal states (click to dismiss)
  case SupplierLine::OK_WILL_DO_OUR_BEST_DELIVER_ON_TIME:
  case SupplierLine::PLEASED_TO_SUPPLY_HURRY_UP:
  case SupplierLine::HOPE_DO_BUSINESS_NEXT_TIME:
  case SupplierLine::MAYBE_DO_BUSINESS_ANOTHER_TIME:
  case SupplierLine::KEEP_REPEATING_TAKE_ME_FOR_A_RIDE:
    return {make_line(current_line_)};

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
  case SupplierLine::GOODBYE:
    // case SupplierLine::OK_WILL_DO_OUR_BEST_DELIVER_ON_TIME:
    // case SupplierLine::PLEASED_TO_SUPPLY_HURRY_UP:
    // case SupplierLine::HOPE_DO_BUSINESS_NEXT_TIME:
    // case SupplierLine::MAYBE_DO_BUSINESS_ANOTHER_TIME:
    // case SupplierLine::KEEP_REPEATING_TAKE_ME_FOR_A_RIDE:
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
  case SupplierLine::OK_WILL_DO_OUR_BEST_DELIVER_ON_TIME:
  case SupplierLine::PLEASED_TO_SUPPLY_HURRY_UP:
  case SupplierLine::GOODBYE:
  case SupplierLine::HOPE_DO_BUSINESS_NEXT_TIME:
  case SupplierLine::MAYBE_DO_BUSINESS_ANOTHER_TIME:
  case SupplierLine::KEEP_REPEATING_TAKE_ME_FOR_A_RIDE:
  case SupplierLine::NEVER_ENTER_CONTRACTS_BELOW_TOTAL:
  case SupplierLine::SETTLE_DEBTS_BEFORE_NEW_CONTRACT:
    return true;
  default:
    return false;
  }
}
