/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "conversations/MafiaConversation.h"

#include <algorithm>
#include <cstddef>
#include <magic_enum/magic_enum.hpp>

#include "conversations/MafiaLine.h"
#include "core/GameTypes.h"
#include "core/character/Actor.h"
#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/CityNames.h"
#include "core/city/District.h"
#include "core/city/Property.h"
#include "core/common/Ids.h"
#include "core/finance/Ledger.h"
#include "core/item/Item.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/UIState.h"
#include "core/util/Constants.h"
#include "engine/Fatal.h"
#include "engine/FormatCompat.h"
#include "engine/Random.h"

using engine::random::coin_flip;
using engine::random::rand_between;
using engine::random::rand_max_n;
using engine::random::rand_percentage_chance;

namespace {

std::string format_datetime(const GameDate& date, int hour, int minute) {
  return compat::format("on {} {} at {}:{:02}", date.month_name(),
                        date.day_with_suffix(), hour, minute);
}

std::string format_datetime_before(const GameDate& date, int hour, int minute) {
  return compat::format("before or on {} {} at {}:{:02}", date.month_name(),
                        date.day_with_suffix(), hour, minute);
}

struct BuildingPick {
  int tile_index = -1;
  std::string description;
};

BuildingPick pick_random_building(const City* city,
                                  std::optional<int> exclude = std::nullopt) {
  if (!city) {
    fatal("No city in pick_random_building");
  }
  const auto& nb = city->named_building_entries();

  auto it = nb.begin();
  for (int attempts = 0; attempts < 10; attempts++) {
    it = nb.begin();
    std::advance(it, rand_max_n(static_cast<int>(nb.size())));
    if (!exclude || it->first != *exclude) {
      break;
    }
  }

  int idx = it->first;
  auto bname = city->building_name(idx);
  auto dist_name = city->district(city->district_by_index(idx)).name();

  return {.tile_index = idx,
          .description = compat::format("{} in {}", bname, dist_name)};
}

} // anonymous namespace

MafiaConversation::MafiaConversation(
    GameState& state, const Actor* actor, MafiaMode mm,
    std::optional<core::RestaurantId> restaurant_id, bool first_contact)
    : BaseConversation(
          compute_start_line(state, actor, mm, restaurant_id, first_contact),
          state, "t/mafia.e", actor),
      restaurant_id_(restaurant_id) {

  auto* player = state.active_player();
  auto* city = state.active_city();

  int difficulty = state.difficulty_level();
  int uw_score = player->social_status().underworld_score();
  clan_danger_ = city->mafia_settings(actor->id()).crime_danger_level;

  // Record which clan this is so a mission accepted here reports back with the
  // matching face.
  auto mafia_actors = city->actors(ActorType::MAFIA);
  for (std::size_t i = 0; i < mafia_actors.size(); ++i) {
    if (mafia_actors[i]->id() == actor->id()) {
      clan_ = static_cast<int>(i);
      break;
    }
  }

  if (mm == MafiaMode::ProtectionRacketOffer && restaurant_id_) {
    auto* restaurant = state.restaurant(*restaurant_id_);
    if (!restaurant) {
      fatal("Expect restaurant and property");
    }
    // first_contact is passed from the caller since the protection_timer
    // is set before the conversation starts to prevent re-triggering.
    auto player_cash = player->cash_balance();
    // Wealth multiplier: $5K or less pays base price, scaling up to 6x.
    double multiplier = std::clamp(static_cast<double>(player_cash) /
                                       static_cast<double>(5000_usd),
                                   1.0, 6.0);
    int base = (rand_max_n(300) + 1500 - uw_score) * (clan_danger_ + 60) / 100;
    int amount = static_cast<int>(base * multiplier);
    amount = amount * (difficulty + 1) / 2;
    if (first_contact) {
      amount /= 2;
    }
    protection_price_ = std::max(amount, 100);
  }
  else {
    // Mafia contacting you does not count, only when you call them do you get
    // this:
    state.record_milestone(
        core::events::MilestoneId::MakesFirstContactWithMafia, player->id());
  }

  apply_npc_effects(state, mm);
}

void MafiaConversation::apply_npc_effects(GameState& state, MafiaMode mm) {
  auto* player = state.active_player();
  const auto& date = state.time().current_date();

  switch (mm) {
  case MafiaMode::MissionDeadlineReached: {
    auto& mission = player->mafia_state().active_mission;
    if (mission && mission->completed) {
      if (mission->reward.cents > 0) {
        player->financials().spend_cash(date, Account::Missions,
                                        -mission->reward);
      }
      state.adjust_underworld_score(*player, 20);
      auto mission_type = mission->mission_type;
      player->mafia_state().active_mission = std::nullopt;

      on_end_ = [this, mission_type]() {
        switch (mission_type) {
        case MafiaMissionType::Weapon:
          state_.deliver_milestone(
              core::events::MilestoneId::CompletedFirstArmsDeal);
          break;
        case MafiaMissionType::Laundering:
          state_.deliver_milestone(
              core::events::MilestoneId::SuccessfullyLaunderedMoney);
          break;
        case MafiaMissionType::Investment:
          state_.deliver_milestone(
              core::events::MilestoneId::FirstExperienceShadyShareDealing);
          break;
        case MafiaMissionType::Package:
          state_.deliver_milestone(
              core::events::MilestoneId::CompletedFirstCourierService);
          break;
        default:
          break;
        }
      };
    }
    break;
  }
  case MafiaMode::MissionOverdue: {
    auto& mission = player->mafia_state().active_mission;
    if (!mission) {
      break;
    }
    if (mission->mission_type == MafiaMissionType::Weapon) {
      player->mafia_state().active_mission = std::nullopt;
      game_over_pending_ = true;
    }
    else {
      auto& ch = player->character();
      ch.health(ch.health() / 2);
      ch.energy(ch.energy() / 2);
      state.adjust_underworld_score(*player, -30);
      player->mafia_state().active_mission = std::nullopt;
      star_message_pending_ = true;
    }
    break;
  }
  case MafiaMode::LoanRepaymentDue: {
    auto& loan = player->mafia_state().loan;
    if (loan && player->cash_balance() >= loan->amount) {
      player->financials().spend_cash(date, Account::Repayment, loan->amount);
      player->mafia_state().loan = std::nullopt;
    }
    else if (loan) {
      player->mafia_state().loan = std::nullopt;
      game_over_pending_ = true;
    }
    break;
  }
  default:
    break;
  }
}

