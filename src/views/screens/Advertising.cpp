/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "views/screens/Advertising.h"

#include "core/character/Player.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"
#include "engine/ui/InteractionEvent.h"
#include "gfx/Blit.h"
#include "magic_enum/magic_enum.hpp"
#include "views/base/ViewDefs.h"

#include <cassert>
#include <memory>
#include "engine/geom/Spaces.h"

inline constexpr int ADVERTISING_BLOCKS_OFFSET_X = 58;
inline constexpr int ADVERTISING_BLOCKS_OFFSET_Y = 116;
inline constexpr int ADVERTISING_BLOCK_SIZE_W = 256;
inline constexpr int ADVERTISING_BLOCK_SIZE_H = 30;
inline constexpr int ADVERTISING_BLOCK_OFFSET = 26;
inline constexpr int CLICKABLE_NEGATIVE_OFFSET = 28;
inline constexpr int MAX_FLYERS = 500;
inline constexpr int MIN_FLYERS = 100;
inline constexpr int FLYERS_STEP_SIZE = 100;
inline constexpr int DEFAULT_CAMPAIGN_RUNTIME = 7;
inline constexpr int MAX_CAMPAIGN_RUNTIME = 30;

constexpr std::array advertising_channels = {
    AdvertisingChannel::FLYERS,
    AdvertisingChannel::POSTERS,
    AdvertisingChannel::NEWSPAPER_ADS,
    AdvertisingChannel::TV_ADS,
};

Advertising::Advertising(GameContext& context, GameState& state)
    : MainPanelView(context, state), tv_ads_(context.font, context.vga) {

  set_up_input_panel();
}

void Advertising::tick() {
  if (tv_ads_.tick()) {
    mark_dirty();
  }
}

void Advertising::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  ctx.surface->fill(240);
  switch (view_mode_) {
  case ViewMode::OVERVIEW:
    render_overview(ctx);
    break;
  case ViewMode::PURCHASING:
    [[fallthrough]];
  case ViewMode::PURCHASED:
    render_purchasing(ctx);
    break;
  case ViewMode::VIEWING:
  default:
    break;
  }
}

void Advertising::render_overview(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back27"));

  for (int i = 0; i <= static_cast<int>(AdvertisingChannel::TV_ADS); ++i) {
    auto channel = static_cast<AdvertisingChannel>(i);

    ViewRect adv_box{
        ADVERTISING_BLOCKS_OFFSET_X,
        ADVERTISING_BLOCKS_OFFSET_Y +
            (i * (ADVERTISING_BLOCK_OFFSET + ADVERTISING_BLOCK_SIZE_H)) -
            CLICKABLE_NEGATIVE_OFFSET,
        ADVERTISING_BLOCK_SIZE_W,
        ADVERTISING_BLOCK_SIZE_H + CLICKABLE_NEGATIVE_OFFSET};

    interactions_.create_frame_item(
        adv_box, magic_enum::enum_name(channel),
        [this, channel](views::ui::InteractionEvent ui) {
          if (ui.type != views::ui::InteractionEvent::Type::Click ||
              ui.button != views::ui::MouseButton::Left) {
            return;
          }
          channel_ = channel;
          mark_dirty();
        });

    int x = ADVERTISING_BLOCKS_OFFSET_X;
    int y = ADVERTISING_BLOCKS_OFFSET_Y +
            (i * (ADVERTISING_BLOCK_OFFSET + ADVERTISING_BLOCK_SIZE_H));

    if (channel_ == channel) {
      // Render the gray/brownish block at the selected channel.
      uint8_t color = 14;
      ViewRect rect(x, y, ADVERTISING_BLOCK_SIZE_W, ADVERTISING_BLOCK_SIZE_H);
      ctx.surface->fill_rect(rect, color);
    }

    // Render campaign info if we have an active campaign
    auto campaign = state_.active_player()->get_campaign(channel);
    if (campaign.has_value()) {
      auto days = campaign->days_remaining;
      auto cost = campaign->rate.cost;
      auto effectiveness = campaign->rate.effectiveness;

      std::string info =
          compat::format("{} days   {}     {} %", days, cost, effectiveness);

      // Render using PIZ-NUMB font, centered in the gray bar
      ViewPoint text_pos{x + 10, y + 8}; // Slight padding from left edge
      context_.font->render_text_onto(*ctx.surface, info, text_pos,
                                      FontFace::PIZ_NUMB);
    }
  }
}

