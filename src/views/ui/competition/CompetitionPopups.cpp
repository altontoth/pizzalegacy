/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/competition/CompetitionPopups.h"

#include <algorithm>
#include <string>
#include <utility>

#include "core/character/Player.h"
#include "core/state/GameState.h"
#include "core/util/GameContext.h"
#include "engine/FormatCompat.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/popups/NumberInputPopup.h"

namespace views::ui::competition {

namespace {
constexpr int min_stake_dollars = 500;
constexpr int max_stake_dollars = 10000;
// Stakes are placed in steps of the minimum stake.
constexpr int stake_step_dollars = min_stake_dollars;
} // namespace

void ask_monthly_entry(
    GameContext& context, GameState& gs, core::PlayerId pid, bool ask_speed,
    std::function<void(std::optional<MonthlyCompetitionEntry>)> reply) {
  auto* p = gs.player(pid);
  if (!p) {
    reply(std::nullopt);
    return;
  }
  std::string player_name = p->character().name();

  context.popup->create<ConfirmPopup>(
      compat::format("{}, do you want to take part in the pizza competition?",
                     player_name),
      [ctx = &context, state = &gs, pid, ask_speed,
       reply = std::move(reply)](bool accepted) mutable {
        if (!accepted) {
          reply(std::nullopt);
          return;
        }

        auto* p2 = state->player(pid);
        if (!p2) {
          reply(std::nullopt);
          return;
        }
        int cash_dollars = static_cast<int>(p2->cash_balance().cents / 100);
        int max_bet =
            std::min(max_stake_dollars,
                     (cash_dollars / stake_step_dollars) * stake_step_dollars);
        if (max_bet < min_stake_dollars) {
          reply(std::nullopt);
          return;
        }

        ctx->popup->create<NumberInputPopup>(
            "How much do you stake?", "$", min_stake_dollars, min_stake_dollars,
            max_bet, stake_step_dollars,
            [ctx, ask_speed,
             reply = std::move(reply)](int bet_dollars) mutable {
              if (!ask_speed) {
                reply(MonthlyCompetitionEntry{.bet_dollars = bet_dollars});
                return;
              }
              // The first accepting player also picks the speed.
              ctx->popup->create<NumberInputPopup>(
                  "Speed:", "", 3, 1, 5, 1,
                  [reply = std::move(reply),
                   bet_dollars](int speed_level) mutable {
                    reply(MonthlyCompetitionEntry{.bet_dollars = bet_dollars,
                                                  .speed_level = speed_level});
                  });
            });
      });
}

} // namespace views::ui::competition
