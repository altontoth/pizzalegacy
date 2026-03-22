/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "conversations/ArmsDealerConversation.h"
#include "conversations/ArmsDealerLine.h"
#include "core/character/Player.h"
#include "core/inventory/Inventory.h"
#include "engine/FormatCompat.h"

ArmsDealerConversation::ArmsDealerConversation(GameState& state,
                                               const Actor* actor)
    : BaseConversation(compute_start_line(state, actor), state, "t/waffen.e",
                       actor),
      market_(state.active_city()->weapon_market()) {}

/* This advances only the lines that don't require user choices */
bool ArmsDealerConversation::advance() {
  switch (current_line_) {
  case ArmsDealerLine::WELCOME:
    current_line(ArmsDealerLine::I_WANT_JOKE_ARTICLES);
    break;
  case ArmsDealerLine::OK_HOW_MANY:
  case ArmsDealerLine::HOW_MANY_SCOOPS:
    current_line(ArmsDealerLine::ONE_PIECE);
    break;
  case ArmsDealerLine::JOKE_INTRO:
    current_line(ArmsDealerLine::SMELLY_CHEESE);
    break;
  case ArmsDealerLine::WHAT_KIND_OF_GOODS:
  case ArmsDealerLine::CHOOSE_FLAVOR:
    current_line(ArmsDealerLine::ICE_VANILLA);
    break;
  case ArmsDealerLine::HOW_MANY_SELL:
    current_line(ArmsDealerLine::ONE_PIECE);
    break;
  case ArmsDealerLine::YOU_REACHED_MAX_ITEMS:
    // TODO: check if they are buying weapons or joke items
    // we might have to store an extra line of history for that,
    // or set a boolean flag or something
    current_line(ArmsDealerLine::BE_CAREFUL_ICE_IS_HOT);
    break;
  case ArmsDealerLine::DONT_NEGOTIATE:
  case ArmsDealerLine::HAVE_FUN:
  case ArmsDealerLine::BE_CAREFUL_ICE_IS_HOT:
  case ArmsDealerLine::PLEASURE_DOING_BUSINESS:
  case ArmsDealerLine::ALREADY_AT_MAX_ITEMS:
  case ArmsDealerLine::NOT_ENOUGH_WEAPONS:
  case ArmsDealerLine::YOU_CANT_AFFORD_WHAT_A_SHAME:
    current_line(ArmsDealerLine::I_WANT_JOKE_ARTICLES);
    break;
  case ArmsDealerLine::HELP_POLICE:
    state_.initiate_arrest(ArrestReason::WeaponsDealing);
    break;
  default:
    std::cerr << "I don't know how to advance without choice from state "
              << current_line_ << "\n";
    return false;
    break;
  }
  return true;
}

