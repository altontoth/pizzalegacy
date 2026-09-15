/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/conversations/PoliceConversation.h"

#include <algorithm>

#include "core/character/InsurancePlayerState.h"
#include "core/character/Player.h"
#include "core/character/SocialStatus.h"
#include "core/city/CentralDepot.h"
#include "core/city/City.h"
#include "core/city/District.h"
#include "core/events/MilestoneId.h"
#include "core/finance/Ledger.h"
#include "core/inventory/Inventory.h"
#include "core/item/Item.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/EndOfDayCause.h"
#include "engine/Fatal.h"
#include "engine/FormatCompat.h"
#include "engine/Math.h"
#include "engine/Random.h"

namespace {

// Visits more than this many days in the future don't qualify, neither do
// restaurants that have never been visited by the mayor.
constexpr int mayor_tip_horizon_days = 100;

// Even though the text file says $20K and $80K, the original uses these
// numbers instead.
constexpr Money cash_bribe = 10'000_usd;
constexpr Money sports_car_bribe = 50'000_usd;

} // namespace

PoliceConversation::PoliceConversation(GameState& state, const Actor* actor,
                                       std::optional<ArrestReason> ar)
    : BaseConversation(compute_start_line(state, actor, ar), state,
                       "t/polizei.e", actor),
      arrest_reason_(ar) {

  auto* player = state_.active_player();
  int difficulty = state_.difficulty_level();

  if (arrest_reason_) {
    int crime_code = static_cast<int>(*arrest_reason_);

    int uw_rank =
        social_rank(player->social_status().underworld_score(), difficulty);
    sentence_days_ = std::clamp((uw_rank + difficulty - 2) * crime_code, 3, 60);

    int raw_bribe = (engine::random::rand_max_n(500) + 500) *
                    (sentence_days_ + player->criminal_record());
    bribe_or_fine_cost_ = Money::from_dollars(
        std::max(math::round_up_to_next_power10<3>(raw_bribe), int64_t{4000}));

    player->increment_criminal_record();
    apply_confiscation();
  }
  else {
    int business_rank =
        social_rank(player->social_status().social_score(), difficulty);

    if (business_rank < 2) {
      return; // player is not welcome, so don't set up anything else
    }

    // Generate random flags for option variants
    for (auto& flag : random_flags_) {
      flag = engine::random::coin_flip();
    }

    mayor_tip_ = mayor_tip(state_, player->id());

    // TODO: Find depot inspection date

    // Compute donation amount
    donation_amount_ =
        Money::from_dollars((random_flags_[3] ? 2 : 1) * 2000 * business_rank);
  }
}

std::optional<PoliceConversation::MayorTip>
PoliceConversation::mayor_tip(const GameState& state, core::PlayerId player) {
  // The tip names the restaurant whose next visit is soonest, ties going to
  // the one opened earliest.
  std::optional<MayorTip> tip;
  int soonest = mayor_tip_horizon_days;
  for (const auto* restaurant :
       state.restaurants_in_city(player, state.active_city_id())) {
    if (restaurant->first_inspection_pending() ||
        restaurant->inspection_days_remaining() >= soonest) {
      continue;
    }
    soonest = restaurant->inspection_days_remaining();
    tip = MayorTip{restaurant->name(), soonest};
  }
  return tip;
}

bool PoliceConversation::warehouse_tip_available(const GameState& state,
                                                 core::PlayerId player) {
  auto* city = state.active_city();
  return city != nullptr && city->central_depot(player).capacity() > 0 &&
         state.district_under_investigation().has_value() &&
         static_cast<int>(state.time().current_date().day()) >=
             GameState::district_investigation_draw_day;
}

std::optional<std::string>
PoliceConversation::investigated_district(const GameState& state) {
  auto drawn = state.district_under_investigation();
  auto* city = state.active_city();
  if (!drawn || !city) {
    return std::nullopt;
  }
  for (const auto& district : city->districts()) {
    if (district->id() == *drawn) {
      return district->name();
    }
  }
  return std::nullopt;
}

