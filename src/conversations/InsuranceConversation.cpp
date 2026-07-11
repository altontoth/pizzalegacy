/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "conversations/InsuranceConversation.h"

#include <magic_enum/magic_enum.hpp>

#include "conversations/InsuranceLine.h"
#include "core/character/Character.h"
#include "core/character/Player.h"
#include "core/finance/Ledger.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/Random.h"

using engine::random::coin_flip;
using engine::random::rand_max_n;
using engine::random::rand_percentage_chance;

namespace {

InsuranceType type_from_choice(InsuranceLine choice) {
  return static_cast<InsuranceType>(
      static_cast<int>(choice) -
      static_cast<int>(InsuranceLine::THEFT_INSURANCE));
}

constexpr int type_multiplier(InsuranceType t) {
  return static_cast<int>(t) + 1;
}

constexpr int type_index(InsuranceType t) { return static_cast<int>(t); }

} // anonymous namespace

InsuranceConversation::InsuranceConversation(GameState& state,
                                             const Actor* actor,
                                             const InsuranceSettings& is,
                                             ActorId actor_id)
    : BaseConversation(compute_start_line(state, actor), state, "t/versich.e",
                       actor),
      insurance_settings_(is), actor_id_(actor_id) {}

bool InsuranceConversation::advance() {
  switch (current_line_) {
  case InsuranceLine::ITS_A_WONDERFUL_DAY_YOU_CAN_FEEL_SAFE_WITH_US:
    current_line(InsuranceLine::I_NEED_INSURANCE);
    break;
  case InsuranceLine::WHAT_KIND_OF_INSURANCE_CAN_I_GIVE_YOU:
  case InsuranceLine::WHAT_KIND_OF_CLAIM_DO_YOU_WISH_TO_MAKE:
    current_line(InsuranceLine::THEFT_INSURANCE);
    break;
  case InsuranceLine::CONGRATULATIONS_YOU_WONT_REGRET_TAKING_POLICY_WITH_US:
  case InsuranceLine::SORRY_NOT_NOTIFIED_OF_CLAIM:
  case InsuranceLine::YOU_MUST_BE_MISTAKEN_YOU_DONT_HAVE:
  case InsuranceLine::
      IM_SORRY_IN_VIEW_OF_YOUR_CIRCUMSTANCES_YOU_ARE_TOO_GREAT_RISK:
  case InsuranceLine::THERES_NO_ROOM_FOR_SYMPATHY_TOUGH_BUSINESS:
  case InsuranceLine::I_AM_TERRIBLY_SORRY_BUT_OTHER_MATTERS_TO_ATTEND_TO:
    current_line(InsuranceLine::I_NEED_INSURANCE);
    break;
  case InsuranceLine::YES_I_BELIEVE_BUT_LOOK_AT_THE_EXTREMELY_SMALL_PRINT:
    current_line(type_specific_excuse());
    break;
  case InsuranceLine::IT_SAYS_HERE_ACTS_OF_GOD_CANNOT_BE_RULED_OUT:
  case InsuranceLine::IT_SAYS_HERE_ONLY_IF_ROBBERY_BETWEEN_ONE_AND_TWO:
  case InsuranceLine::ONLY_IF_NO_PREVIOUS_CONVICTIONS:
  case InsuranceLine::LIFE_INSURANCE_ONLY_IF_YOU_ARE_DEAD:
    current_line(InsuranceLine::HOW_ABOUT_10_BIG_ONES_FOR_YOU);
    break;
  case InsuranceLine::THEN_SOME_OTHER_ACT_DOESNT_MAKE_DIFFERENCE:
  case InsuranceLine::AH_CONDITIONS_CHANGED_NOW_BETWEEN_FOUR_AND_SIX:
  case InsuranceLine::OH_REALLY_HAVE_YOU_FORGOTTEN_BOX_OF_CANDY_IN_KINDERGARTEN:
  case InsuranceLine::
      WHAT_ARE_YOU_THINKING_OF_WE_ARE_RESPECTABLE_WILL_IGNORE_LAST_REMARK:
    current_line(
        InsuranceLine::I_AM_TERRIBLY_SORRY_BUT_OTHER_MATTERS_TO_ATTEND_TO);
    break;
  case InsuranceLine::AH_YES_PREMIUM_WILL_INCREASE:
    current_line(
        InsuranceLine::
            FOR_THIS_REASON_WE_HAVE_TO_INCREASE_PREMIUM_NOW_PREMIUM_IS);
    break;
  case InsuranceLine::LIFE_CLAIM_REBUTTAL:
    current_line(
        InsuranceLine::I_AM_TERRIBLY_SORRY_BUT_OTHER_MATTERS_TO_ATTEND_TO);
    break;
  case InsuranceLine::I_BELIEVE_WE_HAVE_JUST_WHAT_YOU_NEED:
    current_line(static_cast<InsuranceLine>(
        static_cast<int>(
            InsuranceLine::WHAT_ABOUT_THIRD_PARTY_FIRE_AND_THEFT_INSURANCE) +
        rand_max_n(5)));
    break;
  default:
    Logger::warn("InsuranceConversation::advance() unhandled state {}",
                 magic_enum::enum_name(current_line_));
    return false;
  }
  return true;
}

