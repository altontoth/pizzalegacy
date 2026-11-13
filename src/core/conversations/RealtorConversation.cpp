/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/conversations/RealtorConversation.h"

#include <algorithm>
#include <magic_enum/magic_enum.hpp>

#include "core/character/Actor.h"
#include "core/character/Player.h"
#include "core/character/SocialStatus.h"
#include "core/city/City.h"
#include "core/city/CityTypes.h"
#include "core/city/Property.h"
#include "core/conversations/RealtorLine.h"
#include "core/finance/Money.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameOptions.h"
#include "engine/FormatCompat.h"
#include "engine/Random.h"

constexpr double renter_discount = 0.70;
constexpr double rip_off_markup = 2.00;
constexpr double negotiate_minus = 0.80;
constexpr double negotiate_plus = 1.10;
constexpr double sell_start_factor = 0.90;
constexpr double sell_high_threshold = 1.20;
constexpr double sell_fail_factor = 0.80;
// Opening offer is purchase price minus a random amount.
constexpr Money sell_opening_cut_step = 10_usd;
constexpr int sell_opening_cut_range = 500;
constexpr int sell_usable_haggles = 2;
constexpr int personality_haggle_penalty = 30;
constexpr int personality_haggle_option = 4;
constexpr int haggle_base_chance = 70;
constexpr int haggle_chance_step = 20;

RealtorConversation::RealtorConversation(
    GameState& state, const Actor* actor,
    const RealtorSettings& realtor_settings)
    : BaseConversation(compute_start_line(state, actor), state, "t/makler.e",
                       actor),
      ripoff_(engine::random::rand_percentage_chance(
          realtor_settings.rip_off_chance)) {}

/* This advances only the lines that don't require user choices */
bool RealtorConversation::advance() {
  switch (current_line_) {
  case RealtorLine::REALTORS_MAY_I_HELP_YOU:
  case RealtorLine::SORRY_NOT_FOR_SALE:
  case RealtorLine::YOU_ALREADY_OWNER:
  case RealtorLine::THIS_SHOP_IS_NOT_FOR_RENT:
  case RealtorLine::SHOP_IS_NOW_YOURS_SEE_US_AGAIN:
  case RealtorLine::YOU_HAVE_RENTED_THIS_SHOP_GOOD_LUCK:
  case RealtorLine::SOMETHING_SUITABLE_FOR_YOU:
  case RealtorLine::WAREHOUSE_NOW_BELONGS_TO_YOU:
  case RealtorLine::YOU_HAVE_RENTED_THIS_WAREHOUSE:
  case RealtorLine::I_HOPE_NOT_LAST_TIME_WE_DO_BUSINESS:
  case RealtorLine::OK:
    current_line(RealtorLine::I_WANT_TO_BUY_THIS_SHOP);
    break;
  case RealtorLine::OK_MAYBE_WELL_FIND_SOMETHING_ELSE_FOR_YOU:
    current_line(RealtorLine::OK_MAYBE_WELL_FIND_SOMETHING_ELSE_FOR_YOU);
    break;
  case RealtorLine::OH_YES_JUST_REMEMBERED_MY_MISTAKE: {
    price_ = active_property()->purchase_price();
    current_line(RealtorLine::DO_YOU_WANT_TO_BUY_FOR_PRICE);
    break;
  }
  case RealtorLine::ALRIGHT_AT_THAT_PRICE_I_CAN_DO_FAVOUR:
  case RealtorLine::SORRY_BUT_AT_THAT_PRICE_NO_FAVOR: {
    current_line(RealtorLine::DO_YOU_WANT_TO_BUY_FOR_PRICE);
    break;
  }
  default:
    std::cerr << "I don't know how to advance without choice from state "
              << magic_enum::enum_name(current_line_) << "\n";
    return false;
    break;
  }
  return true;
}

