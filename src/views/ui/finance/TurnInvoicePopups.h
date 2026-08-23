/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "core/finance/TurnInvoices.h"

#include <functional>
#include <vector>

class GameContext;
class GameState;
class Restaurant;

// Popup builders for the periodic invoices shown around the turn boundary.
// Amounts come from core/finance/TurnInvoices.h; each popup runs the
// continuation it is given when the player dismisses it.
namespace views::ui::finance {

void show_staff_invoice_popup(GameContext& context, GameState& gs,
                              core::RestaurantId restaurant_id,
                              std::function<void()> after);

void show_electricity_slot_invoice_popup(
    GameContext& context, GameState& gs,
    const core::finance::ElectricitySlotInvoice& invoice,
    std::function<void()> after);

// Monthly costs (loans + rent) for a player.
void show_monthly_invoice_popup(GameContext& context, GameState& gs,
                                core::PlayerId player_id,
                                const core::finance::MonthlyCosts& costs,
                                std::function<void()> on_done = {});

// Weekly profit of all non-active cities (Saturday only).
void show_weekly_city_invoice_popup(GameContext& context, GameState& gs,
                                    core::PlayerId player_id,
                                    std::function<void()> on_done = {});

// Push the end-of-week takings screens for the previous player's active-city
// restaurants, chained into the weekly city profit invoice; interleaves the
// electricity invoice per restaurant at end of month.
void push_weekly_restaurant_reports(GameContext& context, GameState& gs,
                                    const std::vector<Restaurant*>& restaurants,
                                    core::PlayerId previous_player_id,
                                    std::function<void()> on_done = {},
                                    bool interleave_electricity = false);

} // namespace views::ui::finance