PoliceLine
PoliceConversation::compute_start_line(const GameState& state,
                                       const Actor* /*actor*/,
                                       std::optional<ArrestReason> reason) {
  if (!reason) {
    auto* player = state.active_player();
    int difficulty = state.difficulty_level();
    int business_rank =
        social_rank(player->social_status().social_score(), difficulty);
    if (business_rank < 2) {
      return PoliceLine::WHO_ARE_YOU_GET_OUT_OF_HERE_AT_ONCE;
    }
    return PoliceLine::IN_THE_NAME_OF_THE_LAW_MAY_I_WELCOME_YOU;
  }

  int crime_code = static_cast<int>(*reason);
  return static_cast<PoliceLine>(crime_code - 1);
}

bool PoliceConversation::advance() {
  switch (current_line_) {
  case PoliceLine::IN_THE_NAME_OF_THE_LAW_MAY_I_WELCOME_YOU:
    current_line(PoliceLine::ANY_INFO_FOR_SALE);
    break;

  case PoliceLine::IN_NAME_OF_LAW_ARREST_RECEIVING:
  case PoliceLine::IN_NAME_OF_LAW_ARREST_BURGLARY:
  case PoliceLine::IN_NAME_OF_LAW_ARREST_WEAPONS_DEALING:
  case PoliceLine::IN_NAME_OF_LAW_ARREST_LAUNDERING_MONEY:
  case PoliceLine::IN_NAME_OF_LAW_ARREST_SHADY_DEALINGS:
  case PoliceLine::IN_NAME_OF_LAW_ARREST_HOMICIDE:
  case PoliceLine::IN_NAME_OF_LAW_ARREST_POSESSING_WEAPONS: {
    auto* player = state_.active_player();
    int difficulty = state_.difficulty_level();
    int business_rank =
        social_rank(player->social_status().social_score(), difficulty);
    if (business_rank > 6) {
      if (!arrest_reason_) {
        fatal("Don't have an arrest reason when I do expect one.");
      }
      int crime_code = static_cast<int>(*arrest_reason_);
      state_.adjust_social_score(*player, -80 * crime_code);
      current_line(
          PoliceLine::IN_VIEW_OF_YOUR_GOOD_STANDING_NOT_TAKE_IT_FURTHER);
    }
    else {
      current_line(
          PoliceLine::
              YOU_HAVE_BEEN_SENTENCED_TO_X_DAYS_IN_JAIL_OR_WOULD_YOU_RATHER);
    }
    break;
  }

  default:
    std::cerr << "I don't know how to advance without choice from state "
              << current_line_ << "\n";
    return false;
  }
  return true;
}

