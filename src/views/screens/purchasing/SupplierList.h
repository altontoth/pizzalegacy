/*
 *
 * SPDX-FileCopyrightText: 2016 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "views/helpers/SupplierListRenderer.h"
#include "views/screens/purchasing/CategoryOverviewScreen.h"

#include <optional>

class SupplierList : public CategoryOverviewScreen {
public:
  SupplierList(GameContext& context, GameState& state, SupplierType type);
  void tick() override;
  void render(const RenderContext& ctx) override;

protected:
  void draw_page_screen(const RenderContext& ctx) override;
  void set_up_input_panel() override;

private:
  std::optional<SupplierListRenderer<Supplier>> renderer_;
  std::size_t current_page_{0};
};