bool InsuranceConversation::advance_line_choice(InsuranceLine choice) {
  auto* player = state_.active_player();
  const auto& date = state_.time().current_date();

  switch (choice) {
  case InsuranceLine::I_NEED_INSURANCE:
    current_line(InsuranceLine::WHAT_KIND_OF_INSURANCE_CAN_I_GIVE_YOU);
    break;
  case InsuranceLine::I_WANT_TO_MAKE_A_CLAIM:
    current_line(InsuranceLine::WHAT_KIND_OF_CLAIM_DO_YOU_WISH_TO_MAKE);
    break;
  case InsuranceLine::I_WOULD_LIKE_SOME_ADVICE:
    current_line(InsuranceLine::I_BELIEVE_WE_HAVE_JUST_WHAT_YOU_NEED);
    break;
  case InsuranceLine::I_WANT_TO_CANCEL_THE_LEASE:
    current_line(InsuranceLine::THEFT_INSURANCE);
    break;

  case InsuranceLine::THEFT_INSURANCE:
  case InsuranceLine::WAREHOUSE_INSURANCE:
  case InsuranceLine::FIRE_INSURANCE:
  case InsuranceLine::LIABILITY_INSURANCE:
  case InsuranceLine::LIFE_INSURANCE: {
    selected_type_ = type_from_choice(choice);
    int risk = risk_score();

    if (previous_line_ ==
        InsuranceLine::WHAT_KIND_OF_INSURANCE_CAN_I_GIVE_YOU) {
      // Buying flow
      if (insurance_settings_.willingness_threshold < risk) {
        current_line(
            InsuranceLine::
                IM_SORRY_IN_VIEW_OF_YOUR_CIRCUMSTANCES_YOU_ARE_TOO_GREAT_RISK);
        break;
      }
      pending_premium_ =
          Money{static_cast<int64_t>(type_multiplier(selected_type_)) *
                (insurance_settings_.premium_factor + risk) * 12 * 100};
      pending_max_coverage_ =
          Money{static_cast<int64_t>(type_multiplier(selected_type_)) *
                insurance_settings_.willingness_threshold * 800 * 100 /
                (1 << state_.difficulty_level())};
      current_line(InsuranceLine::OK_THE_MONTHLY_PREMIUM_IS);
    }
    else if (previous_line_ ==
             InsuranceLine::WHAT_KIND_OF_CLAIM_DO_YOU_WISH_TO_MAKE) {
      // Claim flow
      auto& ins_state = player->insurance_state();
      const auto& policy = ins_state.policies[type_index(selected_type_)];
      bool valid = policy.is_active() && policy.actor_id == actor_id_ &&
                   ins_state.pending_claim == selected_type_;
      if (!valid) {
        current_line(InsuranceLine::SORRY_NOT_NOTIFIED_OF_CLAIM);
        break;
      }
      // Roll for immediate refusal
      if (rand_percentage_chance(insurance_settings_.rejection_percentage)) {
        ins_state.pending_claim = std::nullopt;
        current_line(
            InsuranceLine::THATS_NOT_MY_RESPONSIBILITY_COLLEAGUE_IN_THAIRLAND);
        break;
      }
      // Next roll for payout
      if (rand_percentage_chance(insurance_settings_.premium_factor)) {
        auto increased = execute_payout(true);
        current_line(increased
                         ? InsuranceLine::AH_YES_PREMIUM_WILL_INCREASE
                         : InsuranceLine::AH_YES_YOU_ARE_RIGHT_WE_WILL_PAY);
      }
      else {
        current_line(
            InsuranceLine::YES_I_BELIEVE_BUT_LOOK_AT_THE_EXTREMELY_SMALL_PRINT);
      }
    }
    else {
      // Cancel flow (previous_line_ I_NEED_INSURANCE)
      auto& ins_state = player->insurance_state();
      auto& policy = ins_state.policies[type_index(selected_type_)];
      if (!policy.is_active() || policy.actor_id != actor_id_) {
        current_line(InsuranceLine::YOU_MUST_BE_MISTAKEN_YOU_DONT_HAVE);
        break;
      }
      // Charge cancellation fee and clear slot
      player->financials().spend_cash(date, Account::Others,
                                      Money{static_cast<int64_t>(2000) * 100});
      policy = InsurancePolicy{};
      current_line(InsuranceLine::ALL_THAT_REMAINS_IS_2K_REDUNDANCY_PROVISION);
    }
    break;
  }

  // let's argue with them:
  case InsuranceLine::BUT_IT_WASNT_AN_ACT_OF_GOD:
  case InsuranceLine::YES_THATS_EXACTLY_WHAT_HAPPENED:
  case InsuranceLine::BUT_I_DONT_HAVE_CONVICTIONS:
  case InsuranceLine::SO_MUCH_THE_WORSE_YOU_SHOULD_BE_GLAD_TO_BE_ALIVE: {
    auto rebuttal = type_specific_rebuttal();
    if (rebuttal == InsuranceLine::LIFE_CLAIM_REBUTTAL &&
        state_.options().fix_legacy_bugs) {
      // Original shows the premium-increase message but forgets to actually
      // increase the premium. With bug fixes enabled, we apply the doubling.
      auto& policy =
          player->insurance_state().policies[type_index(selected_type_)];
      policy.monthly_premium = Money{policy.monthly_premium.cents * 2};
    }
    current_line(rebuttal);
    break;
  }

  // bribe
  case InsuranceLine::HOW_ABOUT_10_BIG_ONES_FOR_YOU: {
    int personality = player->character().personality();
    if (rand_percentage_chance(personality)) {
      player->financials().spend_cash(date, Account::FinesAndBribes,
                                      Money{static_cast<int64_t>(10000) * 100});
      auto increased = execute_payout(true);
      current_line(increased ? InsuranceLine::AH_YES_PREMIUM_WILL_INCREASE
                             : InsuranceLine::AH_YES_YOU_ARE_RIGHT_WE_WILL_PAY);
    }
    else {
      current_line(
          InsuranceLine::
              WHAT_ARE_YOU_THINKING_OF_WE_ARE_RESPECTABLE_WILL_IGNORE_LAST_REMARK);
    }
    break;
  }

  // aww poor me
  case InsuranceLine::BUT_THEN_ID_BE_DONE_FOR: {
    int roll = rand_max_n(100);
    if (roll < 5) {
      execute_payout(false);
      current_line(
          InsuranceLine::NO_PLEASE_DONT_MY_DOCTOR_FORBIDS_OUTBREAKS_OF_EMOTION);
    }
    else if (roll < 55) {
      current_line(InsuranceLine::DONT_WORRY_ILL_TAKE_CARE_FOR_20_BIG_ONES);
    }
    else {
      current_line(InsuranceLine::THERES_NO_ROOM_FOR_SYMPATHY_TOUGH_BUSINESS);
    }
    break;
  }

  default:
    Logger::warn("InsuranceConversation::advance_line_choice() unhandled {}",
                 magic_enum::enum_name(choice));
    return false;
  }
  return true;
}