bool MafiaConversation::advance() {
  switch (current_line_) {
  case MafiaLine::HEY_YOU_LOUSE_LITTLE_PIZZA_MAKER:
  case MafiaLine::WELL_YOU_LITTLE_CROOK:
  case MafiaLine::WELL_YOU_UPSTART:
  case MafiaLine::HELLO_PARTNER:
  case MafiaLine::HELLO_BOSS_EVERYTHING_OK:
  case MafiaLine::HELLO_BOSS_HOWS_BUSINESS:
  case MafiaLine::YOUVE_ALREADY_GOT_A_MISSION:
  case MafiaLine::WE_NEED_TO_GET_TO_KNOW_YOU_BETTER:
  case MafiaLine::SORRY_WE_DONT_HAVE_MISSIONS:
  case MafiaLine::DONT_FORGET_IF_YOU_CANT_PAY_IN_THREE_MONTHS:
  case MafiaLine::AT_THE_MOMENT_WE_DONT_HAVE_INFO_FOR_SALE:
  case MafiaLine::YOU_ALREADY_HAVE_A_LOAN_WITH_US:
  case MafiaLine::WHO_DO_YOU_THINK_YOU_ARE:
    current_line(MafiaLine::LOOKING_FOR_JOB_ON_THE_SIDE);
    break;

  case MafiaLine::MISSION_OFFER:
    current_line(MafiaLine::MISSION_RISK_ACCEPT);
    break;

  case MafiaLine::TAKE_A_LOOK_AT_LOUSY_LOSER:
    current_line(MafiaLine::WHO_DO_YOU_THINK_YOU_ARE);
    break;

  case MafiaLine::I_KNOW_A_REALLY_HOT_PHONE_NUMBER:
  case MafiaLine::I_KNOW_CHEAT_CODE_FOR_PIZZA_TYCOON:
  case MafiaLine::I_KNOW_WHAT_QUEEN_ELIZABETHS_DOG_IS_CALLED:
  case MafiaLine::I_KNOW_HOW_TO_LOSE_50KG:
  case MafiaLine::I_KNOW_PHONENR_OF_GOOD_ARMS_DEALER:
  case MafiaLine::I_HAVE_LATEST_WEAPON_PRICES:
    current_line(MafiaLine::TIP_PRICE_OFFER);
    break;

  case MafiaLine::DIAL_MICROPROSE_UK_CS:
  case MafiaLine::GO_TO_MARKET_RESEARCH_HONEST:
  case MafiaLine::HIS_NAME_IS_FIFI:
  case MafiaLine::WITH_DR_YMMY_NITRO_GLICERINE_DIET:
  case MafiaLine::TRY:
  case MafiaLine::WEAPONS_PRICES_ARE:
    current_line(MafiaLine::LOOKING_FOR_JOB_ON_THE_SIDE);
    break;

  default:
    std::cerr << "MafiaConversation::advance() unhandled state "
              << magic_enum::enum_name(current_line_) << "\n";
    return false;
  }
  return true;
}