bool RealtorConversation::advance_line_choice(RealtorLine choice) {
  auto* property = active_property();
  auto* player = state_.active_player();
  bool owned_by_player =
      property == nullptr || property->owner_or_tenant() == player->id();
  auto charisma = player->character().charisma();

  switch (choice) {
  case RealtorLine::I_WANT_TO_BUY_THIS_SHOP: {
    if (!welcome_here()) {
      current_line(RealtorLine::WE_CANNOT_ALLOW_A_LOW_LIFE_IN_OUR_CITY);
      break;
    }
    if (owned_by_player && property->is_owned()) {
      current_line(RealtorLine::YOU_ALREADY_OWNER);
    }
    else if (!property->is_for_sale() &&
             !(property->is_rented() &&
               property->owner_or_tenant() == player->id())) {
      current_line(RealtorLine::SORRY_NOT_FOR_SALE);
    }
    else {
      Money base = property->purchase_price();
      // If you're renting the place you get a discount and they never try to
      // cheat you.
      if (property->is_rented() &&
          property->owner_or_tenant() == player->id()) {
        price_ = Money(static_cast<int64_t>(base.cents * renter_discount));
      }
      else if (ripoff_) {
        price_ = Money(static_cast<int64_t>(base.cents * rip_off_markup));
      }
      else {
        price_ = base;
      }
      current_line(RealtorLine::DO_YOU_WANT_TO_BUY_FOR_PRICE);
    }
    break;
  }
  case RealtorLine::IVE_HAD_SECOND_THOUGHTS:
    current_line(RealtorLine::OK_MAYBE_WELL_FIND_SOMETHING_ELSE_FOR_YOU);
    break;
  case RealtorLine::PRICE_IS_TOO_HIGH:
    buyer_haggled_already = true;
    if (engine::random::rand_percentage_chance(charisma)) {
      price_ = Money(static_cast<int64_t>(price_.cents * negotiate_minus));
      current_line(RealtorLine::ALRIGHT_AT_THAT_PRICE_I_CAN_DO_FAVOUR);
    }
    else {
      price_ = Money(static_cast<int64_t>(price_.cents * negotiate_plus));
      // The original stores the quote, tenant discount or rip-off included.
      if (state_.rules().fix_legacy_bugs) {
        property->purchase_price(Money(static_cast<int64_t>(
            property->purchase_price().cents * negotiate_plus)));
      }
      else {
        property->purchase_price(price_);
      }
      current_line(RealtorLine::SORRY_BUT_AT_THAT_PRICE_NO_FAVOR);
    }
    break;
  case RealtorLine::PLACE_WAS_PRICED_LESS:
    if (ripoff_) {
      current_line(RealtorLine::OH_YES_JUST_REMEMBERED_MY_MISTAKE);
    }
    else {
      current_line(RealtorLine::THATS_NOT_TRUE_IM_NO_CHEAT_GET_OUT);
      state_.active_city()->refuse_calls_from(actor_->id());
    }
    break;
  case RealtorLine::I_WANT_TO_RENT_THIS_SHOP: {
    if (!welcome_here()) {
      current_line(RealtorLine::WE_CANNOT_ALLOW_A_LOW_LIFE_IN_OUR_CITY);
      break;
    }
    if (!property->is_for_rent()) {
      current_line(RealtorLine::THIS_SHOP_IS_NOT_FOR_RENT);
    }
    else {
      current_line(RealtorLine::YOU_WANT_TO_RENT_THESE_ROOMS_MONTHLY_RENT_IS);
    }
    break;
  }
  case RealtorLine::I_WANT_TO_CANCEL_THE_LEASE:
    if (property->is_rented() && property->owner_or_tenant() == player->id()) {
      current_line(RealtorLine::ARE_YOU_SURE);
    }
    else {
      current_line(RealtorLine::YOU_ARE_NOT_THE_TENANT_OF_SHOP);
    }
    break;
  case RealtorLine::I_WANT_TO_SELL_THIS_SHOP: {
    if (!property->is_owned() || property->owner_or_tenant() != player->id()) {
      current_line(RealtorLine::YOU_ARE_NOT_THE_OWNER_OF_THIS_SHOP);
    }
    else {
      auto value = property->purchase_price();
      Money cut = sell_opening_cut_step *
                  engine::random::rand_max_n(sell_opening_cut_range);
      sell_offer_ =
          Money(static_cast<int64_t>(value.cents * sell_start_factor)) - cut;
      sell_haggle_count_ = 0;
      current_line(RealtorLine::I_WOULD_GIVE_YOU);
    }
    break;
  }
  case RealtorLine::I_WANT_10_PCT_MORE:
    haggle_sale(1);
    break;
  case RealtorLine::I_WANT_20_PCT_MORE:
    haggle_sale(2);
    break;
  case RealtorLine::THIS_PLACE_IS_WORTH_DOUBLE:
    haggle_sale(3);
    break;
  case RealtorLine::NOT_ON_THOSE_CONDITIONS:
    haggle_sale(4);
    break;
  default:
    std::cerr << "I don't know how to advance with choice " << choice
              << " from state " << magic_enum::enum_name(current_line_)
              << std::endl;
    return false;
    break;
  }
  return true;
}

