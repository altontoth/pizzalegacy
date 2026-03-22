/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <nlohmann/json.hpp>
#include "core/character/Character.h"
#include "core/character/MafiaPlayerState.h"
#include "core/character/Player.h"
#include "core/save/SaveGameFood.h"

class GameState;
class GameContext;

namespace save {

using json = nlohmann::json;

json character_to_json(const core::character::Character& character);
core::character::Character json_to_character(const json& j);

json player_to_json(const Player& player);
std::unique_ptr<Player> json_to_player(const json& j, GameContext& context);

json player_recipes_to_json(const core::food::PlayerRecipes& recipes);
core::food::PlayerRecipes json_to_player_recipes(const json& j);

// Inventory serialization
json inventory_to_json(const core::inventory::Inventory& inventory);
void load_inventory(const json& j, core::inventory::Inventory& inventory);

// Loan serialization
json loan_to_json(const Loan& loan);
Loan json_to_loan(const json& j);

// Mafia state serialization
json mafia_state_to_json(const MafiaPlayerState& state);
MafiaPlayerState json_to_mafia_state(const json& j);

// Ledger/financials serialization
json posting_to_json(const Posting& posting);
Posting json_to_posting(const json& j);

json journal_entry_to_json(const JournalEntry& entry);
JournalEntry json_to_journal_entry(const json& j);

json ledger_to_json(const Ledger& ledger);
Ledger json_to_ledger(const json& j);

json player_financials_to_json(const PlayerFinancials& financials);
PlayerFinancials json_to_player_financials(const json& j);

} // namespace save