bool MafiaConversation::advance_line_choice(MafiaLine choice) {
  auto* player = state_.active_player();
  int uw_score = player->social_status().underworld_score();
  int rank = social_rank(uw_score, state_.difficulty_level());

  switch (choice) {
  case MafiaLine::LOOKING_FOR_JOB_ON_THE_SIDE: {
    if (player->mafia_state().active_mission.has_value()) {
      current_line(MafiaLine::YOUVE_ALREADY_GOT_A_MISSION);
      break;
    }
    auto mission = generate_mission(rank);
    if (!mission) {
      current_line(MafiaLine::SORRY_WE_DONT_HAVE_MISSIONS);
      break;
    }
    pending_mission_ = *mission;
    mission_risk_ = mission->risk_pct;
    current_line(MafiaLine::MISSION_OFFER);
    break;
  }

  case MafiaLine::I_NEED_MONEY_URGENTLY: {
    if (player->mafia_state().loan.has_value()) {
      current_line(MafiaLine::YOU_ALREADY_HAVE_A_LOAN_WITH_US);
      break;
    }
    if (rank < 2) {
      current_line(MafiaLine::WE_NEED_TO_GET_TO_KNOW_YOU_BETTER);
      break;
    }
    loan_amount_ = std::max(uw_score * 50, 500);
    loan_repayment_ = loan_amount_ * (clan_danger_ + 100) / 100;
    current_line(MafiaLine::LOAN_OFFER);
    break;
  }

  case MafiaLine::I_NEED_INFORMATION: {
    if (rank < 3) {
      current_line(MafiaLine::TAKE_A_LOOK_AT_LOUSY_LOSER);
      break;
    }
    if (!coin_flip()) {
      current_line(MafiaLine::AT_THE_MOMENT_WE_DONT_HAVE_INFO_FOR_SALE);
      break;
    }
    tip_price_ = rand_between(2, 7) * 250;
    tip_opener_text_.clear();
    tip_result_text_.clear();
    enum class MafiaTips {
      SupportPhoneNumber,
      WeaponsPrices,
      ArmsDealerPhoneNumber,
      Count,
    };
    auto tip_type =
        static_cast<MafiaTips>(rand_max_n(static_cast<int>(MafiaTips::Count)));
    switch (tip_type) {
    case MafiaTips::SupportPhoneNumber: {
      int base = static_cast<int>(MafiaLine::I_KNOW_A_REALLY_HOT_PHONE_NUMBER);
      auto opener = static_cast<MafiaLine>(base + (rand_max_n(4) * 2));
      tip_reveal_ = static_cast<MafiaLine>(static_cast<int>(opener) + 1);
      current_line(opener);
      break;
    }
    case MafiaTips::WeaponsPrices: {
      using core::item::Item;
      constexpr Item weapons[] = {Item::FlameThrower, Item::Chemicals,
                                  Item::Bazooka, Item::Bomb};
      auto tip_city_id = core::CityId{rand_max_n(NUMBER_OF_CITIES)};
      auto tip_city_name = core::city_name(tip_city_id);
      int w1 = rand_max_n(4);
      int w2 = (w1 + 1) % 4;
      auto& market = state_.weapon_market();
      auto price1 = market.purchase_price(tip_city_id, weapons[w1]);
      auto price2 = market.purchase_price(tip_city_id, weapons[w2]);
      tip_opener_text_ = compat::format(" {}.", tip_city_name);
      tip_result_text_ =
          compat::format(" {}: {} cost {:w} and {} {:w} at the moment.",
                         tip_city_name, core::item::to_string(weapons[w1]),
                         price1, core::item::to_string(weapons[w2]), price2);
      tip_reveal_ = MafiaLine::WEAPONS_PRICES_ARE;
      current_line(MafiaLine::I_HAVE_LATEST_WEAPON_PRICES);
      break;
    }
    case MafiaTips::ArmsDealerPhoneNumber: {
      // Arms dealer phone number tip
      auto* city = state_.active_city();
      auto dealers = city->all_arms_dealers();
      if (dealers.size() > 1) {
        // Skip the first dealer (always known), pick from the rest
        int idx = 1 + rand_max_n(static_cast<int>(dealers.size()) - 1);
        const auto* dealer = dealers[idx];
        tip_result_text_ = compat::format(" {}!!!", dealer->phone_number());
        tip_reveal_ = MafiaLine::TRY;
        current_line(MafiaLine::I_KNOW_PHONENR_OF_GOOD_ARMS_DEALER);
      }
      else {
        // This should never happen
        current_line(MafiaLine::AT_THE_MOMENT_WE_DONT_HAVE_INFO_FOR_SALE);
      }
      break;
    }
    case MafiaTips::Count: {
      fatal("MafiaTips Count should never be picked as an option.");
      break;
    }
    }
    break;
  }
  default:
    std::cerr << "MafiaConversation::advance_line_choice() unhandled choice "
              << magic_enum::enum_name(choice) << "\n";
    return false;
  }
  return true;
}