void Advertising::render_purchasing(const RenderContext& ctx) {
  ctx.surface->fill(240);
  assert(channel_);
  switch (channel_.value()) {
  case AdvertisingChannel::FLYERS:
    render_purchasing_flyers(ctx);
    break;
  case AdvertisingChannel::POSTERS:
    render_purchasing_posters(ctx);
    break;
  case AdvertisingChannel::NEWSPAPER_ADS:
    render_purchasing_newspaper_ads(ctx);
    break;
  case AdvertisingChannel::TV_ADS: {
    render_purchasing_tv_ads(ctx);
    // This is a fallback for the TV purchasing;
    // all other modes will have already sent you
    // back to the previous state, but for the TV ads
    // it shows the purchased ad until you click.
    const ViewRect vp_local{0, 0, views::base::MAIN_PANEL_WIDTH,
                            views::base::MAIN_PANEL_HEIGHT};
    // Handle both RMB pause (in both modes) and LMB click (PURCHASED mode)
    interactions_.create_frame_item(
        vp_local, "tv_catch_click", [this](views::ui::InteractionEvent ui) {
          // RMB pauses/resumes playback (matching original behavior)
          if (ui.button == views::ui::MouseButton::Right) {
            if (ui.type == views::ui::InteractionEvent::Type::ButtonDown) {
              tv_ads_.pause();
            }
            else if (ui.type == views::ui::InteractionEvent::Type::ButtonUp) {
              tv_ads_.resume();
            }
            return;
          }
          // LMB click to return to overview (PURCHASED mode only)
          if (view_mode_ == ViewMode::PURCHASED &&
              ui.type == views::ui::InteractionEvent::Type::Click &&
              ui.button == views::ui::MouseButton::Left) {
            return_to_overview();
          }
        });
    break;
  }
  default:
    break;
  }
}

void Advertising::render_purchasing_flyers(const RenderContext& ctx) {
  ctx.surface->fill(240); // TODO remove this because we have two already
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back26"));
  context_.font->render_text_onto(
      *ctx.surface, compat::format("OUTPUT:  {} ITEMS", flyer_count_.value()),
      ViewPoint{176, 84}, FontFace::CSTANDAR);
  context_.font->render_text_onto(*ctx.surface, "Come to", ViewPoint{184, 100},
                                  FontFace::ASTANDAR);
  context_.font->render_text_onto(*ctx.surface, state_.active_player()->name(),
                                  ViewPoint{184, 120}, FontFace::ASTANDAR);
  context_.font->render_text_onto(*ctx.surface, "and try", ViewPoint{184, 140},
                                  FontFace::ASTANDAR);
  context_.font->render_text_onto(*ctx.surface, "our pizzas!",
                                  ViewPoint{134, 160}, FontFace::ASTANDAR);
}

void Advertising::render_purchasing_posters(const RenderContext& ctx) {
  assert(poster_selection_);
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back25"));
  std::string filename =
      "gfx/plakat" + std::to_string(poster_selection_.value());
  auto& poster = context_.vga->get(filename);
  ctx.surface->blit_raw(poster, {64, 18});
}

void Advertising::render_purchasing_newspaper_ads(const RenderContext& ctx) {
  // Werbung is german for advertising
  constexpr int number_of_werbung_lines = 27;
  constexpr int number_of_werbung_lines_on_screen = 5;

  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back31"));
  if (werbung_e_.empty()) {
    auto file = fileutils::open("T/WERBUNG.E");
    std::string line;
    while (std::getline(file, line)) {
      werbung_e_.push_back(line);
    }
  }
  // First we pick a random number that is going to go at the top
  int top_line = engine::random::rand_max_n(number_of_werbung_lines -
                                            number_of_werbung_lines_on_screen);
}

void Advertising::render_purchasing_tv_ads(const RenderContext& ctx) {
  assert(tv_ad_selection_.has_value());
  gfx::blit::blit_with_palette(*ctx.surface,
                               context_.vga->get("gfx/back24"));
  ctx.surface->blit_raw(tv_ads_.next_frame(), {106, 50});
}

void Advertising::handle_page_up_click() {
  // In purchasing screen: adjust selection and duration
  if (view_mode_ == ViewMode::PURCHASING) {
    assert(channel_);
    switch (*channel_) {
    case AdvertisingChannel::FLYERS:
      assert(flyer_count_);
      flyer_count_ = std::min(MAX_FLYERS, *flyer_count_ + FLYERS_STEP_SIZE);
      break;
    case AdvertisingChannel::POSTERS:
      assert(poster_selection_);
      poster_selection_ =
          std::min(NUMBER_OF_POSTERS - 1, *poster_selection_ + 1);
      break;
    case AdvertisingChannel::TV_ADS:
      tv_ads_.next_commercial();
      tv_ad_selection_ = (*tv_ad_selection_ + 1) % NUMBER_OF_TV_ADS;
      break;
    default:
      break;
    }
    // Always allow duration adjustment in purchasing mode
    // campaign_duration_ = std::min(MAX_CAMPAIGN_RUNTIME, campaign_duration_ +
    // 1);
    post_purchase_status();
    mark_dirty();
  }
  // In overview screen: only adjust duration if campaign is editable (just
  // purchased)
  else if (view_mode_ == ViewMode::OVERVIEW && campaign_editable_) {
    campaign_duration_ = std::min(MAX_CAMPAIGN_RUNTIME, campaign_duration_ + 1);
    // Update the stored campaign with new duration
    if (last_purchased_type_.has_value()) {
      auto campaign =
          state_.active_player()->get_campaign(*last_purchased_type_);
      if (campaign.has_value()) {
        campaign->days_remaining = campaign_duration_;
        state_.active_player()->add_campaign(*campaign);
        mark_dirty();
      }
    }
  }
}

