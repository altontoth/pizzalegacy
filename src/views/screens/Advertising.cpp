/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "views/screens/Advertising.h"

#include "core/character/Player.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"
#include "engine/ui/InteractionEvent.h"
#include "engine/ui/TextAnchor.h"
#include "gfx/Blit.h"
#include "magic_enum/magic_enum.hpp"
#include "resources/FontTypes.h"
#include "resources/util/FileUtils.h"
#include "views/base/ViewDefs.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include "engine/geom/Spaces.h"

inline constexpr int ADVERTISING_BLOCKS_OFFSET_X = 58;
inline constexpr int ADVERTISING_BLOCKS_OFFSET_Y = 116;
inline constexpr int ADVERTISING_BLOCK_SIZE_W = 256;
inline constexpr int ADVERTISING_BLOCK_SIZE_H = 30;
inline constexpr int ADVERTISING_BLOCK_OFFSET = 26;
inline constexpr int CLICKABLE_NEGATIVE_OFFSET = 28;

// Campaign info columns
inline constexpr int ADVERTISING_INFO_TEXT_PAD_X = 10;
inline constexpr int ADVERTISING_INFO_TEXT_PAD_Y = 8;
inline constexpr int ADVERTISING_INFO_COST_RIGHT_X = 228;
inline constexpr int ADVERTISING_INFO_PCT_RIGHT_X = 308;
inline constexpr uint8_t ADVERTISING_INFO_TEXT_COLOR = 240;
inline constexpr int MAX_FLYERS = 500;
inline constexpr int MIN_FLYERS = 100;
inline constexpr int FLYERS_STEP_SIZE = 100;
inline constexpr int DEFAULT_CAMPAIGN_RUNTIME = 7;
inline constexpr int MAX_CAMPAIGN_RUNTIME = 30;

// Newspaper advertising layout.
inline constexpr int NEWSPAPER_OPTIONS_X = 48;
inline constexpr int NEWSPAPER_OPTIONS_Y = 150;
inline constexpr int NEWSPAPER_OPTION_HEIGHT = 20;
inline constexpr int NEWSPAPER_OPTION_PAD_X = 4;
inline constexpr uint8_t NEWSPAPER_HOVER_COLOR = 14;

// Newspaper ad preview
inline constexpr int NEWSPAPER_PREVIEW_CENTER_X =
    views::base::MAIN_PANEL_WIDTH / 2;
inline constexpr int NEWSPAPER_PREVIEW_NAME_FULL_Y = 112;
inline constexpr int NEWSPAPER_PREVIEW_NAME_HALF_Y = 80;
inline constexpr int NEWSPAPER_PREVIEW_LINES_FULL_Y = 166;
inline constexpr int NEWSPAPER_PREVIEW_LINES_HALF_Y = 122;
inline constexpr uint8_t NEWSPAPER_PREVIEW_DEFAULT_COLOR = 240;

// To cover the side column in the newspaper background:
inline constexpr uint8_t NEWSPAPER_PAPER_COLOR = 159;
inline constexpr ViewRect NEWSPAPER_SIDE_COVER{250, 60, 100, 284};
inline constexpr int NEWSPAPER_COMPANY_NAME_COLOR = 8;
inline constexpr int NEWSPAPER_SINGLE_COLOR = 11;

// Per-line palette for the full color styles:
inline constexpr std::array<uint8_t, NewspaperAdGame::NUM_ROUNDS>
    NEWSPAPER_AD_LINE_COLORS = {22, 23, 24, 25, 26};

struct NewspaperAdStyle {
  FontFace name_font;
  FontFace line_font;
  bool multicolor;
  int name_y;
  int lines_y;
  int line_height;
};

inline constexpr int NEWSPAPER_PREVIEW_LINE_H_SMALL = 18;
inline constexpr int NEWSPAPER_PREVIEW_LINE_H_LARGE = 26;

