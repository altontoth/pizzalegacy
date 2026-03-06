/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include "core/item/Item.h"
#include "core/state/UIState.h"
#include "views/base/MainPanelView.h"
#include "views/ui/status/StatusController.h"

namespace views::screens::depot {

class DepotWeaponInterface : public MainPanelView {
public:
  DepotWeaponInterface(GameContext& context, GameState& state,
                       DepotWeaponInterfaceState dwis);
  void tick() override;
  void render(const RenderContext& ctx) override;
  virtual void set_up_input_panel();
  StatusController status_controller_;

private:
  Property* property_{nullptr};
  std::optional<core::item::Item> hovered_weapon_;
};

} // namespace views::screens::depot