bool RealtorConversation::advance_proposal(bool agree) {
  switch (current_line_) {
  case RealtorLine::YOU_WANT_TO_RENT_THESE_ROOMS_MONTHLY_RENT_IS:
    if (agree) {
      auto* property = active_property();
      auto type = property->type();
      if (type == PropertyType::BUSINESS_PREMISE) {
        if (state_.real_estate().rent_restaurant(*property,
                                                 *state_.active_player())) {
          current_line(RealtorLine::YOU_HAVE_RENTED_THIS_SHOP_GOOD_LUCK);
        }
        else {
          current_line(RealtorLine::YOU_HAVENT_GOT_ENOUGH_MONEY_GET_OUT);
        }
      }
      else if (type == PropertyType::DEPOT) {
        if (state_.real_estate().rent_depot(*property,
                                            *state_.active_player())) {
          current_line(RealtorLine::YOU_HAVE_RENTED_THIS_WAREHOUSE);
        }
        else {
          current_line(RealtorLine::YOU_HAVENT_GOT_ENOUGH_MONEY_GET_OUT);
        }
      }
    }
    else {
      current_line(RealtorLine::I_WANT_TO_BUY_THIS_SHOP);
    }
    break;
  case RealtorLine::DO_YOU_WANT_TO_BUY_FOR_PRICE:
    if (agree) {
      auto* property = active_property();
      auto type = property->type();
      if (type == PropertyType::BUSINESS_PREMISE) {
        if (state_.real_estate().buy_restaurant(
                *property, *state_.active_player(), price_)) {
          property->purchase_price(price_);
          current_line(RealtorLine::SHOP_IS_NOW_YOURS_SEE_US_AGAIN);
        }
        else {
          current_line(RealtorLine::YOU_HAVENT_GOT_ENOUGH_MONEY_GET_OUT);
        }
      }
      else if (type == PropertyType::DEPOT) {
        if (state_.real_estate().buy_depot(*property, *state_.active_player(),
                                           price_)) {
          property->purchase_price(price_);
          current_line(RealtorLine::WAREHOUSE_NOW_BELONGS_TO_YOU);
        }
        else {
          current_line(RealtorLine::YOU_HAVENT_GOT_ENOUGH_MONEY_GET_OUT);
        }
      }
    }
    else {
      if (buyer_haggled_already) {
        current_line(RealtorLine::OUTRAGEOUS_STOP_HAGGLING);
        state_.active_city()->refuse_calls_from(actor_->id());
      }
      else {
        current_line(RealtorLine::IVE_HAD_SECOND_THOUGHTS);
      }
    }
    break;
  case RealtorLine::I_WOULD_GIVE_YOU:
    if (agree) {
      complete_sale();
    }
    else {
      current_line(RealtorLine::I_WANT_10_PCT_MORE);
    }
    break;
  case RealtorLine::I_CANT_OFFER_YOU_MORE:
    if (agree) {
      complete_sale();
    }
    else {
      current_line(RealtorLine::OK_MAYBE_WELL_FIND_SOMETHING_ELSE_FOR_YOU);
    }
    break;
  case RealtorLine::ARE_YOU_SURE:
    if (agree) {
      current_line(RealtorLine::OK);
      auto* player = state_.active_player();
      if (!active_property()->has_restaurant()) { // depot
        state_.adjust_social_score(*player, -170);
      }
      state_.real_estate().end_property_lease(*active_property(), *player);
    }
    else {
      current_line(RealtorLine::I_WANT_TO_BUY_THIS_SHOP);
    }
    break;
  default:
    // If there is no choice then just advance.
    return advance();
    break;
  }
  return true;
}

