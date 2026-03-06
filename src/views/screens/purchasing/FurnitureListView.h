/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/restaurant/Furniture.h"
#include "core/restaurant/FurnitureBatch.h"
#include "resources/Bob.h"
#include "views/screens/purchasing/CategoryOverviewScreen.h"

class FurnitureListView : public CategoryOverviewScreen {

public:
  FurnitureListView(GameContext& context, GameState& state, SupplierID s_id);
  void tick() override;
  void render(const RenderContext& ctx) override;

protected:
  void draw_page_screen(const RenderContext& ctx) override;
  SupplierType supplier_type() const override;

private:
  void draw_furniture_list(const RenderContext& ctx);
  void draw_furniture_preview(const RenderContext& ctx,
                              const Furniture& furniture, int y);
  void handle_furniture_selection(int furniture_id);
  void show_invoice();
  void draw_page_header(const RenderContext& ctx) override;
  void set_up_input_panel() override;

  const Supplier* supplier_;
  std::vector<FurnitureBatch> furniture_;
  int current_page_{0};
  std::vector<std::size_t> page_starts_;
  Bob& furniture_bob_;
  Bob& kitchen_bob_;
  Surface green_purchase_mark_;
};