bool MafiaConversation::advance_proposal(bool agree) {
  auto* player = state_.active_player();
  const auto& date = state_.time().current_date();

  switch (current_line_) {
  case MafiaLine::NICE_PLACE_YOU_GOT_HERE:
  case MafiaLine::HERE_WE_ARE_AGAIN_WE_WANT_THE_USUAL:
  case MafiaLine::PROTECTION_OFFER: {
    auto* restaurant =
        restaurant_id_ ? state_.restaurant(*restaurant_id_) : nullptr;
    if (!restaurant) {
      fatal("No restaurant to offer protection?!");
    }
    int crime_stats = restaurant->property().district().crime_statistics();

    if (agree) {
      player->financials().spend_cash(
          date, Account::Protection,
          Money{static_cast<int64_t>(protection_price_) * 100});
      if (restaurant) {
        restaurant->protection_timer(rand_between(3, 22) + crime_stats);
      }
      current_line(MafiaLine::THANKS_YOULL_BE_HEARING_FROM_US);
    }
    else {
      state_.adjust_underworld_score(*player, -10);
      int personality = player->character().personality();
      int courage = player->character().courage();
      bool lucky = rand_percentage_chance(personality) &&
                   rand_percentage_chance(courage);

      if (lucky) {
        if (restaurant) {
          restaurant->protection_timer(rand_between(3, 3 + crime_stats));
        }
        current_line(MafiaLine::OK_YOURE_IN_LUCK_THIS_TIME);
      }
      else {
        // Destroy furniture and replace with ashes
        if (restaurant && restaurant->layout()) {
          int pieces_to_destroy = rand_between(5, 14);
          Money total_damage{0};
          auto& layout = *restaurant->layout();

          for (int i = 0;
               i < pieces_to_destroy && !layout.placed_furniture().empty();
               i++) {
            int idx =
                rand_max_n(static_cast<int>(layout.placed_furniture().size()));
            auto* pf = layout.placed_furniture()[idx].get();
            if (!pf || !pf->furniture() || pf->is_ashes()) {
              continue;
            }
            total_damage = total_damage + pf->furniture()->price();
            pf->change_to_ashes();
          }

          if (total_damage.cents > 0) {
            player->financials().transfer(
                date, Account::Furniture, Account::ProfitAndLoss, total_damage,
                *restaurant_id_, AccountingScope::Capital);
          }
          restaurant->update_demographics(player->recipes());
        }
        if (restaurant) {
          restaurant->protection_timer(rand_between(14, 14 + crime_stats));
        }
        current_line(MafiaLine::PLAYING_BIG_HERO_HUH);
      }
    }
    break;
  }

  case MafiaLine::LOAN_OFFER:
    if (agree) {
      // Player receives the loan amount
      player->financials().spend_cash(
          date, Account::Loan,
          Money{static_cast<int64_t>(-loan_amount_) * 100});
      // Must repay the repayment amount (loan + interest) in 90 days
      player->mafia_state().loan = MafiaLoan{
          .amount = Money{static_cast<int64_t>(loan_repayment_) * 100},
          .due_date = date.add_days(90),
          .clan = clan_};
      state_.adjust_underworld_score(*player, 40);
      current_line(MafiaLine::DONT_FORGET_IF_YOU_CANT_PAY_IN_THREE_MONTHS);
    }
    else {
      current_line(MafiaLine::LOOKING_FOR_JOB_ON_THE_SIDE);
    }
    break;

  case MafiaLine::MISSION_RISK_ACCEPT:
    if (agree) {
      if (pending_mission_) {
        pending_mission_->active = true;
        pending_mission_->clan = clan_;
        // Pre-roll the bust: risk_pct chance of police arrest at pickup.
        // Investment missions use risk for profit outcome instead (already
        // handled in generate_mission), so they don't get busted.
        if (pending_mission_->mission_type != MafiaMissionType::Investment) {
          pending_mission_->busted = engine::random::rand_percentage_chance(
              pending_mission_->risk_pct);
        }
        player->mafia_state().active_mission = *pending_mission_;

        if (pending_mission_->mission_type == MafiaMissionType::Laundering) {
          // Give the player the laundering cash up front
          player->financials().spend_cash(date, Account::Missions,
                                          -pending_mission_->cash_amount);
          // Register the 4 laundering accounts so bank transfers reach them
          for (int acct : pending_mission_->bank_accounts) {
            state_.bank_accounts().register_account(acct);
          }
        }
        else if (pending_mission_->mission_type ==
                 MafiaMissionType::Investment) {
          // Deduct investment cost
          player->financials().spend_cash(date, Account::Missions,
                                          pending_mission_->cash_amount);
        }
      }
      current_line(MafiaLine::WE_EXPECT_YOU_TO_CARRY_OUT_ORDERS_TO_THE_LETTER);
    }
    else {
      state_.adjust_underworld_score(*player, -5);
      pending_mission_ = std::nullopt;
      current_line(MafiaLine::LOOKING_FOR_JOB_ON_THE_SIDE);
    }
    break;

  case MafiaLine::TIP_PRICE_OFFER:
    if (agree) {
      player->financials().spend_cash(
          date, Account::Others, Money{static_cast<int64_t>(tip_price_) * 100});
      if (!tip_reveal_) {
        fatal("Tip reveal line not set");
      }
      current_line(*tip_reveal_);
    }
    else {
      current_line(MafiaLine::LOOKING_FOR_JOB_ON_THE_SIDE);
    }
    break;

  default:
    return advance();
  }
  return true;
}

std::string MafiaConversation::protection_offer_text() const {
  std::string rest_name;
  if (restaurant_id_) {
    auto* r = state_.restaurant(*restaurant_id_);
    if (r) {
      rest_name = r->name();
    }
  }
  return compat::format(
      " For just ${} we will offer you protection of restaurant {}! OK?",
      protection_price_, rest_name);
}

Emotion MafiaConversation::emotion() const {
  switch (current_line_) {
  case MafiaLine::YOU_RAT_YOU_HAVENT_CARRIED_OUT_MISSION:
  case MafiaLine::PLAYING_BIG_HERO_HUH:
  case MafiaLine::TAKE_A_LOOK_AT_LOUSY_LOSER:
  case MafiaLine::WHO_DO_YOU_THINK_YOU_ARE:
  case MafiaLine::I_HOPE_YOU_HAVENT_FORGOTTEN_THE_THREE_MONTHS_ARE_UP:
    return Emotion::ANGRY;
  case MafiaLine::THANKS_YOULL_BE_HEARING_FROM_US:
  case MafiaLine::YOU_CARRIED_OUR_MISSION_TO_SATISFACTION:
  case MafiaLine::THE_BUSINESS_WAS_SUCCESSFUL:
  case MafiaLine::OK_TODAY_IS_PAY_DAY:
    return Emotion::HAPPY;
  case MafiaLine::WE_EXPECT_YOU_TO_CARRY_OUT_ORDERS_TO_THE_LETTER:
    return Emotion::NOD;
  default:
    return Emotion::NEUTRAL;
  }
}

