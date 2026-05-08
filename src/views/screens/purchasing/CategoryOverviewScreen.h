/*
 * SPDX-FileCopyrightText: 2016 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/restaurant/FloorPattern.h"
#include "core/supply/Supplier.h"
#include "views/screens/purchasing/PurchasingMain.h"
#include "views/ui/status/StatusController.h"

#include <string>
#include <vector>
#include "engine/geom/Spaces.h"

class CategoryOverviewScreen : public PurchasingMain {
public:
  CategoryOverviewScreen(GameContext& context, GameState& state,
                         SupplierType stype);
  void tick() override;
  void render(const RenderContext& ctx) override;

protected:
  virtual void draw_page_header(const RenderContext& ctx);
  virtual void draw_page_screen(const RenderContext& ctx);
  void set_up_input_panel() override;
  void navigate_to_restaurant(core::RestaurantId new_id) override;
  Surface create_floor_preview(const FloorPattern&, int, int);
  void draw_icon(const RenderContext& ctx);
  virtual SupplierType supplier_type() const;

  std::map<SupplierType, std::string> category_titles;
  std::map<SupplierType, ViewRect> category_offsets;
  SupplierType supplier_type_;

private:
  struct InfoField {
    std::string text;
    // offset relative to the base x-position:
    int x_offset = 0;
  };

  struct InfoLine {
    std::optional<InfoField> label;
    InfoField value;
    std::optional<InfoField> post_fix;
  };

  virtual void draw_flooring_page(const RenderContext& ctx);
  void draw_furniture_page(const RenderContext& ctx);
  void draw_oven_page(const RenderContext& ctx);
  void draw_extras_page(const RenderContext& ctx);
  void draw_inventory_box(const RenderContext& ctx);
  void draw_scrollable_text_box(const RenderContext& ctx, ViewPoint p,
                                const std::vector<std::string>& lines,
                                int scroll_offset);
  void draw_info_section(const RenderContext& ctx,
                         const std::vector<InfoLine>& lines);

  int scroll_offset_{0};
  bool scroll_active_{false};
  int scroll_speed_{0};
  int line_height_{12};
  int max_scroll_height_{0};
  StatusController status_controller_;
};