bool InsuranceConversation::advance_proposal(bool agree) {
  auto* player = state_.active_player();
  const auto& date = state_.time().current_date();

  switch (current_line_) {
  case InsuranceLine::OK_THE_MONTHLY_PREMIUM_IS:
    if (agree) {
      auto& ins_state = player->insurance_state();
      auto& policy = ins_state.policies[type_index(selected_type_)];
      policy.actor_id = actor_id_;
      policy.company_name = actor_->name();
      policy.monthly_premium = pending_premium_;
      policy.max_coverage = pending_max_coverage_;
      // A new policy doesn't cover events that already occurred.
      if (ins_state.pending_claim == selected_type_) {
        ins_state.pending_claim = std::nullopt;
      }
      current_line(
          InsuranceLine::CONGRATULATIONS_YOU_WONT_REGRET_TAKING_POLICY_WITH_US);
    }
    else {
      current_line(InsuranceLine::INSURANCE_OFFER_DECLINED);
    }
    break;

  case InsuranceLine::DONT_WORRY_ILL_TAKE_CARE_FOR_20_BIG_ONES:
    if (agree) {
      player->financials().spend_cash(date, Account::FinesAndBribes,
                                      Money{static_cast<int64_t>(20000) * 100});
      auto increased = execute_payout(true);
      current_line(increased ? InsuranceLine::AH_YES_PREMIUM_WILL_INCREASE
                             : InsuranceLine::AH_YES_YOU_ARE_RIGHT_WE_WILL_PAY);
    }
    else {
      current_line(InsuranceLine::THERES_NO_ROOM_FOR_SYMPATHY_TOUGH_BUSINESS);
    }
    break;

  // Joke insurance; doesn't do anything.
  case InsuranceLine::WHAT_ABOUT_THIRD_PARTY_FIRE_AND_THEFT_INSURANCE:
  case InsuranceLine::FOR_200_WE_HAVE_BANANA_SKIN_POLICY:
  case InsuranceLine::FOR_300_JAIL_BREAK_INSURANCE:
  case InsuranceLine::OR_WHAT_ABOUT_WHOLE_LIFE_RISK_INSURANCE:
  case InsuranceLine::HAVE_YOU_HEARD_ABOUT_RUSSIAN_ROULETTE_INSURANCE:
    if (agree) {
      current_line(
          InsuranceLine::CONGRATULATIONS_YOU_WONT_REGRET_TAKING_POLICY_WITH_US);
    }
    else {
      current_line(InsuranceLine::INSURANCE_OFFER_DECLINED);
    }
    break;

  default:
    return advance();
  }
  return true;
}

