/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "conversations/RealtorConversation.h"

#include <magic_enum/magic_enum.hpp>

#include "conversations/RealtorLine.h"
#include "core/character/Actor.h"
#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/CityTypes.h"
#include "core/city/Property.h"
#include "core/finance/Ledger.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"

constexpr double renter_discount = 0.70;     // 0x46 / 100
constexpr double rip_off_markup = 2.00;      // esi += esi
constexpr double negotiate_minus = 0.80;     // dbl_79B07
constexpr double negotiate_plus = 1.10;      // dbl_79B0F
constexpr double sell_start_factor = 0.90;   // dbl_79AF7
constexpr double sell_high_threshold = 1.20; // dbl_79AFF

// random notes:
// rent -> rent again -> shop is not for rent
// rent -> buy -> cheaper (900K -> 630K -> 504K)
// 450990 -> +10% -> 496089 -> +10% -> 545K

RealtorConversation::RealtorConversation(
    GameState& state, const Actor* actor,
    const RealtorSettings& realtor_settings)
    : BaseConversation(compute_start_line(state, actor), state, "t/makler.e",
                       actor),
      realtor_settings_(realtor_settings) {}

/* This advances only the lines that don't require user choices */
bool RealtorConversation::advance() {
  std::cout << "advance: " << magic_enum::enum_name(current_line_) << "\n";
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
    current_line(RealtorLine::I_WANT_TO_BUY_THIS_SHOP);
    break;
  case RealtorLine::OK_MAYBE_WELL_FIND_SOMETHING_ELSE_FOR_YOU:
    current_line(RealtorLine::OK_MAYBE_WELL_FIND_SOMETHING_ELSE_FOR_YOU);
    break;
  case RealtorLine::OH_YES_JUST_REMEMBERED_MY_MISTAKE: {
    price_ = list_price_;
    ripoff_ = false;
    current_line(RealtorLine::DO_YOU_WANT_TO_BUY_FOR_PRICE);
    break;
  }
  case RealtorLine::ALRIGHT_AT_THAT_PRICE_I_CAN_DO_FAVOUR: {
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
  std::cout << "advance_line_choice: " << magic_enum::enum_name(current_line_)
            << " => " << magic_enum::enum_name(choice) << "\n";
  auto* property = active_property();
  auto* player = state_.active_player();
  bool owned_by_player =
      property == nullptr || property->owner_or_tenant() == player->id();
  auto charisma = player->character().charisma();
  // Do something with the choice
  switch (choice) {
  case RealtorLine::I_WANT_TO_BUY_THIS_SHOP: {
    Logger::warn("I want to buy this");
    if (owned_by_player && property->is_owned()) {
      current_line(RealtorLine::YOU_ALREADY_OWNER);
    }
    // Todo: deal with when player is renting it,
    // I guess is_for_sale() should return true in that
    // case? So we need a player argument there?
    else if (!property->is_for_sale()) {
      current_line(RealtorLine::SORRY_NOT_FOR_SALE);
    }
    else {
      list_price_ = property->get_purchase_price();
      Logger::warn("{} chance of rip off", realtor_settings_.rip_off_chance);
      if (engine::random::rand_percentage_chance(
              (realtor_settings_.rip_off_chance))) {
        ripoff_ = true;
        price_ = list_price_ * rip_off_markup;
      }
      else {
        price_ = list_price_;
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
    price_ = price_ * negotiate_minus;
    current_line(RealtorLine::ALRIGHT_AT_THAT_PRICE_I_CAN_DO_FAVOUR);
    break;
  case RealtorLine::PLACE_WAS_PRICED_LESS:
    current_line(ripoff_ ? RealtorLine::OH_YES_JUST_REMEMBERED_MY_MISTAKE
                         : RealtorLine::THATS_NOT_TRUE_IM_NO_CHEAT_GET_OUT);
    break;
  case RealtorLine::I_WANT_TO_RENT_THIS_SHOP: {
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
  std::cout << "advance_proposal: " << magic_enum::enum_name(current_line_)
            << ": " << (agree ? 1 : 0) << "\n";
  switch (current_line_) {
  case RealtorLine::YOU_WANT_TO_RENT_THESE_ROOMS_MONTHLY_RENT_IS:
    if (agree) {
      auto* property = active_property();
      auto type = property->type();
      if (type == PropertyType::BUSINESS_PREMISE) {
        if (state_.rent_restaurant(*property, *state_.active_player())) {
          current_line(RealtorLine::YOU_HAVE_RENTED_THIS_SHOP_GOOD_LUCK);
        }
        else {
          current_line(RealtorLine::YOU_HAVENT_GOT_ENOUGH_MONEY_GET_OUT);
        }
      }
      else if (type == PropertyType::DEPOT) {
        if (state_.rent_depot(*property, *state_.active_player())) {
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
        state_.buy_restaurant(*active_property(), *state_.active_player());
        current_line(RealtorLine::SHOP_IS_NOW_YOURS_SEE_US_AGAIN);
      }
      else if (type == PropertyType::DEPOT) {
        if (state_.buy_depot(*active_property(), *state_.active_player())) {
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
        state_.refuse_calls_from(actor_->id());
      }
      else {
        current_line(RealtorLine::IVE_HAD_SECOND_THOUGHTS);
      }
    }
    break;
  case RealtorLine::ARE_YOU_SURE:
    if (agree) {
      current_line(RealtorLine::OK);
      if (active_property()->has_restaurant()) {
        Money furniture_amount;
        Money oven_amount;
        auto* restaurant =
            state_.restaurant(*active_property()->restaurant_id());
        const auto& furniture = restaurant->layout()->placed_furniture();
        const auto& kitchen = restaurant->layout()->placed_kitchen_furniture();
        for (const auto& item : furniture) {
          furniture_amount += item->furniture()->price() / 2;
        }
        for (const auto& item : kitchen) {
          oven_amount += item->furniture()->price() / 2;
        }

        if (furniture_amount > 0_usd || oven_amount > 0_usd) {
          state_.status().post({
              .payload = compat::format(
                  "Articles to a total of {:w}, were left in your restaurant. "
                  "The scrapdealer is happy now!!",
                  furniture_amount + oven_amount),
              .scope = StatusScope::Global,
              .type = TransientType::Ephemeral,
              .ttl = std::chrono::milliseconds{6000},
              .coalesce_key = "happy_scrapdealer",
          });

          auto owner = active_property()->owner_or_tenant();
          auto* player = state_.player(*owner);
          auto date = state_.time().current_date();
          if (furniture_amount > 0_usd) {
            player->financials().transfer(
                date, Account::Furniture, Account::ProfitAndLoss,
                furniture_amount, AccountingScope::Operational);
          }
          if (oven_amount > 0_usd) {
            player->financials().transfer(date, Account::Machines,
                                          Account::ProfitAndLoss, oven_amount,
                                          AccountingScope::Operational);
          }
        }
      }
      active_property()->clear_owner();
      active_property()->status(PremiseStatus::NOT_AVAILABLE);
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
    return Emotion::HAPPY;
  case RealtorLine::THATS_NOT_TRUE_IM_NO_CHEAT_GET_OUT:
    return Emotion::ANGRY;
  default:
    return Emotion::NEUTRAL;
  }
}

std::vector<ConversationLine<RealtorLine>> RealtorConversation::lines() const {
  const auto property = active_property();
  // std::cout << "lines: " << magic_enum::enum_name(current_line_) << "\n";
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
        make_line(RealtorLine::ARE_YOU_SURE),
    };
  case RealtorLine::YOU_WANT_TO_RENT_THESE_ROOMS_MONTHLY_RENT_IS: {
    return {
        make_proposal(
            RealtorLine::YOU_WANT_TO_RENT_THESE_ROOMS_MONTHLY_RENT_IS,
            compat::format(" {}", active_property()->get_monthly_rent())),
    };
  }
  case RealtorLine::YOU_HAVE_TO_SELECT_A_PLACE:
    return {
        make_line(RealtorLine::YOU_HAVE_TO_SELECT_A_PLACE),
    };
  case RealtorLine::FOR_THIS_SHOP_ILL_PAY_YOU:
    return {
        make_line(RealtorLine::FOR_THIS_SHOP_ILL_PAY_YOU),
    };
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
        make_line(RealtorLine::I_CANT_OFFER_YOU_MORE),
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
        make_line(RealtorLine::I_WANT_10_PCT_MORE),
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
  case RealtorLine::I_DONT_DEAL_WITH_THIS_PLACE:
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

Property* RealtorConversation::active_property() const {
  auto active_index = state_.last_clicked_property_index();
  if (!active_index) {
    return nullptr;
  }
  auto* city = state_.active_city();
  return city->property_by_index(*active_index);
}