std::vector<ConversationLine<MafiaLine>> MafiaConversation::lines() const {
  switch (current_line_) {

  case MafiaLine::HEY_YOU_LOUSE_LITTLE_PIZZA_MAKER:
    return {make_line(MafiaLine::HEY_YOU_LOUSE_LITTLE_PIZZA_MAKER)};
  case MafiaLine::WELL_YOU_LITTLE_CROOK:
    return {make_line(MafiaLine::WELL_YOU_LITTLE_CROOK)};
  case MafiaLine::WELL_YOU_UPSTART:
    return {make_line(MafiaLine::WELL_YOU_UPSTART)};
  case MafiaLine::HELLO_PARTNER:
    return {make_line(MafiaLine::HELLO_PARTNER)};
  case MafiaLine::HELLO_BOSS_EVERYTHING_OK:
    return {make_line(MafiaLine::HELLO_BOSS_EVERYTHING_OK)};
  case MafiaLine::HELLO_BOSS_HOWS_BUSINESS:
    return {make_line(MafiaLine::HELLO_BOSS_HOWS_BUSINESS)};

  case MafiaLine::LOOKING_FOR_JOB_ON_THE_SIDE:
    return {
        make_option(MafiaLine::LOOKING_FOR_JOB_ON_THE_SIDE),
        make_option(MafiaLine::I_NEED_MONEY_URGENTLY),
        make_option(MafiaLine::I_NEED_INFORMATION),
    };

  case MafiaLine::YOUVE_ALREADY_GOT_A_MISSION:
    return {make_line(MafiaLine::YOUVE_ALREADY_GOT_A_MISSION)};
  case MafiaLine::WE_NEED_TO_GET_TO_KNOW_YOU_BETTER:
    return {make_line(MafiaLine::WE_NEED_TO_GET_TO_KNOW_YOU_BETTER)};
  case MafiaLine::SORRY_WE_DONT_HAVE_MISSIONS:
    return {make_line(MafiaLine::SORRY_WE_DONT_HAVE_MISSIONS)};
  case MafiaLine::MISSION_OFFER: {
    std::string desc;
    if (pending_mission_) {
      const auto& m = *pending_mission_;
      switch (m.mission_type) {
      case MafiaMissionType::Package: {
        auto item_name = lines_.at(m.item_index);
        auto pickup_dt =
            format_datetime(m.pickup.date, m.pickup.hour, m.pickup.minute);
        auto dropoff_dt =
            format_datetime(m.dropoff.date, m.dropoff.hour, m.dropoff.minute);
        desc = compat::format(
            "Pick up {} in the {} {} and bring it to the {} {}!", item_name,
            m.pickup.description, pickup_dt, m.dropoff.description, dropoff_dt);
        break;
      }
      case MafiaMissionType::Attack: {
        // Dead code in original, type 2 is never randomly assigned
        desc = compat::format(
            "Attack restaurant {} and cause at least {}% chemical "
            "contamination!",
            m.dropoff.description, m.contamination_pct);
        break;
      }
      case MafiaMissionType::Weapon: {
        auto weapon_name = lines_.at(m.item_index);
        auto dropoff_dt =
            format_datetime(m.dropoff.date, m.dropoff.hour, m.dropoff.minute);
        desc =
            compat::format("Get {} {} and bring them {} to the {}", m.quantity,
                           weapon_name, dropoff_dt, m.dropoff.description);
        break;
      }
      case MafiaMissionType::Laundering: {
        auto deadline_dt = format_datetime_before(m.deadline, m.deadline_hour,
                                                  m.deadline_minute);
        desc = compat::format(
            "You will now receive {} from us. Divide the money equally "
            "among the following accounts {}: {}, {}, {}, {}",
            m.cash_amount, deadline_dt, m.bank_accounts[0], m.bank_accounts[1],
            m.bank_accounts[2], m.bank_accounts[3]);
        break;
      }
      case MafiaMissionType::Investment: {
        auto inv_name = lines_.at(m.item_index);
        desc = compat::format(
            "We can offer you a share in a very hot deal: about {}. "
            "It will cost you {} The profit would be {}",
            inv_name, m.cash_amount, m.profit);
        break;
      }
      }
    }
    return {make_line(MafiaLine::MISSION_OFFER, {.custom_text = desc})};
  }
  case MafiaLine::MISSION_RISK_ACCEPT: {
    std::string text;
    if (pending_mission_) {
      const auto& m = *pending_mission_;
      if (m.mission_type != MafiaMissionType::Investment) {
        text +=
            compat::format("If you are successful, you'll get {}. ", m.reward);
      }
      text += compat::format("The risk is {}%. Do you accept the mission?",
                             m.risk_pct);
    }
    else {
      text = compat::format("The risk is {}%. Do you accept the mission?",
                            mission_risk_);
    }
    return {make_line(
        MafiaLine::MISSION_RISK_ACCEPT,
        {.is_proposal = true, .easy_accept = false, .custom_text = text})};
  }
  case MafiaLine::WE_EXPECT_YOU_TO_CARRY_OUT_ORDERS_TO_THE_LETTER:
    return {make_terminal_line(
        MafiaLine::WE_EXPECT_YOU_TO_CARRY_OUT_ORDERS_TO_THE_LETTER)};

  case MafiaLine::YOU_CARRIED_OUR_MISSION_TO_SATISFACTION:
    return {
        make_terminal_line(MafiaLine::YOU_CARRIED_OUR_MISSION_TO_SATISFACTION)};
  case MafiaLine::THE_BUSINESS_WAS_SUCCESSFUL:
    return {make_terminal_line(MafiaLine::THE_BUSINESS_WAS_SUCCESSFUL)};
  case MafiaLine::INVESTMENT_FAILED:
    return {make_terminal_line(
        MafiaLine::INVESTMENT_FAILED,
        "Unfortunately, our business has gone down the drain. "
        "It's just one of those things. Of course, the money's "
        "gone too! Sorry!")};
  case MafiaLine::YOU_RAT_YOU_HAVENT_CARRIED_OUT_MISSION:
    return {
        make_terminal_line(MafiaLine::YOU_RAT_YOU_HAVENT_CARRIED_OUT_MISSION)};

  // Protection racket
  case MafiaLine::NICE_PLACE_YOU_GOT_HERE:
    [[fallthrough]];
  case MafiaLine::HERE_WE_ARE_AGAIN_WE_WANT_THE_USUAL: {
    auto offer_text = protection_offer_text();
    return {make_line(current_line_, {.is_proposal = true,
                                      .easy_accept = false,
                                      .custom_text = offer_text})};
  }
  case MafiaLine::PROTECTION_OFFER: {
    auto text = protection_offer_text();
    return {make_line(
        MafiaLine::PROTECTION_OFFER,
        {.is_proposal = true, .easy_accept = false, .custom_text = text})};
  }
  case MafiaLine::THANKS_YOULL_BE_HEARING_FROM_US:
    return {make_terminal_line(MafiaLine::THANKS_YOULL_BE_HEARING_FROM_US)};
  case MafiaLine::OK_YOURE_IN_LUCK_THIS_TIME:
    return {make_terminal_line(MafiaLine::OK_YOURE_IN_LUCK_THIS_TIME)};
  case MafiaLine::PLAYING_BIG_HERO_HUH:
    return {make_terminal_line(MafiaLine::PLAYING_BIG_HERO_HUH)};

  // Loan
  case MafiaLine::YOU_ALREADY_HAVE_A_LOAN_WITH_US: {
    auto* player = state_.active_player();
    auto& loan = player->mafia_state().loan;
    if (loan) {
      int days_left = loan->due_date.days_since(state_.time().current_date());
      auto suffix = lines_.at(
          static_cast<int>(MafiaLine::DAYS_OTHERWISE_YOUD_BETTER_RUN));
      return {make_line(
          MafiaLine::YOU_ALREADY_HAVE_A_LOAN_WITH_US,
          {.custom_text = compat::format(" {} {}", days_left, suffix)})};
    }
    return {make_line(MafiaLine::YOU_ALREADY_HAVE_A_LOAN_WITH_US)};
  }
  case MafiaLine::LOAN_OFFER: {
    const auto& pay_text =
        lines_.at(static_cast<int>(MafiaLine::IF_YOU_PAY_US_THE_SUM));
    auto text =
        compat::format(" ${}{} ${}", loan_amount_, pay_text, loan_repayment_);
    return {make_line(
        MafiaLine::OK_I_THINK_WE_AN_GIVE_YOU_LOAN,
        {.is_proposal = true, .easy_accept = false, .custom_text = text})};
  }
  case MafiaLine::DONT_FORGET_IF_YOU_CANT_PAY_IN_THREE_MONTHS:
    return {make_line(MafiaLine::DONT_FORGET_IF_YOU_CANT_PAY_IN_THREE_MONTHS)};

  // Loan repayment
  case MafiaLine::OK_TODAY_IS_PAY_DAY:
    return {make_terminal_line(MafiaLine::OK_TODAY_IS_PAY_DAY)};
  case MafiaLine::I_HOPE_YOU_HAVENT_FORGOTTEN_THE_THREE_MONTHS_ARE_UP:
    return {make_terminal_line(
        MafiaLine::I_HOPE_YOU_HAVENT_FORGOTTEN_THE_THREE_MONTHS_ARE_UP)};

  // Information
  case MafiaLine::TAKE_A_LOOK_AT_LOUSY_LOSER:
    return {make_line(MafiaLine::TAKE_A_LOOK_AT_LOUSY_LOSER)};
  case MafiaLine::WHO_DO_YOU_THINK_YOU_ARE:
    return {make_line(MafiaLine::WHO_DO_YOU_THINK_YOU_ARE)};

  case MafiaLine::AT_THE_MOMENT_WE_DONT_HAVE_INFO_FOR_SALE:
    return {make_line(MafiaLine::AT_THE_MOMENT_WE_DONT_HAVE_INFO_FOR_SALE)};

  case MafiaLine::I_HAVE_LATEST_WEAPON_PRICES:
    return {make_line(MafiaLine::I_HAVE_LATEST_WEAPON_PRICES,
                      {.custom_text = tip_opener_text_})};
  case MafiaLine::WEAPONS_PRICES_ARE:
    return {make_line(MafiaLine::WEAPONS_PRICES_ARE,
                      {.custom_text = tip_result_text_})};

  case MafiaLine::I_KNOW_PHONENR_OF_GOOD_ARMS_DEALER:
    return {make_line(MafiaLine::I_KNOW_PHONENR_OF_GOOD_ARMS_DEALER)};
  case MafiaLine::TRY:
    return {make_line(MafiaLine::TRY, {.custom_text = tip_result_text_})};

  case MafiaLine::I_KNOW_A_REALLY_HOT_PHONE_NUMBER:
    return {make_line(MafiaLine::I_KNOW_A_REALLY_HOT_PHONE_NUMBER)};
  case MafiaLine::I_KNOW_CHEAT_CODE_FOR_PIZZA_TYCOON:
    return {make_line(MafiaLine::I_KNOW_CHEAT_CODE_FOR_PIZZA_TYCOON)};
  case MafiaLine::I_KNOW_WHAT_QUEEN_ELIZABETHS_DOG_IS_CALLED:
    return {make_line(MafiaLine::I_KNOW_WHAT_QUEEN_ELIZABETHS_DOG_IS_CALLED)};
  case MafiaLine::I_KNOW_HOW_TO_LOSE_50KG:
    return {make_line(MafiaLine::I_KNOW_HOW_TO_LOSE_50KG)};

  case MafiaLine::DIAL_MICROPROSE_UK_CS:
    return {make_line(MafiaLine::DIAL_MICROPROSE_UK_CS)};
  case MafiaLine::GO_TO_MARKET_RESEARCH_HONEST:
    return {make_line(MafiaLine::GO_TO_MARKET_RESEARCH_HONEST)};
  case MafiaLine::HIS_NAME_IS_FIFI:
    return {make_line(MafiaLine::HIS_NAME_IS_FIFI)};
  case MafiaLine::WITH_DR_YMMY_NITRO_GLICERINE_DIET:
    return {make_line(MafiaLine::WITH_DR_YMMY_NITRO_GLICERINE_DIET)};

  case MafiaLine::TIP_PRICE_OFFER: {
    auto text = compat::format(" ${}. Interested?", tip_price_);
    return {make_proposal(MafiaLine::THIS_TRIP_WILL_ONLY_COST_YOU, text)};
  }

  default:
    std::cerr << "MafiaConversation::lines() unhandled state "
              << magic_enum::enum_name(current_line_) << "\n";
    break;
  }
  return {};
}

