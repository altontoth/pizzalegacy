/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

class Restaurant;
class Player;
class GameState;

namespace ai {

// Furniture pieces an AI restaurant opens with.
inline constexpr int opening_furniture_count = 20;

// Initialize AI player's 10 menu slots with random recipes and production
// values. Called once during game initialization. Matches original
// ai_init_menu.
void init_ai_menu(Player& player);

// Daily update of AI menu slot history buffers. Called from end_of_day().
void update_ai_menu(Player& player, int restaurant_count);

// Simplified AI revenue/cost calculation.
// AI restaurants don't go through the full order fulfillment pipeline;
// instead they earn revenue proportional to customer demand and their
// competitive strength (the original game's dual-economy model).
void process_restaurant_revenue(Restaurant& restaurant, Player& player);

// Daily chance for an AI restaurant to add a few more of its layout's items.
void invest_in_restaurant(Restaurant& restaurant, Player& player);

// A light attack is the stink bomb. A heavy one is always turned away by the
// victim's mafia protection, and still uses up the day's one attempt in that
// city.
enum class AttackSeverity { None, Light, Heavy };

// Daily chance for an AI to have a competitor's restaurant stink bombed. The
// target is whichever restaurant is costing this one the most business
// (humans only).
AttackSeverity attack_competitor(Restaurant& restaurant, Player& player,
                                 GameState& game_state);

void run_daily_evaluation(Player& player, GameState& game_state);

// Original aiEvaluateSingleRestaurant bankruptcy check:
// If profit_days < -50 AND competitor_count > 3 AND two courage rolls pass,
// close the restaurant. Returns true if restaurant was closed.
bool check_bankruptcy(Restaurant& restaurant, Player& player,
                      GameState& game_state);

} // namespace ai
