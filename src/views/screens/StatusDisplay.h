/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/finance/Money.h"
#include "core/status/StatusHub.h"
#include "views/base/PanelView.h"

class StatusDisplay : public PanelView {
public:
  StatusDisplay(GameContext& context, GameState& state, StatusHub& hub);
  void tick() override;
  void render(const RenderContext& ctx) override;

  // Snapshot or release the header display (date, time, money).
  // capture_state() records current values and locks them; calling it again
  // refreshes the snapshot while staying locked.
  void capture_state();
  void release_state();

  // Attack mode
  void set_attack_status(std::optional<int> attacker_energy,
                         std::optional<int> defender_energy);
  void clear_attack_status();
  void set_police_coming();
  void set_attack_contamination(std::optional<int> contamination_pct);

private:
  void clear(const RenderContext& ctx);
  void render_header_info(const RenderContext& ctx);
  void render_attack_bars(const RenderContext& ctx);
  void recipe(StatusDisplayRecipe r);
  void render_recipe(const StatusDisplayRecipe& recipe,
                     const RenderContext& ctx);

  StatusDisplayRecipe recipe_;
  std::optional<int> previous_time_;
  std::optional<Money> previous_balance_;
  StatusHub& hub_;
  StatusHub::Subscription sub_;

  // Frozen header state
  bool frozen_{false};
  std::string frozen_date_;
  std::string frozen_time_;
  std::string frozen_money_;

  bool attack_mode_{false};
  std::optional<int> attack_attacker_energy_;
  std::optional<int> attack_defender_energy_;
  std::optional<int> attack_contamination_;
  bool police_coming_{false};
};