inline constexpr std::array<NewspaperAdStyle, NewspaperAdGame::NUM_AD_SIZES>
    NEWSPAPER_AD_STYLES = {{
        // half page, 2 colour
        {.name_font = FontFace::ASTANDAR,
         .line_font = FontFace::CSTANDAR,
         .multicolor = false,
         .name_y = NEWSPAPER_PREVIEW_NAME_HALF_Y,
         .lines_y = NEWSPAPER_PREVIEW_LINES_HALF_Y,
         .line_height = NEWSPAPER_PREVIEW_LINE_H_SMALL},
        // half page, 4 colour
        {.name_font = FontFace::ASTANDAR,
         .line_font = FontFace::CSTANDAR,
         .multicolor = true,
         .name_y = NEWSPAPER_PREVIEW_NAME_HALF_Y,
         .lines_y = NEWSPAPER_PREVIEW_LINES_HALF_Y,
         .line_height = NEWSPAPER_PREVIEW_LINE_H_SMALL},
        // full page, 2 colour
        {.name_font = FontFace::RUBY,
         .line_font = FontFace::PIZ_NUMB,
         .multicolor = false,
         .name_y = NEWSPAPER_PREVIEW_NAME_FULL_Y,
         .lines_y = NEWSPAPER_PREVIEW_LINES_FULL_Y,
         .line_height = NEWSPAPER_PREVIEW_LINE_H_LARGE},
        // full page, 4 colour
        {.name_font = FontFace::RUBY,
         .line_font = FontFace::PIZ_NUMB,
         .multicolor = true,
         .name_y = NEWSPAPER_PREVIEW_NAME_FULL_Y,
         .lines_y = NEWSPAPER_PREVIEW_LINES_FULL_Y,
         .line_height = NEWSPAPER_PREVIEW_LINE_H_LARGE},
    }};

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

ClockRate Advertising::clock_rate() const {
  return tv_ads_.is_playing() ? ClockRate::Stopped : ClockRate::Running;
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
    render_viewing(ctx);
    break;
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
    auto campaign =
        state_.active_player()->get_campaign(state_.active_city_id(), channel);
    if (campaign.has_value()) {
      auto days = campaign->days_remaining;
      auto cost = campaign->rate.cost;
      auto effectiveness = campaign->rate.effectiveness;

      int text_y = y + ADVERTISING_INFO_TEXT_PAD_Y;
      context_.font->render_line_onto(
          *ctx.surface, compat::format("{} days", days),
          ViewPoint{x + ADVERTISING_INFO_TEXT_PAD_X, text_y},
          FontFace::PIZ_NUMB, ADVERTISING_INFO_TEXT_COLOR);
      context_.font->render_line_onto(
          *ctx.surface, compat::format("{}", cost),
          ViewPoint{ADVERTISING_INFO_COST_RIGHT_X, text_y}, FontFace::PIZ_NUMB,
          ADVERTISING_INFO_TEXT_COLOR, TextAnchor::End);
      context_.font->render_line_onto(
          *ctx.surface, compat::format("{} %", effectiveness),
          ViewPoint{ADVERTISING_INFO_PCT_RIGHT_X, text_y}, FontFace::PIZ_NUMB,
          ADVERTISING_INFO_TEXT_COLOR, TextAnchor::End);
    }
  }
}

