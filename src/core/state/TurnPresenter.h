/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "core/finance/TurnInvoices.h"
#include "core/time/GameDate.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

class Restaurant;
class RawRenderable;

// Port for the popups shown around the turn boundary. The turn logic drives
// these without depending on view types; the engine registers a concrete
// implementation at startup. Each call runs a continuation when its popup is
// dismissed so the turn pipeline can resume.
class TurnPresenter {
public:
  virtual ~TurnPresenter() = default;

  // Manager "call me" alert with the manager's portrait.
  virtual void show_manager_alert(std::string message,
                                  const RawRenderable* portrait,
                                  std::function<void()> on_done) = 0;

  // End-of-week takings screens for the previous player's restaurants. When
  // interleave_electricity is set the per-restaurant electricity invoice is
  // folded in and on_done resumes the pipeline once everything is dismissed.
  virtual void show_weekly_reports(const std::vector<Restaurant*>& restaurants,
                                   core::PlayerId previous_player,
                                   std::function<void()> on_done,
                                   bool interleave_electricity) = 0;

  // Per-restaurant electricity/slot invoice at end of month.
  virtual void show_electricity_slot_invoice(
      const core::finance::ElectricitySlotInvoice& invoice,
      std::function<void()> on_done) = 0;

  // Monthly loans and rent invoice for a player.
  virtual void show_monthly_invoice(core::PlayerId player_id,
                                    const core::finance::MonthlyCosts& costs,
                                    std::function<void()> on_done) = 0;

  // The steps queued behind the turn card keep running while it is being shown.
  virtual void show_player_turn_card(core::PlayerId player,
                                     const GameDate& date,
                                     std::function<void()> on_closed) = 0;

  virtual void tick() {}
};

// When there is no presenter registered we use a NoTurnPresenter instead so the
// turn can still finish without having popups. It never builds the end-of-week
// report chain, so it posts that week's money itself.
class NoTurnPresenter : public TurnPresenter {
public:
  explicit NoTurnPresenter(GameState& gs) : gs_(gs) {}

  void show_manager_alert(std::string /*message*/,
                          const RawRenderable* /*portrait*/,
                          std::function<void()> on_done) override {
    on_done();
  }
  void show_weekly_reports(const std::vector<Restaurant*>& restaurants,
                           core::PlayerId previous_player,
                           std::function<void()> on_done,
                           bool interleave_electricity) override {
    core::finance::apply_weekly_settlement(gs_, previous_player, restaurants,
                                           interleave_electricity);
    if (on_done) {
      on_done();
    }
  }
  void show_electricity_slot_invoice(
      const core::finance::ElectricitySlotInvoice& /*invoice*/,
      std::function<void()> on_done) override {
    on_done();
  }
  void show_monthly_invoice(core::PlayerId /*player_id*/,
                            const core::finance::MonthlyCosts& /*costs*/,
                            std::function<void()> on_done) override {
    on_done();
  }
  // Held until the next tick so the steps queued behind the card run first.
  void show_player_turn_card(core::PlayerId /*player*/,
                             const GameDate& /*date*/,
                             std::function<void()> on_closed) override {
    pending_close_ = std::move(on_closed);
  }
  void tick() override {
    if (auto callback = std::exchange(pending_close_, nullptr)) {
      callback();
    }
  }

private:
  GameState& gs_;
  std::function<void()> pending_close_;
};