bool PoliceConversation::advance_line_choice(PoliceLine choice) {
  auto* player = state_.active_player();
  int difficulty = state_.difficulty_level();

  switch (choice) {
  case PoliceLine::SO_ILL_GO_TO_JAIL_SO_WHAT:
    go_to_jail(sentence_days_);
    current_line(PoliceLine::PLAYER_GOING_TO_JAIL);
    break;

  case PoliceLine::HOW_ABOUT_20K_FOR_YOU: {
    int chance = 70 - (difficulty * 12);
    if (engine::random::rand_percentage_chance(std::max(chance, 5)) &&
        player->cash_balance() >= cash_bribe) {
      player->financials().transfer(state_.time().current_date(), Account::Cash,
                                    Account::FinesAndBribes, cash_bribe,
                                    AccountingScope::Operational);
      current_line(PoliceLine::SURE_BUT_THIS_WILL_REMAIN_OUR_SECRET);
    }
    else {
      sentence_days_ *= 2;
      go_to_jail(sentence_days_);
      current_line(PoliceLine::YOU_CANNOT_BRIBE_ME_SENTENCED_DOUBLED);
    }
    break;
  }

  case PoliceLine::COULD_YOU_DO_WITH_A_SPORTS_CAR: {
    int chance = 80 - (difficulty * 10);
    if (engine::random::rand_percentage_chance(std::max(chance, 5)) &&
        player->cash_balance() >= sports_car_bribe) {
      player->financials().transfer(state_.time().current_date(), Account::Cash,
                                    Account::FinesAndBribes, sports_car_bribe,
                                    AccountingScope::Operational);
      current_line(
          PoliceLine::ALRIGHT_ILL_HAVE_ONE_FOR_80K_AND_YOU_CAN_GO_FREE);
    }
    else {
      sentence_days_ *= 2;
      go_to_jail(sentence_days_);
      current_line(PoliceLine::YOU_CANNOT_BRIBE_ME_SENTENCED_DOUBLED);
    }
    break;
  }

  case PoliceLine::ANY_INFO_FOR_SALE:
  case PoliceLine::SSSHH_DO_YOU_SELL_INFO:
    player->financials().transfer(state_.time().current_date(), Account::Cash,
                                  Account::FinesAndBribes, 1000_usd,
                                  AccountingScope::Operational);
    current_line(
        PoliceLine::WHAT_DO_YOU_THINK_I_AM_I_FINE_YOU_1K_FOR_INSULTING);
    break;

  case PoliceLine::HOW_ARE_YOU: {
    if (warehouse_tip_available(state_, player->id())) {
      current_line(
          PoliceLine::
              FINE_THANKS_I_EVEN_KNOW_WHERE_WE_WILL_BE_CHECKING_WAREHOUSE_NEXT_MONTH_FOR_4K);
    }
    else {
      current_line(PoliceLine::NOT_TOO_GOOD_IM_VERY_BUSY_GOODBYE);
    }
    break;
  }

  case PoliceLine::NICE_DAY: {
    // No restaurants in this city means no offer and no charisma roll.
    bool offer = false;
    if (!state_.restaurants_in_city(player->id(), state_.active_city_id())
             .empty()) {
      offer = engine::random::rand_percentage_chance(
                  player->character().charisma()) &&
              mayor_tip_.has_value();
    }

    if (offer) {
      current_line(
          PoliceLine::
              YES_INDEED_I_HAPPENED_TO_FIND_OUT_MAYOR_VISIT_RESTAURANT_FOR_2K);
    }
    else {
      current_line(PoliceLine::ITS_NOT_THAT_NICE_STOP_BOTHERING_ME);
    }
    break;
  }

  case PoliceLine::HOW_IS_YOUR_PETTY_CASH:
  case PoliceLine::DO_YOU_NEED_ANY_MONEY:
    go_to_jail(2);
    current_line(
        PoliceLine::
            IF_YOUR_TRYING_TO_BRIBE_ME_YOUVE_GOT_THE_WRONG_PERSON_2_DAYS_JAIL);
    break;

  case PoliceLine::DID_YOU_LOSE_THIS_WALLET:
  case PoliceLine::NEED_A_DONATION_TO_THE_LOCAL_POLICE: {
    if (player->cash_balance() >= donation_amount_) {
      player->financials().transfer(state_.time().current_date(), Account::Cash,
                                    Account::FinesAndBribes, donation_amount_,
                                    AccountingScope::Operational);
      state_.record_milestone(core::events::MilestoneId::FirstBribery,
                              player->id());
      current_line(PoliceLine::YES_THANKS_WHAT_WOULD_I_HAVE_DONE_WITHOUT);
    }
    else {
      current_line(PoliceLine::YOURE_OFFERING_ME_MONEY_YOU_DONT_EVEN_HAVE);
    }
    break;
  }

  default:
    std::cerr << "I don't know how to advance with choice " << choice
              << " from state " << current_line_ << "\n";
    return false;
  }
  return true;
}