void Advertising::render_purchasing(const RenderContext& ctx) {
  ctx.surface->fill(240);
  assert(channel_);
  switch (channel_.value()) {
  case AdvertisingChannel::FLYERS:
    render_purchasing_flyers(ctx);
    add_page_scroll_zone("flyers_scroll");
    break;
  case AdvertisingChannel::POSTERS:
    render_purchasing_posters(ctx);
    add_page_scroll_zone("posters_scroll");
    break;
  case AdvertisingChannel::NEWSPAPER_ADS:
    add_page_scroll_zone("newspaper_scroll");
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
          if (ui.type == views::ui::InteractionEvent::Type::Scroll) {
            handle_scroll(ui.scroll_delta);
            return;
          }
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
  assert(flyer_count_);
  render_flyer(ctx, *flyer_count_);
}

void Advertising::render_flyer(const RenderContext& ctx, int flyer_count) {
  gfx::blit::blit_with_palette(*ctx.surface, context_.vga->get("gfx/back26"));
  context_.font->render_text_onto(
      *ctx.surface, compat::format("OUTPUT:  {} ITEMS", flyer_count),
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
  render_poster(ctx, *poster_selection_);
}

void Advertising::render_poster(const RenderContext& ctx, int poster_id) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back25"));
  std::string filename = "gfx/plakat" + std::to_string(poster_id);
  auto& poster = context_.vga->get(filename);
  ctx.surface->blit_raw(poster, {64, 18});
}

void Advertising::render_purchasing_newspaper_ads(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back31"));
  ctx.surface->fill_rect(NEWSPAPER_SIDE_COVER, NEWSPAPER_PAPER_COLOR);

  if (newspaper_game_.phase() == NewspaperAdGame::Phase::SegmentSelection) {
    auto options = newspaper_game_.current_options();
    for (int row = 0; row < NewspaperAdGame::OPTIONS_PER_ROUND; ++row) {
      const int y = NEWSPAPER_OPTIONS_Y + (row * NEWSPAPER_OPTION_HEIGHT);
      const auto metrics =
          context_.font->size_text(options[row], FontFace::PIZ_NUMB);
      ViewRect row_rect{NEWSPAPER_OPTIONS_X - NEWSPAPER_OPTION_PAD_X, y,
                        metrics.width + (2 * NEWSPAPER_OPTION_PAD_X),
                        NEWSPAPER_OPTION_HEIGHT};

      if (hovered_news_row_ == row) {
        ViewRect highlight_rect{NEWSPAPER_OPTIONS_X - 6, y, metrics.width,
                                metrics.height};
        ctx.surface->fill_rect(highlight_rect, NEWSPAPER_HOVER_COLOR);
      }
      context_.font->render_text_onto(*ctx.surface, options[row],
                                      ViewPoint{NEWSPAPER_OPTIONS_X, y},
                                      FontFace::PIZ_NUMB);

      interactions_.create_frame_item(
          row_rect, compat::format("news_opt_{}", row),
          [this, row](views::ui::InteractionEvent ui) {
            switch (ui.type) {
            case views::ui::InteractionEvent::Type::Enter:
            case views::ui::InteractionEvent::Type::Hover:
              hovered_news_row_ = row;
              mark_dirty();
              break;
            case views::ui::InteractionEvent::Type::Leave:
              hovered_news_row_.reset();
              mark_dirty();
              break;
            case views::ui::InteractionEvent::Type::Click:
              if (ui.button != views::ui::MouseButton::Left) {
                break;
              }
              hovered_news_row_.reset();
              newspaper_game_.select(row);
              if (newspaper_game_.phase() ==
                  NewspaperAdGame::Phase::FormatSelect) {
                post_purchase_status();
              }
              mark_dirty();
              break;
            default:
              break;
            }
          });
    }
    return;
  }

  // SizeSelect: all 5 fragments are picked, so show the finished ad preview.
  // The +/- buttons cycle the 4 styles; the size label + cost appear in the
  // status window (post_purchase_status), not here.
  render_newspaper_preview(ctx, state_.active_player()->name(),
                           newspaper_game_.chosen_fragments(),
                           newspaper_game_.ad_format());
}

void Advertising::render_newspaper_preview(
    const RenderContext& ctx, const std::string& company_name,
    const std::vector<std::string>& fragments, NewspaperAdFormat ad_format) {
  const auto& style = NEWSPAPER_AD_STYLES[static_cast<int>(ad_format)];
  context_.font->render_line_onto(
      *ctx.surface, company_name,
      ViewPoint{NEWSPAPER_PREVIEW_CENTER_X, style.name_y}, style.name_font,
      NEWSPAPER_PREVIEW_DEFAULT_COLOR, TextAnchor::Middle);

  int line_y = style.lines_y;
  for (int i = 0; i < static_cast<int>(fragments.size()); ++i) {
    const uint8_t color = style.multicolor ? NEWSPAPER_AD_LINE_COLORS[i]
                                           : NEWSPAPER_PREVIEW_DEFAULT_COLOR;
    context_.font->render_line_onto(
        *ctx.surface, fragments[i],
        ViewPoint{NEWSPAPER_PREVIEW_CENTER_X, line_y}, style.line_font, color,
        TextAnchor::Middle);
    line_y += style.line_height;
  }
}

void Advertising::render_viewing(const RenderContext& ctx) {
  if (!channel_) {
    return;
  }
  const auto campaign =
      state_.active_player()->get_campaign(state_.active_city_id(), *channel_);
  if (!campaign) {
    return;
  }
  switch (*channel_) {
  case AdvertisingChannel::FLYERS:
    if (campaign->flyer_count) {
      render_flyer(ctx, *campaign->flyer_count);
    }
    break;
  case AdvertisingChannel::POSTERS:
    if (campaign->poster_id) {
      render_poster(ctx, *campaign->poster_id);
    }
    break;
  case AdvertisingChannel::NEWSPAPER_ADS:
    if (campaign->newspaper_ad_format) {
      gfx::blit::blit_raw_with_palette(*ctx.surface,
                                       context_.vga->get("gfx/back31"));
      ctx.surface->fill_rect(NEWSPAPER_SIDE_COVER, NEWSPAPER_PAPER_COLOR);
      render_newspaper_preview(ctx, state_.active_player()->name(),
                               campaign->newspaper_lines,
                               *campaign->newspaper_ad_format);
    }
    break;
  case AdvertisingChannel::TV_ADS:
    render_tv(ctx);
    break;
  }
}

void Advertising::render_purchasing_tv_ads(const RenderContext& ctx) {
  assert(tv_ad_selection_.has_value());
  render_tv(ctx);
}

void Advertising::render_tv(const RenderContext& ctx) {
  gfx::blit::blit_with_palette(*ctx.surface, context_.vga->get("gfx/back24"));
  ctx.surface->blit_raw(tv_ads_.next_frame(), {106, 50});
}

void Advertising::handle_scroll(int scroll_delta) {
  if (scroll_delta > 0) {
    handle_page_up_click();
  }
  else if (scroll_delta < 0) {
    handle_page_down_click();
  }
}

void Advertising::add_page_scroll_zone(const char* name) {
  const ViewRect panel{0, 0, views::base::MAIN_PANEL_WIDTH,
                       views::base::MAIN_PANEL_HEIGHT};
  interactions_.create_frame_item(
      panel, name, [this](views::ui::InteractionEvent ui) {
        if (ui.type == views::ui::InteractionEvent::Type::Scroll) {
          handle_scroll(ui.scroll_delta);
        }
      });
}

void Advertising::handle_page_up_click() {
  // In purchasing screen: adjust selection and duration
  if (view_mode_ == ViewMode::PURCHASING) {
    assert(channel_);
    if (*channel_ == AdvertisingChannel::NEWSPAPER_ADS) {
      if (newspaper_game_.phase() == NewspaperAdGame::Phase::FormatSelect) {
        newspaper_game_.size_up();
        post_purchase_status();
        mark_dirty();
      }
      return;
    }
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
      auto campaign = state_.active_player()->get_campaign(
          state_.active_city_id(), *last_purchased_type_);
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
    if (*channel_ == AdvertisingChannel::NEWSPAPER_ADS) {
      if (newspaper_game_.phase() == NewspaperAdGame::Phase::FormatSelect) {
        newspaper_game_.size_down();
        post_purchase_status();
        mark_dirty();
      }
      return;
    }
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
      auto campaign = state_.active_player()->get_campaign(
          state_.active_city_id(), *last_purchased_type_);
      if (campaign.has_value()) {
        campaign->days_remaining = campaign_duration_;
        state_.active_player()->add_campaign(*campaign);
        mark_dirty();
      }
    }
  }
}

