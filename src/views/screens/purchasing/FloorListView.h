/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "views/screens/purchasing/CategoryOverviewScreen.h"

class FloorListView : public CategoryOverviewScreen {
public:
  FloorListView(GameContext& context, GameState& state, SupplierID id);

  void tick() override;
  void render(const RenderContext& ctx) override;
  void set_up_input_panel() override;

protected:
  void draw_page_screen(const RenderContext& ctx) override;
  void draw_page_header(const RenderContext& ctx) override;
  SupplierType supplier_type() const override;
  void back_out() override;

private:
  void charge_exit();
  void draw_flooring_page(const RenderContext& ctx) override;
  void draw_flooring_list(const RenderContext& ctx);

  const Supplier* supplier_{nullptr};
  std::span<const FloorPattern> patterns_;
  int current_page_{0};
  std::vector<int> page_starts_;

  std::optional<int> selected_index_;
  Surface green_purchase_mark_;
};
