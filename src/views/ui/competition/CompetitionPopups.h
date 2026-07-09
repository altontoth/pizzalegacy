/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <optional>

#include "core/common/Ids.h"
#include "core/state/CompetitionCoordinator.h"

class GameContext;
class GameState;

namespace views::ui::competition {

// Popup chain asking one player to enter the monthly pizza competition:
// participation ConfirmPopup -> stake NumberInputPopup -> speed
// NumberInputPopup (first accepting player only). Registered on GameState as
// the monthly entry presenter. reply(nullopt) means the player declined or
// cannot afford the minimum stake.
void ask_monthly_entry(
    GameContext& context, GameState& gs, core::PlayerId pid, bool ask_speed,
    std::function<void(std::optional<MonthlyCompetitionEntry>)> reply);

} // namespace views::ui::competition