void Advertising::handle_view_click() {
  if (view_mode_ != ViewMode::OVERVIEW || !channel_) {
    return;
  }
  const auto campaign =
      state_.active_player()->get_campaign(state_.active_city_id(), *channel_);
  if (!campaign) {
    return;
  }
  if (*channel_ == AdvertisingChannel::TV_ADS && campaign->tv_ad_id) {
    tv_ads_.play_commercial(*campaign->tv_ad_id);
  }
  view_mode_ = ViewMode::VIEWING;
  mark_dirty();
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
  case AdvertisingChannel::NEWSPAPER_ADS:
    if (werbung_e_.empty()) {
      werbung_e_ = fileutils::load_text_lines("T/WERBUNG.E");
    }
    newspaper_game_.start(werbung_e_, state_.active_player()->name(),
                          state_.options().fix_legacy_bugs);
    hovered_news_row_.reset();
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
    if (state_.active_player()->has_active_campaign(state_.active_city_id(),
                                                    channel_.value())) {
      // TODO: Show error message "There is already a campaign in this category"
      return;
    }
    // When entering purchase mode, lock any previously purchased campaign
    campaign_editable_ = false;
    view_mode_ = ViewMode::PURCHASING;
    initialize_new_campaign(channel_.value());
  }
  else if (view_mode_ == ViewMode::PURCHASING) {
    // For the newspaper slogan game, the cart only confirms once the player
    // has finished the 5 rounds and is on the size-selection step.
    if (channel_.value() == AdvertisingChannel::NEWSPAPER_ADS &&
        newspaper_game_.phase() != NewspaperAdGame::Phase::FormatSelect) {
      return;
    }
    finalize_purchase();
  }
  mark_dirty();
}