Emotion RealtorConversation::emotion() const {
  switch (current_line_) {
  case RealtorLine::SHOP_IS_NOW_YOURS_SEE_US_AGAIN:
  case RealtorLine::OH_YES_JUST_REMEMBERED_MY_MISTAKE:
  case RealtorLine::I_HOPE_NOT_LAST_TIME_WE_DO_BUSINESS:
    return Emotion::HAPPY;
  case RealtorLine::THATS_NOT_TRUE_IM_NO_CHEAT_GET_OUT:
  case RealtorLine::NO_IVE_HAD_IT_WITH_YOU:
    return Emotion::ANGRY;
  default:
    return Emotion::NEUTRAL;
  }
}

std::vector<ConversationLine<RealtorLine>> RealtorConversation::lines() const {
  switch (current_line_) {
  case RealtorLine::REALTORS_MAY_I_HELP_YOU: {
    std::string custom_line =
        compat::format("Realtors {}. How may I help you?", actor_->name());
    return {
        make_text_line(RealtorLine::REALTORS_MAY_I_HELP_YOU, custom_line),
    };
  }
  case RealtorLine::OK:
    return {
        make_line(RealtorLine::OK),
    };
  case RealtorLine::SOMETHING_SUITABLE_FOR_YOU:
    return {
        make_line(RealtorLine::SOMETHING_SUITABLE_FOR_YOU),
    };
  case RealtorLine::SORRY_NOT_FOR_SALE:
    return {
        make_line(RealtorLine::SORRY_NOT_FOR_SALE),
    };
  case RealtorLine::DO_YOU_WANT_TO_BUY_FOR_PRICE: {
    return {
        make_proposal(RealtorLine::DO_YOU_WANT_TO_BUY_FOR_PRICE,
                      compat::format(" {}", price_)),
    };
  }
  case RealtorLine::I_DONT_DEAL_WITH_THIS_PLACE:
    return {
        make_line(RealtorLine::I_DONT_DEAL_WITH_THIS_PLACE),
    };
  case RealtorLine::YOU_HAVE_RENTED_THIS_WAREHOUSE:
    return {
        make_line(RealtorLine::YOU_HAVE_RENTED_THIS_WAREHOUSE),
    };
  case RealtorLine::SO_YOU_WANT_TO_BUY_AFTER_ALL_PRICE_IS:
    return {
        make_line(RealtorLine::SO_YOU_WANT_TO_BUY_AFTER_ALL_PRICE_IS),
    };
  case RealtorLine::YOU_WERE_IMPRESSED_EH:
    return {
        make_line(RealtorLine::YOU_WERE_IMPRESSED_EH),
    };
  case RealtorLine::THIS_SHOP_IS_NOT_FOR_RENT:
    return {
        make_line(RealtorLine::THIS_SHOP_IS_NOT_FOR_RENT),
    };
  case RealtorLine::YOU_ARE_NOT_THE_TENANT_OF_SHOP:
    return {
        make_line(RealtorLine::YOU_ARE_NOT_THE_TENANT_OF_SHOP),
    };
  case RealtorLine::WHAT_CAN_I_DO_FOR_YOU:
    return {
        make_line(RealtorLine::WHAT_CAN_I_DO_FOR_YOU),
    };
  case RealtorLine::ARE_YOU_SURE:
    return {
        make_proposal(RealtorLine::ARE_YOU_SURE),
    };
  case RealtorLine::YOU_WANT_TO_RENT_THESE_ROOMS_MONTHLY_RENT_IS: {
    return {
        make_line(
            RealtorLine::YOU_WANT_TO_RENT_THESE_ROOMS_MONTHLY_RENT_IS,
            {
                .is_proposal = true,
                .easy_accept = false,
                .custom_text = compat::format(
                    " {}", active_property()->get_monthly_rent())
            }),
    };
  }
  case RealtorLine::YOU_HAVE_TO_SELECT_A_PLACE:
    return {
        make_line(RealtorLine::YOU_HAVE_TO_SELECT_A_PLACE),
    };
  case RealtorLine::I_WOULD_GIVE_YOU: {
    auto text = compat::format("I would give you {} for it.", sell_offer_);
    return {
        make_text_line(RealtorLine::I_WOULD_GIVE_YOU, text),
    };
  }
  case RealtorLine::YOU_ARE_NOT_THE_OWNER_OF_THIS_SHOP:
    return {
        make_line(RealtorLine::YOU_ARE_NOT_THE_OWNER_OF_THIS_SHOP),
    };
  case RealtorLine::SHOP_INCLUDE_AT_NEXT_AUCTION:
    return {
        make_line(RealtorLine::SHOP_INCLUDE_AT_NEXT_AUCTION),
    };
  case RealtorLine::RATHER_PUT_IT_UP_AUCTION_AS_YOU_WISH:
    return {
        make_line(RealtorLine::RATHER_PUT_IT_UP_AUCTION_AS_YOU_WISH),
    };
  case RealtorLine::HOW_ABOUT_THIS_ONE_FOR_ONLY:
    return {
        make_line(RealtorLine::HOW_ABOUT_THIS_ONE_FOR_ONLY),
    };
  case RealtorLine::WAREHOUSE_NOW_BELONGS_TO_YOU:
    return {
        make_line(RealtorLine::WAREHOUSE_NOW_BELONGS_TO_YOU),
    };
  case RealtorLine::I_CANT_OFFER_YOU_MORE:
    return {
        make_proposal(RealtorLine::I_CANT_OFFER_YOU_MORE),
    };
  case RealtorLine::YOU_PLANT_AUCTION_RATHER_SELL_TO_ME:
    return {
        make_line(RealtorLine::YOU_PLANT_AUCTION_RATHER_SELL_TO_ME),
    };
  case RealtorLine::YOU_HAVE_RENTED_THIS_SHOP_GOOD_LUCK:
    return {
        make_line(RealtorLine::YOU_HAVE_RENTED_THIS_SHOP_GOOD_LUCK),
    };
  case RealtorLine::SHOP_IS_NOW_YOURS_SEE_US_AGAIN:
    return {
        make_line(RealtorLine::SHOP_IS_NOW_YOURS_SEE_US_AGAIN),
    };
  case RealtorLine::YOU_HAVENT_GOT_ENOUGH_MONEY_GET_OUT:
    return {
        make_line(RealtorLine::YOU_HAVENT_GOT_ENOUGH_MONEY_GET_OUT),
    };
  case RealtorLine::I_HOPE_NOT_LAST_TIME_WE_DO_BUSINESS:
    return {
        make_line(RealtorLine::I_HOPE_NOT_LAST_TIME_WE_DO_BUSINESS),
    };
  case RealtorLine::I_HOPE_YOU_DONT_REGRET_GIVING_UP_LOCATION:
    return {
        make_line(RealtorLine::I_HOPE_YOU_DONT_REGRET_GIVING_UP_LOCATION),
    };
  case RealtorLine::OK_THE_SHOP_WOULD_BE:
    return {
        make_line(RealtorLine::OK_THE_SHOP_WOULD_BE),
    };
  case RealtorLine::I_WANT_TO_BUY_THIS_SHOP:
    return {
        make_option(RealtorLine::I_WANT_TO_BUY_THIS_SHOP),
        make_option(RealtorLine::I_WANT_TO_SELL_THIS_SHOP),
        make_option(RealtorLine::I_WANT_TO_RENT_THIS_SHOP),
        make_option(RealtorLine::I_WANT_TO_CANCEL_THE_LEASE),
    };
  case RealtorLine::I_WANT_TO_PUT_UP_FOR_AUCTION:
    return {
        make_line(RealtorLine::I_WANT_TO_PUT_UP_FOR_AUCTION),
    };
  case RealtorLine::PRICE_IS_TOO_HIGH:
    return {
        make_line(RealtorLine::PRICE_IS_TOO_HIGH),
    };
  case RealtorLine::LOCATION_IS_POOR:
    return {
        make_line(RealtorLine::LOCATION_IS_POOR),
    };
  case RealtorLine::IVE_HAD_SECOND_THOUGHTS:
    return {
        make_option(RealtorLine::IVE_HAD_SECOND_THOUGHTS),
        make_option(RealtorLine::PRICE_IS_TOO_HIGH),
        make_option(RealtorLine::PLACE_WAS_PRICED_LESS),
    };
  case RealtorLine::OH_YES_JUST_REMEMBERED_MY_MISTAKE:
    return {
        make_line(RealtorLine::OH_YES_JUST_REMEMBERED_MY_MISTAKE),
    };
  case RealtorLine::THATS_NOT_TRUE_IM_NO_CHEAT_GET_OUT:
    return {
        make_line(RealtorLine::THATS_NOT_TRUE_IM_NO_CHEAT_GET_OUT),
    };
  case RealtorLine::OK_MAYBE_WELL_FIND_SOMETHING_ELSE_FOR_YOU:
    return {
        make_line(RealtorLine::OK_MAYBE_WELL_FIND_SOMETHING_ELSE_FOR_YOU),
    };
  case RealtorLine::ALRIGHT_AT_THAT_PRICE_I_CAN_DO_FAVOUR:
    return {
        make_line(RealtorLine::ALRIGHT_AT_THAT_PRICE_I_CAN_DO_FAVOUR),
    };
  case RealtorLine::SORRY_BUT_AT_THAT_PRICE_NO_FAVOR:
    return {
        make_line(RealtorLine::SORRY_BUT_AT_THAT_PRICE_NO_FAVOR),
    };
  case RealtorLine::OUTRAGEOUS_STOP_HAGGLING:
    return {
        make_line(RealtorLine::OUTRAGEOUS_STOP_HAGGLING),
    };
  case RealtorLine::YOU_ALREADY_OWNER:
    return {
        make_line(RealtorLine::YOU_ALREADY_OWNER),
    };
  case RealtorLine::YOU_CANT_AFFORT_FIRST_MONTH_RENT:
    return {
        make_line(RealtorLine::YOU_CANT_AFFORT_FIRST_MONTH_RENT),
    };
  case RealtorLine::I_WANT_10_PCT_MORE:
    return {
        make_option(RealtorLine::I_WANT_10_PCT_MORE),
        make_option(RealtorLine::I_WANT_20_PCT_MORE),
        make_option(RealtorLine::THIS_PLACE_IS_WORTH_DOUBLE),
        make_option(RealtorLine::NOT_ON_THOSE_CONDITIONS),
    };
  case RealtorLine::I_WANT_20_PCT_MORE:
    return {
        make_line(RealtorLine::I_WANT_20_PCT_MORE),
    };
  case RealtorLine::THIS_PLACE_IS_WORTH_DOUBLE:
    return {
        make_line(RealtorLine::THIS_PLACE_IS_WORTH_DOUBLE),
    };
  case RealtorLine::NOT_ON_THOSE_CONDITIONS:
    return {
        make_line(RealtorLine::NOT_ON_THOSE_CONDITIONS),
    };
  case RealtorLine::NO_IVE_HAD_IT_WITH_YOU:
    return {
        make_line(RealtorLine::NO_IVE_HAD_IT_WITH_YOU),
    };
  case RealtorLine::WE_CANNOT_ALLOW_A_LOW_LIFE_IN_OUR_CITY:
    return {
        make_line(RealtorLine::WE_CANNOT_ALLOW_A_LOW_LIFE_IN_OUR_CITY),
    };
  default:
    std::cerr << "Get Lines Not yet implemented for line "
              << lines_.at(static_cast<int>(current_line_)) << std::endl;
    break;
  }
  return {};
}