bool ArmsDealerConversation::advance_line_choice(ArmsDealerLine choice) {
  // Do something with the choice
  switch (choice) {
  case ArmsDealerLine::I_WANT_JOKE_ARTICLES:
    current_line(ArmsDealerLine::JOKE_INTRO);
    break;
  case ArmsDealerLine::I_WANT_WEAPONS:
    current_line(ArmsDealerLine::HELP_POLICE);
    break;
  case ArmsDealerLine::INTERESTED_IN_NEW_GOODS:
    current_line(ArmsDealerLine::WHAT_KIND_OF_GOODS);
    break;
  case ArmsDealerLine::HAVE_ICE_CREAM:
    current_line(ArmsDealerLine::CHOOSE_FLAVOR);
    break;
  case ArmsDealerLine::SMELLY_CHEESE:
  case ArmsDealerLine::LAXATIVES:
  case ArmsDealerLine::RATS:
  case ArmsDealerLine::STINK_BOMB:
    selected_item_ = to_arms_dealer_item(choice);
    price_ = market_.purchase_price(selected_item_);
    current_line(ArmsDealerLine::JOKE_PRICE);
    break;
  case ArmsDealerLine::ICE_VANILLA:
  case ArmsDealerLine::ICE_CHOCOLATE:
  case ArmsDealerLine::ICE_STRAWBERRY:
  case ArmsDealerLine::ICE_LEMON:
    selected_item_ = to_arms_dealer_item(choice);
    price_ = market_.purchase_price(selected_item_);
    if (previous_line_ == ArmsDealerLine::CHOOSE_FLAVOR) {
      current_line(ArmsDealerLine::ONE_SCOOP_COSTS);
    }
    else if (previous_line_ == ArmsDealerLine::WHAT_KIND_OF_GOODS) {
      current_line(ArmsDealerLine::ILL_PAY);
    }
    break;
  case ArmsDealerLine::ONE_PIECE:
  case ArmsDealerLine::TWO_PIECE:
  case ArmsDealerLine::FIVE_PIECE:
  case ArmsDealerLine::TEN_PIECE:
  case ArmsDealerLine::TWENTY_PIECE: {
    const int pieces = piece_count(choice);

    // selling
    if (previous_line_ == ArmsDealerLine::HOW_MANY_SELL) {
      SaleResult sale = make_sale(pieces, selected_item_, price_);

      if (sale == SaleResult::NOT_ENOUGH_STOCK) {
        current_line(ArmsDealerLine::NOT_ENOUGH_WEAPONS);
      }
      else { // SUCCESS
        current_line(ArmsDealerLine::PLEASURE_DOING_BUSINESS);
      }
      break;
    }

    // buying
    PurchaseResult buy = make_purchase(pieces, selected_item_, price_);
    switch (buy) {
    case PurchaseResult::NOT_ENOUGH_MONEY:
      current_line(ArmsDealerLine::YOU_CANT_AFFORD_WHAT_A_SHAME);
      break;
    case PurchaseResult::NOW_AT_MAX:
      current_line(ArmsDealerLine::YOU_REACHED_MAX_ITEMS);
      break;
    case PurchaseResult::ALREADY_AT_MAX:
      current_line(ArmsDealerLine::ALREADY_AT_MAX_ITEMS);
      break;
    case PurchaseResult::SUCCESS: {
      if (previous_line_ == ArmsDealerLine::OK_HOW_MANY) {
        current_line(ArmsDealerLine::HAVE_FUN);
      }
      else {
        current_line(ArmsDealerLine::BE_CAREFUL_ICE_IS_HOT);
      }
      break;
    }
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

bool ArmsDealerConversation::advance_proposal(bool agree) {
  switch (current_line_) {
  case ArmsDealerLine::JOKE_PRICE:
    if (agree) {
      current_line(ArmsDealerLine::OK_HOW_MANY);
    }
    else {
      current_line(ArmsDealerLine::DONT_NEGOTIATE);
    }
    break;
  case ArmsDealerLine::ILL_PAY:
    if (agree) {
      current_line(ArmsDealerLine::HOW_MANY_SELL);
    }
    else {
      current_line(ArmsDealerLine::NO_HAGGLING_BUDDY);
    }
    break;
  case ArmsDealerLine::ONE_SCOOP_COSTS:
    if (agree) {
      current_line(ArmsDealerLine::HOW_MANY_SCOOPS);
    }
    else {
      current_line(ArmsDealerLine::DONT_NEGOTIATE);
    }
    break;
  default:
    // If there is no choice then just advance.
    return advance();
    break;
  }
  return true;
}

Emotion ArmsDealerConversation::emotion() const {
  switch (current_line_) {
  case ArmsDealerLine::NO_HAGGLING_BUDDY:
    return Emotion::ANGRY;
  default:
    return Emotion::NEUTRAL;
  }
}

std::vector<ConversationLine<ArmsDealerLine>>
ArmsDealerConversation::lines() const {
  switch (current_line_) {
  case ArmsDealerLine::WELCOME:
    return {
        make_line(ArmsDealerLine::WELCOME),
    };
    break;
  case ArmsDealerLine::HELP_POLICE:
    return {
        make_line(ArmsDealerLine::HELP_POLICE),
    };
    break;
  case ArmsDealerLine::SMELLY_CHEESE:
    return {
        make_option(ArmsDealerLine::SMELLY_CHEESE),
        make_option(ArmsDealerLine::LAXATIVES),
        make_option(ArmsDealerLine::RATS),
        make_option(ArmsDealerLine::STINK_BOMB),
    };
    break;
  case ArmsDealerLine::FLAME_THROWERS:
    return {
        make_option(ArmsDealerLine::FLAME_THROWERS),
        make_option(ArmsDealerLine::CHEMICALS),
        make_option(ArmsDealerLine::BAZOOKA),
        make_option(ArmsDealerLine::BOMB),
    };
    break;
  case ArmsDealerLine::I_WANT_JOKE_ARTICLES:
    return {
        make_option(ArmsDealerLine::I_WANT_JOKE_ARTICLES),
        make_option(ArmsDealerLine::I_WANT_WEAPONS),
        make_option(ArmsDealerLine::INTERESTED_IN_NEW_GOODS),
        make_option(ArmsDealerLine::HAVE_ICE_CREAM),
    };
    break;
  case ArmsDealerLine::ICE_VANILLA:
    return {
        make_option(ArmsDealerLine::ICE_VANILLA),
        make_option(ArmsDealerLine::ICE_CHOCOLATE),
        make_option(ArmsDealerLine::ICE_STRAWBERRY),
        make_option(ArmsDealerLine::ICE_LEMON),
    };
    break;
  case ArmsDealerLine::ONE_SCOOP_COSTS: {
    return {
        make_line(
            ArmsDealerLine::ONE_SCOOP_COSTS,
            {.is_proposal = true, .custom_text = compat::format("{}", price_)}),
    };
    break;
  }
  case ArmsDealerLine::ILL_PAY: {
    return {
        make_line(
            ArmsDealerLine::ILL_PAY,
            {.is_proposal = true, .custom_text = compat::format("{}", price_)}),
    };
    break;
  }
  case ArmsDealerLine::OK_HOW_MANY:
    return {
        make_line(ArmsDealerLine::OK_HOW_MANY),
    };
    break;
  case ArmsDealerLine::JOKE_INTRO:
    return {
        make_line(ArmsDealerLine::JOKE_INTRO),
    };
    break;
  case ArmsDealerLine::ONE_PIECE:
    return {
        make_option(ArmsDealerLine::ONE_PIECE),
        make_option(ArmsDealerLine::TWO_PIECE),
        make_option(ArmsDealerLine::FIVE_PIECE),
        make_option(ArmsDealerLine::TEN_PIECE),
        make_option(ArmsDealerLine::TWENTY_PIECE),
    };
    break;
  case ArmsDealerLine::DONT_NEGOTIATE:
    return {
        make_line(ArmsDealerLine::DONT_NEGOTIATE),
    };
    break;
  case ArmsDealerLine::HAVE_FUN:
    return {
        make_line(ArmsDealerLine::HAVE_FUN),
    };
    break;
  case ArmsDealerLine::YOU_CANT_AFFORD_WHAT_A_SHAME:
    return {
        make_line(ArmsDealerLine::YOU_CANT_AFFORD_WHAT_A_SHAME),
    };
    break;
  case ArmsDealerLine::WHAT_KIND_OF_GOODS:
    return {
        make_line(ArmsDealerLine::WHAT_KIND_OF_GOODS),
    };
    break;
  case ArmsDealerLine::CHOOSE_FLAVOR:
    return {
        make_line(ArmsDealerLine::CHOOSE_FLAVOR),
    };
    break;
  case ArmsDealerLine::HOW_MANY_SCOOPS:
    return {
        make_line(ArmsDealerLine::HOW_MANY_SCOOPS),
    };
    break;
  // This shouldn't be used, not sure why it was double in WAFFEN.e:
  case ArmsDealerLine::HOW_MANY_SCOOPS2:
    return {
        make_line(ArmsDealerLine::HOW_MANY_SCOOPS2),
    };
    break;
  case ArmsDealerLine::BE_CAREFUL_ICE_IS_HOT:
    return {
        make_line(ArmsDealerLine::BE_CAREFUL_ICE_IS_HOT),
    };
    break;
  case ArmsDealerLine::NO_HAGGLING_BUDDY:
    return {
        make_line(ArmsDealerLine::NO_HAGGLING_BUDDY),
    };
    break;
  case ArmsDealerLine::NOT_ENOUGH_WEAPONS:
    return {
        make_line(ArmsDealerLine::NOT_ENOUGH_WEAPONS),
    };
    break;
  case ArmsDealerLine::PLEASURE_DOING_BUSINESS:
    return {
        make_line(ArmsDealerLine::PLEASURE_DOING_BUSINESS),
    };
    break;
  case ArmsDealerLine::YOU_REACHED_MAX_ITEMS:
    return {
        make_line(ArmsDealerLine::YOU_REACHED_MAX_ITEMS),
    };
    break;
  case ArmsDealerLine::ALREADY_AT_MAX_ITEMS:
    return {
        make_line(ArmsDealerLine::ALREADY_AT_MAX_ITEMS),
    };
    break;
  case ArmsDealerLine::HOW_MANY_SELL:
    return {
        make_line(ArmsDealerLine::HOW_MANY_SELL),
    };
    break;
  case ArmsDealerLine::JOKE_PRICE: {
    return {
        make_proposal(ArmsDealerLine::JOKE_PRICE, compat::format("{}", price_)),
    };
    break;
  }
  default:
    std::cerr << "Get Lines Not yet implemented for line "
              << lines_.at(static_cast<int>(current_line_)) << "\n";
    break;
  }
  return {};
}

bool ArmsDealerConversation::has_ended() const {
  switch (current_line_) {
  case ArmsDealerLine::NO_HAGGLING_BUDDY:
    return true;
  default:
    return false;
  }
}

ArmsDealerLine
ArmsDealerConversation::compute_start_line(const GameState& /*state*/,
                                           const Actor* /*actor*/) {
  return ArmsDealerLine::WELCOME;
}

constexpr core::item::Item
ArmsDealerConversation::to_arms_dealer_item(ArmsDealerLine line) const {
  using A = ArmsDealerLine;
  using core::item::Item;
  switch (line) {
  case A::SMELLY_CHEESE:
    return Item::SmellyCheese;
  case A::LAXATIVES:
    return Item::Laxatives;
  case A::RATS:
    return Item::Rats;
  case A::STINK_BOMB:
    return Item::StinkBomb;
  case A::ICE_VANILLA:
    return Item::FlameThrower;
  case A::ICE_CHOCOLATE:
    return Item::Chemicals;
  case A::ICE_STRAWBERRY:
    return Item::Bazooka;
  case A::ICE_LEMON:
    return Item::Bomb;
  default:
    assert(!"non-item ArmsDealerLine");
    return Item::Count; // unreachable
  }
}

constexpr int ArmsDealerConversation::piece_count(ArmsDealerLine l) {
  switch (l) {
  case ArmsDealerLine::ONE_PIECE:
    return 1;
  case ArmsDealerLine::TWO_PIECE:
    return 2;
  case ArmsDealerLine::FIVE_PIECE:
    return 5;
  case ArmsDealerLine::TEN_PIECE:
    return 10;
  case ArmsDealerLine::TWENTY_PIECE:
    return 20;
  default:
    return 0;
  }
}

// enum class PurchaseResult { SUCCESS, NOT_ENOUGH_MONEY, MAX_255, };
ArmsDealerConversation::PurchaseResult
ArmsDealerConversation::make_purchase(int requested, core::item::Item id,
                                      Money price_per) {

  assert(requested > 0);

  Player& player = *state_.active_player();
  core::inventory::Inventory& inv = player.inventory();

  const int current = inv.count(id);
  const int free_room = std::max(0, inv.max_per_item(id) - current);
  const int to_buy = std::min(requested, free_room);

  // Make sure they have room:
  if (to_buy == 0) {
    return PurchaseResult::ALREADY_AT_MAX;
  }

  const Money planned_cost = price_per * to_buy;

  if (player.cash_balance() < planned_cost) {
    return PurchaseResult::NOT_ENOUGH_MONEY;
  }

  // Just making sure we're adding something:
  const Money cost_for_requested = price_per * to_buy;
  const int actually_added = inv.add_with_cost(id, to_buy, cost_for_requested);
  if (actually_added == 0) {
    return PurchaseResult::ALREADY_AT_MAX;
  }

  // Pay only for what really went in.
  const Money actual_cost = price_per * actually_added;
  // Don't have to check return value because we already checked if we have
  // enough money
  (void)player.spend_cash(state_.time().current_date(), Account::Weapons,
                          actual_cost);

  // Underworld score: total_cost / 500
  const int delta = static_cast<int>(actual_cost.whole() / 500);
  if (delta > 0) {
    state_.adjust_underworld_score(player, delta);
  }

  return (actually_added == requested) ? PurchaseResult::SUCCESS
                                       : PurchaseResult::NOW_AT_MAX;
}

// enum class SaleResult { SUCCESS, NOT_ENOUGH_STOCK, };
ArmsDealerConversation::SaleResult
ArmsDealerConversation::make_sale(int requested, core::item::Item id,
                                  Money price_per) {
  assert(requested > 0);

  Player& player = *state_.active_player();
  core::inventory::Inventory& inv = player.inventory();

  const int in_stock = inv.count(id);
  if (in_stock < requested) {
    return SaleResult::NOT_ENOUGH_STOCK;
  }

  const Money cost_basis = inv.remove_with_cost(id, requested);

  const GameDate date = state_.time().current_date();
  const Money proceeds = price_per * requested;

  player.financials().transfer(date, Account::Weapons, Account::Cash, proceeds,
                               AccountingScope::Operational);

  // Underworld score: profit / 100
  const Money profit = proceeds - cost_basis;
  if (profit > 0_usd) {
    const int delta = static_cast<int>(profit.whole() / 100);
    if (delta > 0) {
      state_.adjust_underworld_score(player, delta);
    }
    player.add_weapon_dealing_profit(profit);

    if (player.weapon_dealing_profit() >= 3000_usd) {
      state_.message_manager().schedule_message(
          core::events::LeventId::YouHaveMadeOver3KProfitThroughWeaponDeals,
          player.id(), date);
    }
  }

  return SaleResult::SUCCESS;
}