bool PoliceConversation::advance_proposal(bool agree) {
  auto* player = state_.active_player();

  switch (current_line_) {
  case PoliceLine::
      YOU_HAVE_BEEN_SENTENCED_TO_X_DAYS_IN_JAIL_OR_WOULD_YOU_RATHER: {
    if (agree && player->cash_balance() >= bribe_or_fine_cost_) {
      player->financials().transfer(
          state_.time().current_date(), Account::Cash, Account::FinesAndBribes,
          bribe_or_fine_cost_, AccountingScope::Operational);
      if (!arrest_reason_) {
        fatal("Don't have an arrest reason when I do expect one.");
      }
      int crime_code = static_cast<int>(*arrest_reason_);
      state_.adjust_social_score(*player, -10 * crime_code);
      state_.record_milestone(
          core::events::MilestoneId::YouReceivedFineFirstTime, player->id());
      // Liability insurance covers fines you pay.
      player->make_insurance_claim(InsuranceType::LIABILITY);
      current_line(PoliceLine::PLAYER_PAID_FINE);
    }
    else {
      current_line(PoliceLine::SO_ILL_GO_TO_JAIL_SO_WHAT);
    }
    break;
  }

  case PoliceLine::
      FINE_THANKS_I_EVEN_KNOW_WHERE_WE_WILL_BE_CHECKING_WAREHOUSE_NEXT_MONTH_FOR_4K:
    if (agree) {
      player->financials().transfer(state_.time().current_date(), Account::Cash,
                                    Account::FinesAndBribes, 4000_usd,
                                    AccountingScope::Operational);
      current_line(
          investigated_district(state_)
              ? PoliceLine::
                    THANKS_A_LOT_NEXT_MONTH_WE_WILL_EXAMINE_WAREHOUSES_IN
              : PoliceLine::POLICE_HUNG_UP);
    }
    else {
      current_line(PoliceLine::NOT_TOO_GOOD_IM_VERY_BUSY_GOODBYE);
    }
    break;

  case PoliceLine::
      YES_INDEED_I_HAPPENED_TO_FIND_OUT_MAYOR_VISIT_RESTAURANT_FOR_2K:
    if (agree) {
      player->financials().transfer(state_.time().current_date(), Account::Cash,
                                    Account::FinesAndBribes, 2000_usd,
                                    AccountingScope::Operational);
      current_line(PoliceLine::THANK_YOU_THE_MAYOR_WILL_VISIT_YOUR_RESTAURANT);
    }
    else {
      current_line(PoliceLine::ITS_NOT_THAT_NICE_STOP_BOTHERING_ME);
    }
    break;

  default:
    return advance();
  }
  return true;
}

Emotion PoliceConversation::emotion() const {
  switch (current_line_) {
  case PoliceLine::WHAT_DO_YOU_THINK_I_AM_I_FINE_YOU_1K_FOR_INSULTING:
  case PoliceLine::
      IF_YOUR_TRYING_TO_BRIBE_ME_YOUVE_GOT_THE_WRONG_PERSON_2_DAYS_JAIL:
  case PoliceLine::YOU_CANNOT_BRIBE_ME_SENTENCED_DOUBLED:
  case PoliceLine::WHO_ARE_YOU_GET_OUT_OF_HERE_AT_ONCE:
    return Emotion::ANGRY;
  case PoliceLine::SURE_BUT_THIS_WILL_REMAIN_OUR_SECRET:
  case PoliceLine::ALRIGHT_ILL_HAVE_ONE_FOR_80K_AND_YOU_CAN_GO_FREE:
  case PoliceLine::THANK_YOU_THE_MAYOR_WILL_VISIT_YOUR_RESTAURANT:
  case PoliceLine::YES_THANKS_WHAT_WOULD_I_HAVE_DONE_WITHOUT:
    return Emotion::HAPPY;
  default:
    return Emotion::NEUTRAL;
  }
}