bool RealtorConversation::has_ended() const {
  switch (current_line_) {
  case RealtorLine::YOU_HAVENT_GOT_ENOUGH_MONEY_GET_OUT:
  case RealtorLine::OUTRAGEOUS_STOP_HAGGLING:
  case RealtorLine::THATS_NOT_TRUE_IM_NO_CHEAT_GET_OUT:
  case RealtorLine::NO_IVE_HAD_IT_WITH_YOU:
  case RealtorLine::I_DONT_DEAL_WITH_THIS_PLACE:
  case RealtorLine::WE_CANNOT_ALLOW_A_LOW_LIFE_IN_OUR_CITY:
    return true;
  default:
    return false;
  }
}

RealtorLine RealtorConversation::compute_start_line(const GameState& state,
                                                    const Actor* actor) {
  auto active_index = state.last_clicked_property_index();
  if (!active_index) {
    return RealtorLine::YOU_HAVE_TO_SELECT_A_PLACE;
  }
  auto* city = state.active_city();
  auto* property = city->property_by_index(*active_index);
  if (property != nullptr && property->realtor_id() != actor->id()) {
    return RealtorLine::I_DONT_DEAL_WITH_THIS_PLACE;
  }
  return RealtorLine::REALTORS_MAY_I_HELP_YOU;
}