void Advertising::handle_back_click() {
  if (view_mode_ == ViewMode::OVERVIEW) {
    // Leaving the screen - lock all campaigns (make them read-only)
    campaign_editable_ = false;
    state_.ui().pop();
    mark_dirty();
    return;
  }

  // Viewing a running campaign: Back just returns to the overview.
  if (view_mode_ == ViewMode::VIEWING) {
    if (channel_ == AdvertisingChannel::TV_ADS) {
      tv_ads_.reset();
    }
    view_mode_ = ViewMode::OVERVIEW;
    mark_dirty();
    return;
  }

  // Newspaper slogan game: you can only back out before the first pick. Once
  // building has started, back is locked until all 5 lines are chosen; at that
  // point back and buy both finalize the current ad selection.
  if (channel_ == AdvertisingChannel::NEWSPAPER_ADS) {
    if (newspaper_game_.phase() == NewspaperAdGame::Phase::FormatSelect) {
      finalize_purchase();
      mark_dirty();
      return;
    }
    if (!newspaper_game_.chosen_fragments().empty()) {
      // Mid-build: cannot cancel.
      return;
    }
    // Nothing picked yet: fall through to the normal cancel.
  }

  if (channel_) {
    reset_new_campaign(channel_.value());
  }
  view_mode_ = ViewMode::OVERVIEW;
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
    campaign.rate.effectiveness = newspaper_game_.effectiveness();
    campaign.rate.cost = newspaper_game_.cost();
    campaign.newspaper_ad_format = newspaper_game_.ad_format();
    campaign.newspaper_lines = newspaper_game_.chosen_fragments();
    break;
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

  view_mode_ = ViewMode::OVERVIEW;
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
  case AdvertisingChannel::NEWSPAPER_ADS: {
    if (newspaper_game_.phase() != NewspaperAdGame::Phase::FormatSelect) {
      return;
    }
    // e.g. "half page 2 colours" headline, then the cost line.
    recipe.add_header(newspaper_game_.size_label());
    recipe.add_two_column("Costs:",
                          compat::format("{:w}", newspaper_game_.cost()));
    break;
  }
  default:
    return;
  }

  state_.status().set_baseline(
      [recipe = std::move(recipe)]() { return recipe; });
}
