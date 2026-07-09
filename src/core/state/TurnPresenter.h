/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"

#include <functional>
#include <string>
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
  virtual void show_electricity_slot_invoice(core::RestaurantId restaurant_id,
                                             std::function<void()> on_done) = 0;

  // Monthly loans and rent invoice for a player.
  virtual void show_monthly_invoice(core::PlayerId player_id) = 0;
};