bool MafiaConversation::has_ended() const {
  switch (current_line_) {
  case MafiaLine::THANKS_YOULL_BE_HEARING_FROM_US:
  case MafiaLine::OK_YOURE_IN_LUCK_THIS_TIME:
  case MafiaLine::PLAYING_BIG_HERO_HUH:
  case MafiaLine::YOU_CARRIED_OUR_MISSION_TO_SATISFACTION:
  case MafiaLine::YOU_RAT_YOU_HAVENT_CARRIED_OUT_MISSION:
  case MafiaLine::OK_TODAY_IS_PAY_DAY:
  case MafiaLine::I_HOPE_YOU_HAVENT_FORGOTTEN_THE_THREE_MONTHS_ARE_UP:
  case MafiaLine::WE_EXPECT_YOU_TO_CARRY_OUT_ORDERS_TO_THE_LETTER:
    if (game_over_pending_) {
      game_over_pending_ = false;
      auto* player = state_.active_player();
      state_.record_milestone(
          core::events::MilestoneId::CoolVinnieAndBigLuigiTakeYouForABath,
          player->id());
      state_.ui().push<GameOverState>(player->id(),
                                      GameOverCause::KilledByMafia);
    }
    if (star_message_pending_) {
      star_message_pending_ = false;
      auto* player = state_.active_player();
      state_.record_milestone(
          core::events::MilestoneId::YouSeeStarsAndHearPlentyOfBirdSong,
          player->id());
    }
    return true;
  default:
    return false;
  }
}