Emotion InsuranceConversation::emotion() const {
  switch (current_line_) {
  case InsuranceLine::AH_YES_YOU_ARE_RIGHT_WE_WILL_PAY:
  case InsuranceLine::AH_YES_PREMIUM_WILL_INCREASE:
  case InsuranceLine::CONGRATULATIONS_YOU_WONT_REGRET_TAKING_POLICY_WITH_US:
  case InsuranceLine::NO_PLEASE_DONT_MY_DOCTOR_FORBIDS_OUTBREAKS_OF_EMOTION:
    return Emotion::HAPPY;
  case InsuranceLine::
      IM_SORRY_IN_VIEW_OF_YOUR_CIRCUMSTANCES_YOU_ARE_TOO_GREAT_RISK:
  case InsuranceLine::
      WHAT_ARE_YOU_THINKING_OF_WE_ARE_RESPECTABLE_WILL_IGNORE_LAST_REMARK:
  case InsuranceLine::I_AM_TERRIBLY_SORRY_BUT_OTHER_MATTERS_TO_ATTEND_TO:
    return Emotion::ANGRY;
  default:
    return Emotion::NEUTRAL;
  }
}

std::vector<ConversationLine<InsuranceLine>>
InsuranceConversation::lines() const {
  switch (current_line_) {
  case InsuranceLine::I_NEED_INSURANCE:
    return {
        make_option(InsuranceLine::I_NEED_INSURANCE),
        make_option(InsuranceLine::I_WANT_TO_MAKE_A_CLAIM),
        make_option(InsuranceLine::I_WOULD_LIKE_SOME_ADVICE),
        make_option(InsuranceLine::I_WANT_TO_CANCEL_THE_LEASE),
    };

  case InsuranceLine::THEFT_INSURANCE:
    return {
        make_option(InsuranceLine::THEFT_INSURANCE),
        make_option(InsuranceLine::WAREHOUSE_INSURANCE),
        make_option(InsuranceLine::FIRE_INSURANCE),
        make_option(InsuranceLine::LIABILITY_INSURANCE),
        make_option(InsuranceLine::LIFE_INSURANCE),
    };

  case InsuranceLine::HOW_ABOUT_10_BIG_ONES_FOR_YOU:
    return {
        make_option(type_specific_counter_arg()),
        make_option(InsuranceLine::HOW_ABOUT_10_BIG_ONES_FOR_YOU),
        make_option(InsuranceLine::BUT_THEN_ID_BE_DONE_FOR),
    };

  case InsuranceLine::OK_THE_MONTHLY_PREMIUM_IS:
    return {
        make_concat_proposal(InsuranceLine::OK_THE_MONTHLY_PREMIUM_IS,
                             compat::format(" {}  ", pending_premium_),
                             InsuranceLine::THE_MAXIMUM_SUM_INSURED_IS,
                             compat::format(" {}. Would you sign please?",
                                            pending_max_coverage_)),
    };

  case InsuranceLine::DONT_WORRY_ILL_TAKE_CARE_FOR_20_BIG_ONES:
    return {make_proposal(
        InsuranceLine::DONT_WORRY_ILL_TAKE_CARE_FOR_20_BIG_ONES, false)};

  case InsuranceLine::WHAT_ABOUT_THIRD_PARTY_FIRE_AND_THEFT_INSURANCE:
  case InsuranceLine::FOR_200_WE_HAVE_BANANA_SKIN_POLICY:
  case InsuranceLine::FOR_300_JAIL_BREAK_INSURANCE:
  case InsuranceLine::OR_WHAT_ABOUT_WHOLE_LIFE_RISK_INSURANCE:
  case InsuranceLine::HAVE_YOU_HEARD_ABOUT_RUSSIAN_ROULETTE_INSURANCE:
    return {make_proposal(current_line_, false)};

  case InsuranceLine::AH_YES_YOU_ARE_RIGHT_WE_WILL_PAY:
  case InsuranceLine::THATS_NOT_MY_RESPONSIBILITY_COLLEAGUE_IN_THAIRLAND:
  case InsuranceLine::NO_PLEASE_DONT_MY_DOCTOR_FORBIDS_OUTBREAKS_OF_EMOTION:
  case InsuranceLine::ALL_THAT_REMAINS_IS_2K_REDUNDANCY_PROVISION:
    return {make_terminal_line(current_line_)};

  case InsuranceLine::
      FOR_THIS_REASON_WE_HAVE_TO_INCREASE_PREMIUM_NOW_PREMIUM_IS: {
    auto& policy = state_.active_player()
                       ->insurance_state()
                       .policies[type_index(selected_type_)];
    return {make_terminal_line(
        InsuranceLine::
            FOR_THIS_REASON_WE_HAVE_TO_INCREASE_PREMIUM_NOW_PREMIUM_IS,
        compat::format(" {}", policy.monthly_premium))};
  }

  // Life claim rebuttal: shows the premium-increase text as click-through.
  // The actual doubling (if enabled) happens on state entry in
  // advance_line_choice().
  case InsuranceLine::LIFE_CLAIM_REBUTTAL: {
    auto& policy = state_.active_player()
                       ->insurance_state()
                       .policies[type_index(selected_type_)];
    return {make_line(
        InsuranceLine::
            FOR_THIS_REASON_WE_HAVE_TO_INCREASE_PREMIUM_NOW_PREMIUM_IS,
        {.custom_text = compat::format(" {}", policy.monthly_premium)})};
  }

  // Payout confirmed and premium will increase: show "we will pay" text as
  // click-through so advance() can follow up with the premium-increase message.
  case InsuranceLine::AH_YES_PREMIUM_WILL_INCREASE:
    return {make_line(InsuranceLine::AH_YES_YOU_ARE_RIGHT_WE_WILL_PAY)};

  // Silent exit: player declined the insurance offer.
  case InsuranceLine::INSURANCE_OFFER_DECLINED:
    return {};

  case InsuranceLine::ITS_A_WONDERFUL_DAY_YOU_CAN_FEEL_SAFE_WITH_US:
  case InsuranceLine::WHAT_KIND_OF_INSURANCE_CAN_I_GIVE_YOU:
  case InsuranceLine::WHAT_KIND_OF_CLAIM_DO_YOU_WISH_TO_MAKE:
  case InsuranceLine::
      IM_SORRY_IN_VIEW_OF_YOUR_CIRCUMSTANCES_YOU_ARE_TOO_GREAT_RISK:
  case InsuranceLine::CONGRATULATIONS_YOU_WONT_REGRET_TAKING_POLICY_WITH_US:
  case InsuranceLine::SORRY_NOT_NOTIFIED_OF_CLAIM:
  case InsuranceLine::YOU_MUST_BE_MISTAKEN_YOU_DONT_HAVE:
  case InsuranceLine::YES_I_BELIEVE_BUT_LOOK_AT_THE_EXTREMELY_SMALL_PRINT:
  case InsuranceLine::THERES_NO_ROOM_FOR_SYMPATHY_TOUGH_BUSINESS:
  case InsuranceLine::I_AM_TERRIBLY_SORRY_BUT_OTHER_MATTERS_TO_ATTEND_TO:
  case InsuranceLine::
      WHAT_ARE_YOU_THINKING_OF_WE_ARE_RESPECTABLE_WILL_IGNORE_LAST_REMARK:
  case InsuranceLine::IT_SAYS_HERE_ACTS_OF_GOD_CANNOT_BE_RULED_OUT:
  case InsuranceLine::IT_SAYS_HERE_ONLY_IF_ROBBERY_BETWEEN_ONE_AND_TWO:
  case InsuranceLine::ONLY_IF_NO_PREVIOUS_CONVICTIONS:
  case InsuranceLine::LIFE_INSURANCE_ONLY_IF_YOU_ARE_DEAD:
  case InsuranceLine::THEN_SOME_OTHER_ACT_DOESNT_MAKE_DIFFERENCE:
  case InsuranceLine::AH_CONDITIONS_CHANGED_NOW_BETWEEN_FOUR_AND_SIX:
  case InsuranceLine::OH_REALLY_HAVE_YOU_FORGOTTEN_BOX_OF_CANDY_IN_KINDERGARTEN:
  case InsuranceLine::I_BELIEVE_WE_HAVE_JUST_WHAT_YOU_NEED:
    return {make_line(current_line_)};

  default:
    Logger::warn("InsuranceConversation::lines() unhandled state {}",
                 magic_enum::enum_name(current_line_));
    break;
  }
  return {};
}

