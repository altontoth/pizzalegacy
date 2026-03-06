/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/save/SaveGameCharacter.h"
#include <ctime>
#include "core/character/Player.h"
#include "core/state/GameState.h"
#include "core/util/GameContext.h"
#include "factories/CharacterFactory.h"

namespace save {

json character_to_json(const core::character::Character& character) {
  json j;
  j["id"] = character.id();
  // Save character name for debugging/display
  j["first_name"] = character.first_name();
  j["last_name"] = character.last_name();
  // Save dynamic values that can change
  j["age"] = character.age();
  j["health"] = character.health();
  j["energy"] = character.energy();
  j["experience"] = character.experience();
  j["cooking"] = character.cooking();
  j["serving"] = character.serving();
  j["management"] = character.management();
  j["motivation"] = character.motivation();
  j["pizzas_per_day"] = character.pizzas_per_day();
  return j;
}

core::character::Character json_to_character(const json& j) {
  uint8_t char_id = j["id"].get<uint8_t>();
  auto* char_data = CharacterFactory::create_character(char_id);
  core::character::Character character(char_data);

  // Restore dynamic values
  if (j.contains("age")) {
    character.age(j["age"].get<uint8_t>());
  }
  if (j.contains("health")) {
    character.health(j["health"].get<uint8_t>());
  }
  if (j.contains("energy")) {
    character.energy(j["energy"].get<uint8_t>());
  }
  if (j.contains("cooking")) {
    character.cooking(j["cooking"].get<uint8_t>());
  }
  if (j.contains("serving")) {
    character.serving(j["serving"].get<uint8_t>());
  }
  if (j.contains("management")) {
    character.management(j["management"].get<uint8_t>());
  }
  if (j.contains("motivation")) {
    character.motivation(j["motivation"].get<uint8_t>());
  }
  if (j.contains("pizzas_per_day")) {
    character.pizzas_per_day(j["pizzas_per_day"].get<int>());
  }

  return character;
}

json player_to_json(const Player& player) {
  json j;
  j["id"] = player.id().value;
  j["name"] = player.name();
  j["jail_time"] = player.jail_time();
  j["human_controlled"] = player.is_human_controlled();
  j["character"] = character_to_json(player.character());

  // Save recipes
  j["recipes"] = player_recipes_to_json(player.recipes());

  // Save financials (including ledger)
  j["financials"] = player_financials_to_json(player.financials());

  j["start_of_day_hour"] = player.start_of_day_hour();
  if (player.holiday_days_left().has_value()) {
    j["holiday_days_left"] = *player.holiday_days_left();
  }

  // Save AI menu slots (only meaningful for AI players, but always written for
  // simplicity)
  json ai_menu_array = json::array();
  for (const auto& slot : player.ai_menu_slots()) {
    json slot_j;
    slot_j["recipe_id"] = slot.recipe_id;
    slot_j["production_counter"] = slot.production_counter;
    slot_j["max_production"] = slot.max_production;
    json history_array = json::array();
    for (int value : slot.history) {
      history_array.push_back(value);
    }
    slot_j["history"] = history_array;
    ai_menu_array.push_back(slot_j);
  }
  j["ai_menu_slots"] = ai_menu_array;

  // Save loans (map<CityId, vector<Loan>>)
  json loans_obj = json::object();
  for (const auto& [city_id, city_loans] : player.loans()) {
    json loans_array = json::array();
    for (const auto& loan : city_loans) {
      loans_array.push_back(loan_to_json(loan));
    }
    loans_obj[std::to_string(city_id.value)] = loans_array;
  }
  j["loans"] = loans_obj;

  j["inventory"] = inventory_to_json(player.inventory());

  return j;
}

std::unique_ptr<Player> json_to_player(const json& j, GameContext& context) {
  core::PlayerId pid{j["id"].get<int>()};
  bool human{j["human_controlled"].get<bool>()};
  auto character = json_to_character(j["character"]);

  auto player = std::make_unique<Player>(pid, character, human);
  player->name(j["name"].get<std::string>());
  player->jail_time(j["jail_time"].get<uint8_t>());

  // Restore recipes
  if (j.contains("recipes")) {
    player->recipes() = json_to_player_recipes(j["recipes"]);
  }

  // Restore financials (including ledger)
  if (j.contains("financials")) {
    player->financials() = json_to_player_financials(j["financials"]);
  }

  if (j.contains("start_of_day_hour")) {
    player->start_of_day_hour(j["start_of_day_hour"].get<uint8_t>());
  }
  if (j.contains("holiday_days_left")) {
    player->holiday(j["holiday_days_left"].get<int>());
  }

  // Restore AI menu slots
  if (j.contains("ai_menu_slots")) {
    auto& slots = player->ai_menu_slots();
    int i = 0;
    for (const auto& slot_j : j["ai_menu_slots"]) {
      if (i >= static_cast<int>(slots.size())) {
        break;
      }
      slots[i].recipe_id = slot_j.value("recipe_id", 0);
      slots[i].production_counter = slot_j.value("production_counter", 0);
      slots[i].max_production = slot_j.value("max_production", 0);
      if (slot_j.contains("history")) {
        int hi = 0;
        for (const auto& val_j : slot_j["history"]) {
          if (hi >= static_cast<int>(slots[i].history.size())) {
            break;
          }
          slots[i].history[hi++] = val_j.get<int>();
        }
      }
      ++i;
    }
  }

  // Restore loans
  if (j.contains("loans")) {
    for (auto it = j["loans"].begin(); it != j["loans"].end(); ++it) {
      core::CityId city_id{std::stoi(it.key())};
      for (const auto& loan_j : it.value()) {
        Loan loan = json_to_loan(loan_j);
        player->loans_in_city(city_id).push_back(loan);
      }
    }
  }

  // Restore inventory into existing object (Inventory is not copy-assignable)
  if (j.contains("inventory")) {
    load_inventory(j["inventory"], player->inventory());
  }

  return player;
}

json player_recipes_to_json(const core::food::PlayerRecipes& recipes) {
  using namespace core::food;

  json j;
  j["next_id"] = recipes.next_id.value;

  // Save canonical recipes
  json canonical_array = json::array();
  for (size_t i = 0; i < recipes.canon.size(); ++i) {
    const auto& slot = recipes.canon[i];
    if (slot.created()) {
      canonical_array.push_back(recipe_to_json(*slot.recipe));
    }
    else {
      canonical_array.push_back(nullptr);
    }
  }
  j["canonical"] = canonical_array;

  // Save user recipes
  json user_array = json::array();
  for (const auto& recipe : recipes.user) {
    user_array.push_back(recipe_to_json(recipe));
  }
  j["user"] = user_array;

  return j;
}

core::food::PlayerRecipes json_to_player_recipes(const json& j) {
  using namespace core::food;

  PlayerRecipes recipes;
  recipes.next_id = core::RecipeId{j["next_id"].get<int>()};

  // Restore canonical recipes
  if (j.contains("canonical")) {
    for (size_t i = 0; i < j["canonical"].size() && i < recipes.canon.size();
         ++i) {
      if (!j["canonical"][i].is_null()) {
        auto recipe = json_to_recipe(j["canonical"][i]);
        recipes.canon[i] = CanonicalSlot{std::move(recipe)};
      }
    }
  }

  // Restore user recipes
  if (j.contains("user")) {
    for (const auto& recipe_json : j["user"]) {
      recipes.user.push_back(json_to_recipe(recipe_json));
    }
  }

  return recipes;
}

json inventory_to_json(const core::inventory::Inventory& inventory) {
  json j = json::object();
  for (std::size_t i = 0; i < core::item::item_count; ++i) {
    auto item = static_cast<core::item::Item>(i);
    int qty = inventory[item];
    if (qty > 0) {
      j[std::string(core::item::to_string(item))] = qty;
    }
  }
  return j;
}

void load_inventory(const json& j, core::inventory::Inventory& inventory) {
  for (std::size_t i = 0; i < core::item::item_count; ++i) {
    auto item = static_cast<core::item::Item>(i);
    auto key = std::string(core::item::to_string(item));
    if (j.contains(key)) {
      inventory[item] = j[key].get<int>();
    }
  }
}

json loan_to_json(const Loan& loan) {
  json j;
  j["bank_id"] = loan.bank_id().value;
  j["start_amount"] = loan.start_amount().cents;
  j["interest_percentage"] = loan.interest_percentage();
  j["start_months"] = loan.start_months();
  j["months_left"] = loan.months_left();
  j["monthly_principal"] = loan.monthly_principal().cents;
  j["monthly_interest"] = loan.monthly_interest().cents;
  return j;
}

Loan json_to_loan(const json& j) {
  return Loan::from_save(
      core::BankId{j["bank_id"].get<int32_t>()},
      Money{j["start_amount"].get<int64_t>()},
      j["interest_percentage"].get<uint8_t>(), j["start_months"].get<int>(),
      j["months_left"].get<int>(), Money{j["monthly_principal"].get<int64_t>()},
      Money{j["monthly_interest"].get<int64_t>()});
}

json posting_to_json(const Posting& posting) {
  json j;
  j["account"] = static_cast<int>(posting.account);
  j["amount"] = posting.amount.cents;
  if (posting.restaurant_id.has_value()) {
    j["restaurant_id"] = posting.restaurant_id.value().value;
  }
  else {
    j["restaurant_id"] = nullptr;
  }
  j["scope"] = static_cast<int>(posting.scope);
  return j;
}

Posting json_to_posting(const json& j) {
  Account account = static_cast<Account>(j["account"].get<int>());
  Money amount{j["amount"].get<int64_t>()};
  std::optional<core::RestaurantId> restaurant_id = std::nullopt;

  if (j.contains("restaurant_id") && !j["restaurant_id"].is_null()) {
    restaurant_id = core::RestaurantId{j["restaurant_id"].get<int32_t>()};
  }

  AccountingScope scope = AccountingScope::Operational;
  if (j.contains("scope")) {
    scope = static_cast<AccountingScope>(j["scope"].get<int>());
  }

  return Posting{account, amount, restaurant_id, scope};
}

json journal_entry_to_json(const JournalEntry& entry) {
  json j;

  // Save date
  j["date"] = {{"year", entry.date.year()},
               {"month", entry.date.month()},
               {"day", entry.date.day()}};

  // Save postings
  json postings_array = json::array();
  for (const auto& posting : entry.lines) {
    postings_array.push_back(posting_to_json(posting));
  }
  j["lines"] = postings_array;

  return j;
}

JournalEntry json_to_journal_entry(const json& j) {
  // Initialize with default date
  GameDate date{1995, 1, 1};

  // Restore date
  if (j.contains("date")) {
    int year = j["date"]["year"].get<int>();
    unsigned month = j["date"]["month"].get<unsigned>();
    unsigned day = j["date"]["day"].get<unsigned>();
    date = GameDate{year, static_cast<int>(month), static_cast<int>(day)};
  }

  JournalEntry entry{date, {}};

  // Restore postings
  if (j.contains("lines")) {
    for (const auto& posting_json : j["lines"]) {
      entry.lines.push_back(json_to_posting(posting_json));
    }
  }

  return entry;
}

json ledger_to_json(const Ledger& ledger) {
  json j;

  // Access journal through reflection (Ledger doesn't have a public getter)
  // We'll need to add a getter or friend this function
  // For now, we'll use the friend approach via PlayerFinancials

  return j;
}

Ledger json_to_ledger(const json& j) {
  Ledger ledger;
  return ledger;
}

json player_financials_to_json(const PlayerFinancials& financials) {
  json j;

  // Get all account balances
  json balances = json::object();

  // Save all accounts
  const std::vector<Account> all_accounts = {
      Account::Sales,     Account::CoinMachines,  Account::Missions,
      Account::Staff,     Account::Rents,         Account::Stock,
      Account::Machines,  Account::Furniture,     Account::Protection,
      Account::Repayment, Account::Others,        Account::Cash,
      Account::Capital,   Account::ProfitAndLoss, Account::Loan,
      Account::Property,  Account::Weapons,       Account::FinesAndBribes,
      Account::Debts,     Account::DeliveryDebts, Account::Cheat};

  for (Account account : all_accounts) {
    Money balance = financials.balance(account);
    balances[to_string(account)] = balance.cents;
  }

  j["balances"] = balances;

  // TODO: We need to store the full ledger

  return j;
}

PlayerFinancials json_to_player_financials(const json& j) {
  PlayerFinancials financials;

  // Restore account balances by creating synthetic journal entries
  if (j.contains("balances")) {
    GameDate load_date{1995, 1, 1}; // Use a base date for initial balances

    for (auto it = j["balances"].begin(); it != j["balances"].end(); ++it) {
      int64_t balance_cents = it.value().get<int64_t>();

      if (balance_cents != 0) {
        // Find the account enum from string
        std::string account_name = it.key();
        auto account_opt = string_to_account(account_name);

        if (!account_opt.has_value()) {
          // Unknown account name, skip it
          continue;
        }

        Account account = account_opt.value();

        // Transfer from Capital to restore the balance
        if (balance_cents > 0) {
          financials.transfer(load_date, Account::Capital, account,
                              Money{balance_cents}, AccountingScope::Capital);
        }
        else {
          financials.transfer(load_date, account, Account::Capital,
                              Money{-balance_cents}, AccountingScope::Capital);
        }
      }
    }
  }

  return financials;
}

} // namespace save