void Advertising::handle_page_down_click() {
  // In purchasing screen: adjust selection and duration
  if (view_mode_ == ViewMode::PURCHASING) {
    assert(channel_);
    switch (channel_.value()) {
    case AdvertisingChannel::FLYERS:
      assert(flyer_count_);
      flyer_count_ =
          std::max(MIN_FLYERS, flyer_count_.value() - FLYERS_STEP_SIZE);
      break;
    case AdvertisingChannel::POSTERS:
      poster_selection_ = std::max(0, poster_selection_.value() - 1);
      break;
    case AdvertisingChannel::TV_ADS:
      tv_ads_.previous_commercial();
      tv_ad_selection_ =
          (*tv_ad_selection_ - 1 + NUMBER_OF_TV_ADS) % NUMBER_OF_TV_ADS;
      break;
    default:
      break;
    }
    // Always allow duration adjustment in purchasing mode
    campaign_duration_ =
        std::max(DEFAULT_CAMPAIGN_RUNTIME, campaign_duration_ - 1);
    post_purchase_status();
    mark_dirty();
  }
  // In overview screen: only adjust duration if campaign is editable (just
  // purchased)
  else if (view_mode_ == ViewMode::OVERVIEW && campaign_editable_) {
    campaign_duration_ =
        std::max(DEFAULT_CAMPAIGN_RUNTIME, campaign_duration_ - 1);
    // Update the stored campaign with new duration
    if (last_purchased_type_.has_value()) {
      auto campaign =
          state_.active_player()->get_campaign(*last_purchased_type_);
      if (campaign.has_value()) {
        campaign->days_remaining = campaign_duration_;
        state_.active_player()->add_campaign(*campaign);
        mark_dirty();
      }
    }
  }
}

void Advertising::handle_view_click() {
  // This button only does something in the overview screen
  if (view_mode_ != ViewMode::OVERVIEW) {
    return;
  }
}

void Advertising::initialize_new_campaign(AdvertisingChannel channel) {
  campaign_duration_ = DEFAULT_CAMPAIGN_RUNTIME; // Reset to 7 days
  switch (channel) {
  case AdvertisingChannel::FLYERS:
    flyer_count_ = MIN_FLYERS;
    break;
  case AdvertisingChannel::POSTERS:
    poster_selection_ = 0;
    break;
  case AdvertisingChannel::TV_ADS:
    tv_ad_selection_ = 0;
    // tv_ads_ = std::make_unique<TVPlaybackEngine>(context_.font,
    // context_.vga);
    tv_ads_.init();
    break;
  default:
    break;
  }
  post_purchase_status();
}

void Advertising::reset_new_campaign(AdvertisingChannel channel) {
  switch (channel) {
  case AdvertisingChannel::FLYERS:
    flyer_count_.reset();
    break;
  case AdvertisingChannel::POSTERS:
    poster_selection_.reset();
    break;
  case AdvertisingChannel::TV_ADS:
    tv_ad_selection_.reset();
    tv_ads_.reset();
    break;
  default:
    break;
  }
}

void Advertising::handle_shop_click() {
  if (!channel_) {
    return;
  }

  // Check if player already has an active campaign in this category
  if (view_mode_ == ViewMode::OVERVIEW) {
    if (state_.active_player()->has_active_campaign(channel_.value())) {
      // TODO: Show error message "There is already a campaign in this category"
      return;
    }
    // When entering purchase mode, lock any previously purchased campaign
    campaign_editable_ = false;
    view_mode_ = ViewMode::PURCHASING;
    initialize_new_campaign(channel_.value());
  }
  else if (view_mode_ == ViewMode::PURCHASING) {
    finalize_purchase();
  }
  mark_dirty();
}

void Advertising::handle_back_click() {
  if (view_mode_ == ViewMode::OVERVIEW) {
    // Leaving the screen - lock all campaigns (make them read-only)
    campaign_editable_ = false;
    state_.ui().pop();
  }
  else {
    if (channel_) {
      reset_new_campaign(channel_.value());
    }
    view_mode_ = ViewMode::OVERVIEW;
  }
  mark_dirty();
}