bool InsuranceConversation::has_ended() const {
  switch (current_line_) {
  case InsuranceLine::INSURANCE_OFFER_DECLINED:
  case InsuranceLine::AH_YES_YOU_ARE_RIGHT_WE_WILL_PAY:
  case InsuranceLine::
      FOR_THIS_REASON_WE_HAVE_TO_INCREASE_PREMIUM_NOW_PREMIUM_IS:
    return true;
  default:
    return false;
  }
}

InsuranceLine
InsuranceConversation::compute_start_line(const GameState& /*state*/,
                                          const Actor* /*actor*/) {
  return InsuranceLine::ITS_A_WONDERFUL_DAY_YOU_CAN_FEEL_SAFE_WITH_US;
}

int InsuranceConversation::risk_score() const {
  int score = 0;
  auto* player = state_.active_player();
  auto underworld = player->social_status().underworld_score();
  auto social = player->social_status().social_score();
  if (underworld > 500) {
    score += 10;
  }
  if (underworld > 1500) {
    score += 10;
  }
  if (underworld > social) {
    score += 20;
  }
  if (social > 2000) {
    score -= 10;
  }
  return score;
}

bool InsuranceConversation::execute_payout(bool may_increase_premium) {
  auto* player = state_.active_player();
  const auto& date = state_.time().current_date();
  auto& ins_state = player->insurance_state();
  auto& policy = ins_state.policies[type_index(selected_type_)];

  ins_state.pending_claim = std::nullopt;
  player->financials().spend_cash(date, Account::Others, -policy.max_coverage);

  if (may_increase_premium && coin_flip()) {
    policy.monthly_premium = Money{policy.monthly_premium.cents * 2};
    return true;
  }
  return false;
}