void RealtorConversation::complete_sale() {
  auto* property = active_property();
  state_.real_estate().sell_property(*property, *state_.active_player(),
                                     sell_offer_);
  // The agreed price becomes the premise's value.
  property->purchase_price(sell_offer_);
  current_line(RealtorLine::I_HOPE_NOT_LAST_TIME_WE_DO_BUSINESS);
}

void RealtorConversation::haggle_sale(int option) {
  // Option 4 asks for the biggest raise and is the only one whose chance comes
  // from personality. For the rest, the more you ask for the less likely they
  // are to agree.
  int chance = 0;
  if (option == personality_haggle_option) {
    int personality = state_.active_player()->character().personality();
    chance = std::clamp(personality - personality_haggle_penalty, 0, 100);
  }
  else {
    chance = haggle_base_chance - haggle_chance_step * option;
  }

  if (engine::random::rand_percentage_chance(chance)) {
    sell_offer_ = Money{sell_offer_.cents * (option + 10) / 10};
  }
  else {
    sell_offer_ =
        Money(static_cast<int64_t>(sell_offer_.cents * sell_fail_factor));
  }

  sell_haggle_count_++;
  if (sell_haggle_count_ > sell_usable_haggles) {
    current_line(RealtorLine::NO_IVE_HAD_IT_WITH_YOU);
    state_.active_city()->refuse_calls_from(actor_->id());
    return;
  }

  // Past the max the realtor stops offering, but the raise stands and the
  // player can still take it.
  auto max_offer = Money(static_cast<int64_t>(
      active_property()->purchase_price().cents * sell_high_threshold));
  if (sell_offer_ > max_offer) {
    current_line(RealtorLine::I_CANT_OFFER_YOU_MORE);
    return;
  }

  current_line(RealtorLine::I_WOULD_GIVE_YOU);
}

bool RealtorConversation::welcome_here() const {
  const auto* player = state_.active_player();
  return social_rank(player->social_status().social_score(),
                     state_.difficulty_level()) > 0;
}

Property* RealtorConversation::active_property() const {
  auto active_index = state_.last_clicked_property_index();
  if (!active_index) {
    return nullptr;
  }
  auto* city = state_.active_city();
  return city->property_by_index(*active_index);
}
