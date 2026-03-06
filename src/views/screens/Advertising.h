/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/marketing/AdvertisingCampaign.h"
#include "engine/geom/Spaces.h"
#include "views/base/MainPanelView.h"
#include "views/helpers/TVPlaybackEngine.h"

class Advertising : public MainPanelView {
  enum class ViewMode {
    OVERVIEW,
    PURCHASING,
    VIEWING,
    PURCHASED,
  };

public:
  Advertising(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  [[nodiscard]] bool is_overview() const;
  void handle_page_up_click();
  void handle_page_down_click();
  void handle_view_click();
  void handle_shop_click();
  void handle_back_click();
  void render_overview(const RenderContext& ctx);
  void render_purchasing(const RenderContext& ctx);
  void render_purchasing_flyers(const RenderContext& ctx);
  void render_purchasing_posters(const RenderContext& ctx);
  void render_purchasing_newspaper_ads(const RenderContext& ctx);
  void render_purchasing_tv_ads(const RenderContext& ctx);
  // void render_viewing(const RenderContext& ctx);
  void finalize_purchase();

  void set_up_input_panel();
  void reset_new_campaign(AdvertisingChannel channel);
  void initialize_new_campaign(AdvertisingChannel channel);
  void return_to_overview();
  void post_purchase_status();

  std::optional<AdvertisingChannel> channel_;
  ViewMode view_mode_{ViewMode::OVERVIEW};
  TVPlaybackEngine tv_ads_;
  // std::optional<NewspaperAdRenderer> newspaper_ads_;
  std::optional<int> flyer_count_;
  std::optional<int> poster_selection_;
  std::optional<int> tv_ad_selection_;
  std::optional<AdvertisingChannel> last_purchased_type_;
  std::vector<std::string> werbung_e_;
  int campaign_duration_{7}; // Days (7-30)
  // True if we just purchased and haven't left screen
  bool campaign_editable_{false};
};
