/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/state/TurnPresenter.h"

class GameContext;
class GameState;

namespace views::ui {

// Concrete TurnPresenter that renders the turn-boundary popups. Registered on
// the GameState at startup so the turn logic can stay free of view types.
class TurnPresenterView : public TurnPresenter {
public:
  TurnPresenterView(GameContext& context, GameState& gs)
      : context_(context), gs_(gs) {}

  void show_manager_alert(std::string message, const RawRenderable* portrait,
                          std::function<void()> on_done) override;
  void show_weekly_reports(const std::vector<Restaurant*>& restaurants,
                           core::PlayerId previous_player,
                           std::function<void()> on_done,
                           bool interleave_electricity) override;
  void show_electricity_slot_invoice(
      const core::finance::ElectricitySlotInvoice& invoice,
      std::function<void()> on_done) override;
  void show_monthly_invoice(core::PlayerId player_id,
                            const core::finance::MonthlyCosts& costs,
                            std::function<void()> on_done) override;
  void show_winner_announcement(const MayorAwardEvent& award,
                                std::function<void()> on_done) override;
  void show_player_turn_card(core::PlayerId player, const GameDate& date,
                             std::function<void()> on_closed) override;

private:
  GameContext& context_;
  GameState& gs_;
};

} // namespace views::ui