std::vector<ConversationLine<PoliceLine>> PoliceConversation::lines() const {
  switch (current_line_) {
  case PoliceLine::IN_THE_NAME_OF_THE_LAW_MAY_I_WELCOME_YOU:
  case PoliceLine::IN_NAME_OF_LAW_ARREST_RECEIVING:
  case PoliceLine::IN_NAME_OF_LAW_ARREST_BURGLARY:
  case PoliceLine::IN_NAME_OF_LAW_ARREST_WEAPONS_DEALING:
  case PoliceLine::IN_NAME_OF_LAW_ARREST_LAUNDERING_MONEY:
  case PoliceLine::IN_NAME_OF_LAW_ARREST_SHADY_DEALINGS:
  case PoliceLine::IN_NAME_OF_LAW_ARREST_HOMICIDE:
  case PoliceLine::IN_NAME_OF_LAW_ARREST_POSESSING_WEAPONS:
    return {make_line(current_line_)};

  case PoliceLine::ANY_INFO_FOR_SALE:
    return {
        make_option(random_flags_[0] ? PoliceLine::ANY_INFO_FOR_SALE
                                     : PoliceLine::SSSHH_DO_YOU_SELL_INFO),
        make_option(random_flags_[1] ? PoliceLine::HOW_ARE_YOU
                                     : PoliceLine::NICE_DAY),
        make_option(random_flags_[2] ? PoliceLine::HOW_IS_YOUR_PETTY_CASH
                                     : PoliceLine::DO_YOU_NEED_ANY_MONEY),
        make_option(random_flags_[3]
                        ? PoliceLine::DID_YOU_LOSE_THIS_WALLET
                        : PoliceLine::NEED_A_DONATION_TO_THE_LOCAL_POLICE),
    };

  case PoliceLine::WHAT_DO_YOU_THINK_I_AM_I_FINE_YOU_1K_FOR_INSULTING:
  case PoliceLine::NOT_TOO_GOOD_IM_VERY_BUSY_GOODBYE:
  case PoliceLine::ITS_NOT_THAT_NICE_STOP_BOTHERING_ME:
  case PoliceLine::
      IF_YOUR_TRYING_TO_BRIBE_ME_YOUVE_GOT_THE_WRONG_PERSON_2_DAYS_JAIL:
  case PoliceLine::WHO_ARE_YOU_GET_OUT_OF_HERE_AT_ONCE:
  case PoliceLine::IN_VIEW_OF_YOUR_GOOD_STANDING_NOT_TAKE_IT_FURTHER:
  case PoliceLine::YOURE_OFFERING_ME_MONEY_YOU_DONT_EVEN_HAVE:
  case PoliceLine::YOU_CANNOT_BRIBE_ME_SENTENCED_DOUBLED:
  case PoliceLine::SURE_BUT_THIS_WILL_REMAIN_OUR_SECRET:
  case PoliceLine::ALRIGHT_ILL_HAVE_ONE_FOR_80K_AND_YOU_CAN_GO_FREE:
    return {make_terminal_line(current_line_)};

  case PoliceLine::THANKS_A_LOT_NEXT_MONTH_WE_WILL_EXAMINE_WAREHOUSES_IN:
    return {make_terminal_line(
        PoliceLine::THANKS_A_LOT_NEXT_MONTH_WE_WILL_EXAMINE_WAREHOUSES_IN,
        compat::format(" {}.", *investigated_district(state_)))};
  case PoliceLine::THANK_YOU_THE_MAYOR_WILL_VISIT_YOUR_RESTAURANT:
    return {make_terminal_line(
        PoliceLine::THANK_YOU_THE_MAYOR_WILL_VISIT_YOUR_RESTAURANT,
        compat::format("{} in exactly {} days!", mayor_tip_->restaurant,
                       mayor_tip_->countdown_days))};

  case PoliceLine::YES_THANKS_WHAT_WOULD_I_HAVE_DONE_WITHOUT:
    return {make_terminal_line(
        PoliceLine::YES_THANKS_WHAT_WOULD_I_HAVE_DONE_WITHOUT,
        compat::format(
            " {:w}! {}", donation_amount_,
            lines_.at(static_cast<int>(
                PoliceLine::
                    THAT_WILL_COME_IN_HANDY_I_WONT_FORGET_YOUR_GENEROSITY))))};

  // Voluntary proposals
  case PoliceLine::
      FINE_THANKS_I_EVEN_KNOW_WHERE_WE_WILL_BE_CHECKING_WAREHOUSE_NEXT_MONTH_FOR_4K:
  case PoliceLine::
      YES_INDEED_I_HAPPENED_TO_FIND_OUT_MAYOR_VISIT_RESTAURANT_FOR_2K:
    return {make_proposal(current_line_, false)};

  case PoliceLine::
      YOU_HAVE_BEEN_SENTENCED_TO_X_DAYS_IN_JAIL_OR_WOULD_YOU_RATHER:
    return {make_proposal(
        PoliceLine::
            YOU_HAVE_BEEN_SENTENCED_TO_X_DAYS_IN_JAIL_OR_WOULD_YOU_RATHER,
        compat::format("You have been sentenced to {} days in jail! Or would "
                       "you rather pay a fine of {:w}?",
                       sentence_days_, bribe_or_fine_cost_))};

  case PoliceLine::SO_ILL_GO_TO_JAIL_SO_WHAT:
    return {
        make_option(PoliceLine::SO_ILL_GO_TO_JAIL_SO_WHAT),
        make_option(PoliceLine::HOW_ABOUT_20K_FOR_YOU),
        make_option(PoliceLine::COULD_YOU_DO_WITH_A_SPORTS_CAR),
    };

  case PoliceLine::DURING_INSPECTION_OF_WAREHOUSE_WEAPONS_CONFISCATED:
    return {make_terminal_line(
        PoliceLine::DURING_INSPECTION_OF_WAREHOUSE_WEAPONS_CONFISCATED)};

  case PoliceLine::PLAYER_GOING_TO_JAIL:
  case PoliceLine::PLAYER_PAID_FINE:
  case PoliceLine::POLICE_HUNG_UP:
    return {};

  default:
    std::cerr << "Get Lines not yet implemented for police line "
              << static_cast<int>(current_line_) << "\n";
    break;
  }
  return {};
}

