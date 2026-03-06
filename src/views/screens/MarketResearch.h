/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include "engine/geom/Spaces.h"
#include "resources/Bob.h"
#include "resources/BobManager.h"
#include "resources/ResourceManager.h"
#include "views/base/MainPanelView.h"

class District;

enum class MarketResearchScreen {
  Area_Info,
  Property_Info,
  Economy,
  Customer_Taste,
  Trends,
  Pizza_Charts,
  Competitors,
  Market_Share,
  None,
};

class MarketResearch : public MainPanelView {
public:
  MarketResearch(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  void create_buttons();
  void render_area_info(const RenderContext& ctx);
  void render_property_info(const RenderContext& ctx);
  void render_economy(const RenderContext& ctx);
  void render_customer_taste(const RenderContext& ctx);
  void render_trends(const RenderContext& ctx);
  void render_pizza_charts(const RenderContext& ctx);
  void render_competitors(const RenderContext& ctx);
  void render_market_share(const RenderContext& ctx);
  void render_paper(const RenderContext& ctx, ViewRect box);
  void render_bar_diagram_background(const RenderContext& ctx);

  const District& district_;
  MarketResearchScreen current_screen_{MarketResearchScreen::Area_Info};
  Bob& stanbobs_;
};