InsuranceLine InsuranceConversation::type_specific_excuse() const {
  switch (selected_type_) {
  case InsuranceType::THEFT:
  case InsuranceType::WAREHOUSE:
    return InsuranceLine::IT_SAYS_HERE_ACTS_OF_GOD_CANNOT_BE_RULED_OUT;
  case InsuranceType::FIRE:
    return InsuranceLine::IT_SAYS_HERE_ONLY_IF_ROBBERY_BETWEEN_ONE_AND_TWO;
  case InsuranceType::LIABILITY:
    return InsuranceLine::ONLY_IF_NO_PREVIOUS_CONVICTIONS;
  case InsuranceType::LIFE:
    return InsuranceLine::LIFE_INSURANCE_ONLY_IF_YOU_ARE_DEAD;
  }
}

InsuranceLine InsuranceConversation::type_specific_counter_arg() const {
  switch (selected_type_) {
  case InsuranceType::THEFT:
  case InsuranceType::WAREHOUSE:
    return InsuranceLine::BUT_IT_WASNT_AN_ACT_OF_GOD;
  case InsuranceType::FIRE:
    return InsuranceLine::YES_THATS_EXACTLY_WHAT_HAPPENED;
  case InsuranceType::LIABILITY:
    return InsuranceLine::BUT_I_DONT_HAVE_CONVICTIONS;
  case InsuranceType::LIFE:
    return InsuranceLine::SO_MUCH_THE_WORSE_YOU_SHOULD_BE_GLAD_TO_BE_ALIVE;
  }
}

InsuranceLine InsuranceConversation::type_specific_rebuttal() const {
  switch (selected_type_) {
  case InsuranceType::THEFT:
  case InsuranceType::WAREHOUSE:
    return InsuranceLine::THEN_SOME_OTHER_ACT_DOESNT_MAKE_DIFFERENCE;
  case InsuranceType::FIRE:
    return InsuranceLine::AH_CONDITIONS_CHANGED_NOW_BETWEEN_FOUR_AND_SIX;
  case InsuranceType::LIABILITY:
    return InsuranceLine::
        OH_REALLY_HAVE_YOU_FORGOTTEN_BOX_OF_CANDY_IN_KINDERGARTEN;
  case InsuranceType::LIFE:
    return InsuranceLine::LIFE_CLAIM_REBUTTAL;
  }
}