bool PoliceConversation::has_ended() const {
  switch (current_line_) {
  case PoliceLine::IN_VIEW_OF_YOUR_GOOD_STANDING_NOT_TAKE_IT_FURTHER:
  case PoliceLine::SURE_BUT_THIS_WILL_REMAIN_OUR_SECRET:
  case PoliceLine::ALRIGHT_ILL_HAVE_ONE_FOR_80K_AND_YOU_CAN_GO_FREE:
  case PoliceLine::YOU_CANNOT_BRIBE_ME_SENTENCED_DOUBLED:
  case PoliceLine::PLAYER_GOING_TO_JAIL:
  case PoliceLine::PLAYER_PAID_FINE:
  case PoliceLine::POLICE_HUNG_UP:
  case PoliceLine::DURING_INSPECTION_OF_WAREHOUSE_WEAPONS_CONFISCATED:
  case PoliceLine::WHO_ARE_YOU_GET_OUT_OF_HERE_AT_ONCE:
  case PoliceLine::WHAT_DO_YOU_THINK_I_AM_I_FINE_YOU_1K_FOR_INSULTING:
  case PoliceLine::
      IF_YOUR_TRYING_TO_BRIBE_ME_YOUVE_GOT_THE_WRONG_PERSON_2_DAYS_JAIL:
  case PoliceLine::NOT_TOO_GOOD_IM_VERY_BUSY_GOODBYE:
  case PoliceLine::ITS_NOT_THAT_NICE_STOP_BOTHERING_ME:
  case PoliceLine::THANKS_A_LOT_NEXT_MONTH_WE_WILL_EXAMINE_WAREHOUSES_IN:
  case PoliceLine::THANK_YOU_THE_MAYOR_WILL_VISIT_YOUR_RESTAURANT:
  case PoliceLine::YES_THANKS_WHAT_WOULD_I_HAVE_DONE_WITHOUT:
  case PoliceLine::YOURE_OFFERING_ME_MONEY_YOU_DONT_EVEN_HAVE:
    return true;
  default:
    return false;
  }
}

void PoliceConversation::seize_carried_weapons(GameState& state,
                                               Player& player) {
  auto& inv = player.inventory();
  Money seized{0};
  for (auto item : core::item::attack_weapon_list) {
    seized = seized + player.weapon_unit_cost(item) * inv.count(item);
    inv.remove(item, inv.count(item));
  }
  if (seized.cents > 0) {
    player.financials().transfer(state.time().current_date(), Account::Weapons,
                                 Account::ProfitAndLoss, seized,
                                 AccountingScope::Operational);
  }
}

void PoliceConversation::apply_confiscation() {
  auto* player = state_.active_player();

  switch (*arrest_reason_) {
  case ArrestReason::WeaponsDealing:
    sentence_days_ = 8;
    seize_carried_weapons(state_, *player);
    break;
  case ArrestReason::LaunderingMoney:
    player->mafia_state().active_mission = std::nullopt;
    break;
  // The weapons went when the police searched the depot.
  case ArrestReason::WarehouseWeapons:
    sentence_days_ = 20;
    break;
  case ArrestReason::PosessingWeapons:
    sentence_days_ = 4;
    seize_carried_weapons(state_, *player);
    break;
  default:
    break;
  }
}

void PoliceConversation::go_to_jail(int days) {
  days = std::min(days, 99);
  auto* player = state_.active_player();
  player->increment_criminal_record();
  player->jail(days);
  state_.adjust_social_score(*player, -(days * 4));
  state_.adjust_underworld_score(*player, 10);

  // Show prison fullscreen after the conversation view pops.
  // The on_dismiss callback starts the jail day loop.
  on_end_ = [&state = state_] {
    state.deliver_milestone(
        core::events::MilestoneId::YouAreOnTheWayToPrison, [&state] {
          state.turn().request_end_of_turn(EndOfDayCause::PlayerAction);
        });
  };
}