void Advertising::set_up_input_panel() {

  input_panel_->add_button(Icon::ARROW_UP, ButtonType::Horizontal,
                           ButtonPosition::TopLeft, "Turn page",
                           [this] { handle_page_up_click(); });

  input_panel_->add_button(Icon::ARROW_DOWN, ButtonType::Horizontal,
                           ButtonPosition::MiddleLeft, "Turn page",
                           [this] { handle_page_down_click(); });

  input_panel_->add_button(Icon::SHOPPING_CART, ButtonType::Horizontal,
                           ButtonPosition::BottomLeft, "Buy",
                           [this] { handle_shop_click(); });

  input_panel_->add_button(Icon::EXAMINE, ButtonType::Vertical,
                           ButtonPosition::TopRight, "View ads",
                           [this] { handle_view_click(); });

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this] { handle_back_click(); });
}

bool Advertising::is_overview() const {
  return view_mode_ == ViewMode::OVERVIEW;
}

void Advertising::finalize_purchase() {
  if (!channel_) {
    // Somehow we got here without knowing what we were doing.
    return;
  }

  last_purchased_type_ = channel_;

  AdvertisingCampaign campaign;
  campaign.channel = *channel_;
  campaign.city_id = state_.active_city_id();
  campaign.days_remaining = campaign_duration_;

  // Calculate cost and effectiveness based on channel type
  switch (*channel_) {
  case AdvertisingChannel::FLYERS: {
    if (!flyer_count_) {
      return;
    }
    int count_index = (*flyer_count_ / FLYERS_STEP_SIZE) - 1; // 0-4
    campaign.rate.effectiveness = (count_index + 1) * 2;
    campaign.rate.cost = Money{(count_index + 1) * 50 * 100};
    campaign.flyer_count = flyer_count_;
    break;
  }
  case AdvertisingChannel::POSTERS: {
    if (!poster_selection_) {
      return;
    }
    // Get from City's randomized poster options table
    const auto& poster_option =
        state_.active_city()->poster_option(*poster_selection_);
    campaign.rate = poster_option;
    campaign.poster_id = poster_selection_;
    break;
  }
  case AdvertisingChannel::TV_ADS: {
    if (!tv_ad_selection_) {
      return;
    }
    // Get from City's randomized TV ad options table
    const auto& tv_option =
        state_.active_city()->tv_ad_option(*tv_ad_selection_);
    campaign.rate = tv_option;
    campaign.tv_ad_id = tv_ad_selection_;
    break;
  }
  case AdvertisingChannel::NEWSPAPER_ADS:
    // Not yet implemented - requires minigame
    return;
  }

  // Store campaign with player
  state_.active_player()->add_campaign(campaign);

  campaign_editable_ = true;

  if (*channel_ == AdvertisingChannel::TV_ADS) {
    tv_ads_.start_playback();
    view_mode_ = ViewMode::PURCHASED;
  }
  else {
    return_to_overview();
  }
}

void Advertising::return_to_overview() {
  // Clean up any active selections/campaigns
  if (channel_) {
    reset_new_campaign(*channel_);
  }

  // Reset view state
  view_mode_ = ViewMode::OVERVIEW;
  // channel_.reset();

  state_.status().set_baseline([] { return StatusDisplayRecipe{}; });
  mark_dirty();
}

void Advertising::post_purchase_status() {
  if (!channel_) {
    return;
  }

  StatusDisplayRecipe recipe;
  switch (*channel_) {
  case AdvertisingChannel::FLYERS: {
    if (!flyer_count_) {
      return;
    }
    int count_index = (*flyer_count_ / FLYERS_STEP_SIZE) - 1;
    Money cost{static_cast<int64_t>((count_index + 1) * 50 * 100)};
    recipe.add_two_column("Output:", compat::format("{}", *flyer_count_));
    recipe.add_two_column("Costs:", compat::format("{:w}", cost));
    break;
  }
  case AdvertisingChannel::POSTERS: {
    if (!poster_selection_) {
      return;
    }
    const auto& option =
        state_.active_city()->poster_option(*poster_selection_);
    recipe.add_two_column("Costs:", compat::format("{:w}", option.cost));
    break;
  }
  case AdvertisingChannel::TV_ADS: {
    if (!tv_ad_selection_) {
      return;
    }
    const auto& option = state_.active_city()->tv_ad_option(*tv_ad_selection_);
    recipe.add_two_column("Costs:", compat::format("{:w}", option.cost));
    break;
  }
  default:
    return;
  }

  state_.status().set_baseline(
      [recipe = std::move(recipe)]() { return recipe; });
}