MafiaLine MafiaConversation::compute_start_line(
    GameState& state, const Actor* /* actor */, MafiaMode mode,
    std::optional<core::RestaurantId> /* restaurant_id */, bool first_contact) {
  auto* player = state.active_player();
  int uw_score = player->social_status().underworld_score();
  int rank = social_rank(uw_score, state.difficulty_level());

  switch (mode) {
  case MafiaMode::PlayerCall: {
    int greeting_offset = std::clamp((rank - 1) / 2, 0, 5);
    int base = static_cast<int>(MafiaLine::HEY_YOU_LOUSE_LITTLE_PIZZA_MAKER);
    return static_cast<MafiaLine>(base + greeting_offset);
  }

  case MafiaMode::ProtectionRacketOffer: {
    if (first_contact) {
      return MafiaLine::NICE_PLACE_YOU_GOT_HERE;
    }
    return MafiaLine::HERE_WE_ARE_AGAIN_WE_WANT_THE_USUAL;
  }

  case MafiaMode::MissionDeadlineReached: {
    auto& mission = player->mafia_state().active_mission;
    if (mission && mission->mission_type == MafiaMissionType::Investment) {
      return mission->reward.cents > 0 ? MafiaLine::THE_BUSINESS_WAS_SUCCESSFUL
                                       : MafiaLine::INVESTMENT_FAILED;
    }
    return MafiaLine::YOU_CARRIED_OUR_MISSION_TO_SATISFACTION;
  }

  case MafiaMode::MissionOverdue:
    return MafiaLine::YOU_RAT_YOU_HAVENT_CARRIED_OUT_MISSION;

  case MafiaMode::LoanRepaymentDue: {
    auto& loan = player->mafia_state().loan;
    if (loan && player->cash_balance() >= loan->amount) {
      return MafiaLine::OK_TODAY_IS_PAY_DAY;
    }
    return MafiaLine::I_HOPE_YOU_HAVENT_FORGOTTEN_THE_THREE_MONTHS_ARE_UP;
  }
  }
  return MafiaLine::HEY_YOU_LOUSE_LITTLE_PIZZA_MAKER;
}

std::optional<MafiaMission> MafiaConversation::generate_mission(int rank) {
  MafiaMission mission{};
  const auto& date = state_.time().current_date();
  auto* city = state_.active_city();
  int difficulty = state_.difficulty_level();

  // Higher rank unlocks higher mission types (rank/2, capped at 5).
  // rand_max_n(0) returns 0, so rank 0-1 always yields type 1 (package).
  int max_type = std::min(rank / 2, 5);
  int type = rand_max_n(max_type) + 1;
  if (type == 2) {
    type = 3; // skip sabotage type (dead code in original)
  }

  // The first 72 time blocks are available for missions (0:00 to 11:50).
  // Pickup: 1-4 days from today
  int pickup_days = 1 + rand_max_n(4);
  auto pickup_date = date.add_days(pickup_days);
  int pickup_slot = rand_max_n(72);
  int pickup_hour = pickup_slot / 6;
  int pickup_minute = (pickup_slot % 6) * 10;

  // Delivery: 1-3 days after pickup
  int delivery_days = pickup_days + 1 + rand_max_n(3);
  auto delivery_date = date.add_days(delivery_days);

  // Original fails when the deadline would cross the year boundary
  if (delivery_date.year() != date.year()) {
    return std::nullopt;
  }
  int delivery_slot = rand_max_n(72);
  int delivery_hour = delivery_slot / 6;
  int delivery_minute = (delivery_slot % 6) * 10;

  switch (type) {
  case 1: {
    // Package delivery: pick up item at one building, deliver to another
    mission.mission_type = MafiaMissionType::Package;
    mission.item_index =
        rand_between(static_cast<int>(MafiaLine::A_BUCKET_OF_WATER),
                     static_cast<int>(MafiaLine::A_TIME_MACHINE) + 1);

    auto source = pick_random_building(city);
    auto dest = pick_random_building(city, source.tile_index);
    mission.pickup.description = source.description;
    mission.pickup.tile_index = source.tile_index;
    mission.dropoff.description = dest.description;
    mission.dropoff.tile_index = dest.tile_index;

    mission.pickup.date = pickup_date;
    mission.pickup.hour = pickup_hour;
    mission.pickup.minute = pickup_minute;

    mission.dropoff.date = delivery_date;
    mission.dropoff.hour = delivery_hour;
    mission.dropoff.minute = delivery_minute;

    mission.risk_pct =
        (difficulty * 3) + clan_danger_ + rand_max_n(difficulty * 3);

    int reward_raw = (rand_max_n(60) + 60) * (clan_danger_ + 5);
    mission.reward = Money{static_cast<int64_t>(reward_raw) * 100};

    mission.active = true;
    mission.deadline = delivery_date;
    break;
  }
  case 3: {
    // Weapons acquisition: buy weapons and drop off
    mission.mission_type = MafiaMissionType::Weapon;
    mission.quantity = rand_max_n(20) + 2;
    mission.item_index =
        rand_between(static_cast<int>(MafiaLine::FLAME_THROWERS),
                     static_cast<int>(MafiaLine::BOMBS) + 1);

    auto dest = pick_random_building(city);
    mission.dropoff.description = dest.description;
    mission.dropoff.tile_index = dest.tile_index;
    mission.dropoff.date = delivery_date;
    mission.dropoff.hour = delivery_hour;
    mission.dropoff.minute = delivery_minute;

    mission.risk_pct = (difficulty * 3) + clan_danger_ +
                       rand_max_n(std::max(difficulty * 3, 1));

    // Paid at the going rate for the weapon asked for, in the city the job was
    // offered in. The per-unit price is truncated before the quantity applies.
    auto weapon = static_cast<core::item::Item>(
        static_cast<int>(core::item::Item::FlameThrower) +
        (mission.item_index - static_cast<int>(MafiaLine::FLAME_THROWERS)));
    const Money unit_price =
        (state_.weapon_market().purchase_price(city->id(), weapon) *
         ((mission.risk_pct * 2) + (rand_max_n(80) + 80)) / 100)
            .truncated_to_dollars();
    mission.reward = unit_price * mission.quantity;

    mission.deadline = delivery_date;
    break;
  }
  case 4: {
    // Money laundering: receive cash, distribute across 4 bank accounts
    mission.mission_type = MafiaMissionType::Laundering;
    int amount_10k = rand_max_n(120) + 30;
    mission.cash_amount =
        Money{static_cast<int64_t>(amount_10k) * 10'000 * 100};

    for (int i = 0; i < 4; i++) {
      mission.bank_accounts[i] = rand_between(100'000, 1'000'000);
    }

    // Deadline uses the generic delivery date
    mission.deadline = delivery_date;
    mission.deadline_hour = delivery_hour;
    mission.deadline_minute = delivery_minute;

    mission.risk_pct = rand_between(20, 60);

    int reward_raw =
        static_cast<int>((static_cast<int64_t>(amount_10k) * 10'000 *
                          (mission.risk_pct + rand_max_n(10))) /
                         100);
    mission.reward = Money{static_cast<int64_t>(reward_raw) * 100};

    mission.active = true;
    break;
  }
  case 5: {
    // Investment deal: pay upfront, receive profit later (or not).
    // Result pre-decided at generation: risk_pct chance of failure.
    mission.risk_pct = rand_between(15, 45);
    mission.mission_type = MafiaMissionType::Investment;
    mission.item_index =
        rand_between(static_cast<int>(MafiaLine::A_COLUMN_COFFEE_PLANTATION),
                     static_cast<int>(MafiaLine::A_DAM_BUSTING_RAID) + 1);

    int profit = (rand_max_n(15'000) + 8'000) * (mission.risk_pct + 5);
    mission.profit = Money{
        static_cast<int64_t>(math::round_up_to_next_power10<2>(profit)) * 100};

    int profit_dollars = static_cast<int>(mission.profit.cents / 100);
    int cost_dollars = (profit_dollars / 2) + (rand_max_n(10) * 10'000);
    mission.cash_amount = Money{
        static_cast<int64_t>(math::round_up_to_next_power10<2>(cost_dollars)) *
        100};

    // Result pre-decided: risk_pct chance of failure. reward=0 means failure.
    mission.reward =
        rand_percentage_chance(mission.risk_pct) ? 0_usd : mission.profit;
    mission.completed = true;
    mission.deadline = delivery_date;
    break;
  }
  default:
    fatal("Unknown mission type");
    break;
  }

  return mission;
}

bool MafiaConversation::back_allowed() const {
  switch (current_line_) {
  case MafiaLine::NICE_PLACE_YOU_GOT_HERE:
  case MafiaLine::HERE_WE_ARE_AGAIN_WE_WANT_THE_USUAL:
  case MafiaLine::PLAYING_BIG_HERO_HUH:
  case MafiaLine::OK_TODAY_IS_PAY_DAY:
  case MafiaLine::I_HOPE_YOU_HAVENT_FORGOTTEN_THE_THREE_MONTHS_ARE_UP:
    return false;
  default:
    return true;
    break;
  }
}
