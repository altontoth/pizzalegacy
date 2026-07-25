/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/AnimatedStats.h"

#include <algorithm>
#include "core/Demographics.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/AnimatedStatsScreen.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/util/LegacyConstants.h"
#include "engine/FormatCompat.h"
#include "engine/Random.h"
#include "gfx/Blit.h"
#include "gfx/Draw.h"
#include "resources/BlitmapIcons.h"
#include "resources/BlitmapManager.h"
#include "resources/FontManager.h"
#include "views/base/ViewDefs.h"
#include "views/ui/PageDecoration.h"

AnimatedStats::AnimatedStats(GameContext& context, GameState& state,
                             core::RestaurantId restaurant_id,
                             AnimatedStatsScreen screen)
    : RestaurantSubView(context, state, restaurant_id), current_screen_(screen),
      previous_screen_(screen), lebbo_bob_(context_.bob->get("bob/lebbo.bob")) {

  restaurant_ = state_.restaurant(restaurant_id_);
  create_buttons();
  reset_animation();
}

void AnimatedStats::tick() {
  // Check if screen changed and reset animation
  if (current_screen_ != previous_screen_) {
    reset_animation();
    previous_screen_ = current_screen_;
  }

  switch (current_screen_) {
  case AnimatedStatsScreen::Production:
    tick_production();
    break;
  case AnimatedStatsScreen::Capacity:
    tick_capacity();
    break;
  case AnimatedStatsScreen::Popularity:
    tick_popularity();
    break;
  case AnimatedStatsScreen::Reputation:
    tick_reputation();
    break;
  case AnimatedStatsScreen::Pizza_Quality:
    tick_pizza_quality();
    break;
  default:
    break;
  }
}

void AnimatedStats::tick_production() {
  if (anim_progress_ < PRODUCTION_TOTAL_FRAMES) {
    anim_progress_++;
    mark_dirty();
  }
}

void AnimatedStats::tick_capacity() {
  // Phase 0: Demand bar, Phase 1: Red fill, Phase 2: Capacity bar, Phase 3+:
  // done
  if (anim_phase_ >= 3) {
    return; // Animation complete
  }

  anim_progress_++;

  // Check phase completion thresholds
  // These are approximations - render will clamp to actual values
  constexpr int DEMAND_BAR_MAX =
      CAPACITY_MAX_BAR_WIDTH / CAPACITY_DEMAND_BAR_SPEED;
  constexpr int RED_FILL_MAX =
      (CAPACITY_NUM_CATEGORIES * CAPACITY_BAR_HEIGHT) / CAPACITY_RED_FILL_SPEED;
  constexpr int CAPACITY_BAR_MAX = CAPACITY_MAX_BAR_WIDTH / CAPACITY_BAR_SPEED;

  int threshold = 0;
  switch (anim_phase_) {
  case 0:
    threshold = DEMAND_BAR_MAX;
    break;
  case 1:
    threshold = RED_FILL_MAX;
    break;
  case 2:
    threshold = CAPACITY_BAR_MAX;
    break;
  default:
    break;
  }

  if (anim_progress_ >= threshold) {
    anim_phase_++;
    anim_progress_ = 0;
  }

  mark_dirty();
}

void AnimatedStats::tick_popularity() {
  if (anim_sub_progress_ >= POPULARITY_NUM_DEMOGRAPHICS) {
    return;
  }

  const auto& strength = restaurant_->economy().competitive_strength();
  int value = strength[static_cast<ConsumerType>(anim_sub_progress_)];
  int fill_target = POPULARITY_BAR_FILL_START - popularity_target_y(value);

  anim_progress_ += POPULARITY_FILL_SPEED * 2;

  if (anim_progress_ >= fill_target) {
    anim_sub_progress_++;
    anim_progress_ = 0;
  }

  mark_dirty();
}

void AnimatedStats::tick_reputation() {
  if (anim_sub_progress_ >= REPUTATION_NUM_COMPONENTS) {
    return;
  }

  const auto& comps = restaurant_->popularity().as_array();
  int magnitude = std::abs(static_cast<int>(comps[anim_sub_progress_]));

  if (magnitude == 0) {
    anim_sub_progress_++;
    mark_dirty();
    return;
  }

  anim_tick_++;
  if (anim_tick_ % ANIM_TICKS_PER_UNIT == 0) {
    anim_progress_++;
    if (anim_progress_ >= magnitude) {
      anim_sub_progress_++;
      anim_progress_ = 0;
      anim_tick_ = 0;
    }
  }

  mark_dirty();
}

void AnimatedStats::tick_pizza_quality() {
  if (anim_sub_progress_ >= PIZZA_QUALITY_NUM_COMPONENTS) {
    return;
  }

  int magnitude = std::abs(pizza_quality_values_[anim_sub_progress_]);
  if (magnitude == 0) {
    anim_sub_progress_++;
    mark_dirty();
    return;
  }

  anim_tick_++;
  if (anim_tick_ % ANIM_TICKS_PER_UNIT == 0) {
    anim_progress_++;
    if (anim_progress_ >= magnitude) {
      anim_sub_progress_++;
      anim_progress_ = 0;
      anim_tick_ = 0;
    }
  }

  mark_dirty();
}

void AnimatedStats::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  switch (current_screen_) {
  case AnimatedStatsScreen::Reputation:
    render_reputation(ctx);
    break;
  case AnimatedStatsScreen::Pizza_Quality:
    render_pizza_quality(ctx);
    break;
  case AnimatedStatsScreen::Capacity:
    render_capacity(ctx);
    break;
  case AnimatedStatsScreen::Popularity:
    render_popularity(ctx);
    break;
  case AnimatedStatsScreen::Turnover:
    render_turnover(ctx);
    break;
  case AnimatedStatsScreen::Production:
    render_production(ctx);
    break;
  case AnimatedStatsScreen::None:
    break;
  }
}

void AnimatedStats::create_buttons() {
  using AS = AnimatedStatsScreen;
  using BT = ButtonType;

  auto make_screen_button = [this](BT type, AS screen, Icon icon,
                                   ButtonPosition pos,
                                   const std::string& tooltip) {
    input_panel_->add_button(icon, type, pos, tooltip, [this, screen]() {
      if (screen == AS::None) {
        state_.ui().pop();
      }
      else {
        current_screen_ = screen;
        mark_dirty();
        Logger::debug("Switched to animated stats screen: {}",
                      static_cast<int>(screen));
      }
    });
  };

  make_screen_button(BT::Horizontal, AS::Production, Icon::ROTATING_ARROWS,
                     ButtonPosition::TopLeft, "Production");

  make_screen_button(BT::Square, AS::Reputation, Icon::P,
                     ButtonPosition::TopRight, "Reputation");

  make_screen_button(BT::Horizontal, AS::Capacity, Icon::CAPACITY,
                     ButtonPosition::MiddleLeft, "Capacity");

  make_screen_button(BT::Square, AS::Pizza_Quality, Icon::Q,
                     ButtonPosition::MiddleRight, "Pizza Quality");

  make_screen_button(BT::Square, AS::Popularity, Icon::CHARACTER2,
                     ButtonPosition::BottomLeft, "Popularity");

  make_screen_button(BT::Square, AS::Turnover, Icon::MONEY_BAG,
                     ButtonPosition::BottomCenter, "Turnover");

  make_screen_button(BT::Square, AS::None, Icon::ARROW_BACK,
                     ButtonPosition::BottomRight, "Back");
}

void AnimatedStats::draw_decorated_background(const RenderContext& ctx,
                                              uint8_t bg_color) const {
  ctx.surface->fill(bg_color);
  const auto& stanbobs = context_.bob->get("bob/stanbobs.bob");
  constexpr int margin = 3;
  views::ui::render_decorated_corners(
      *ctx.surface, stanbobs,
      ViewRect{{margin, margin},
               {views::base::MAIN_PANEL_WIDTH - margin * 2,
                views::base::MAIN_PANEL_HEIGHT - margin * 2}},
      6, {11});
}

// Shared thermometer rendering, ported from the original.
void AnimatedStats::draw_thermometer(const RenderContext& ctx) const {
  constexpr int WL = LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  constexpr int WT = LegacyConstants::WOOD_BORDER_HEIGHT_TOP;
  const auto& blitmap4 = context_.blitmap->get4();

  blitmap4.render_icon(Blitmap4Icon::THERMOMETER_TOP, *ctx.surface,
                       ViewPoint{(99 * 2) - WL, (33 * 2) - WT});
  for (int y = (60 * 2) - WT; y < (151 * 2) - WT; y += 10 * 2) {
    blitmap4.render_icon(Blitmap4Icon::THERMOMETER_MIDDLE, *ctx.surface,
                         ViewPoint{(99 * 2) - WL, y});
  }
  blitmap4.render_icon(Blitmap4Icon::THERMOMETER_BOTTOM, *ctx.surface,
                       ViewPoint{(96 * 2) - WL, (157 * 2) - WT});
}

// fill_level range: -10 to 100, only positive values draw
void AnimatedStats::draw_thermometer_fill(const RenderContext& ctx,
                                          int fill_level) const {
  constexpr int WL = LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  constexpr int WT = LegacyConstants::WOOD_BORDER_HEIGHT_TOP;
  constexpr int FILL_X = (102 * 2) - WL;
  constexpr int FILL_BASE_Y = (140 * 2) - WT;
  const auto& blitmap4 = context_.blitmap->get4();

  int clamped = std::clamp(fill_level, -10, 100);
  for (int level = 0; level < clamped; level++) {
    blitmap4.render_icon(Blitmap4Icon::THERMOMETER_LINE_2, *ctx.surface,
                         ViewPoint{FILL_X, FILL_BASE_Y - (level * 2)});
  }
}

void AnimatedStats::draw_component_arrow(const RenderContext& ctx,
                                         ViewPoint arrow_pos,
                                         bool is_left) const {
  constexpr int BAR_W = 30 * 2;
  const auto& blitmap4 = context_.blitmap->get4();

  auto bar_icon =
      is_left ? Blitmap4Icon::ARROW_BAR_RIGHT : Blitmap4Icon::ARROW_BAR_LEFT;
  auto bar_surf = blitmap4.icon_surface(bar_icon);
  int icon_w = bar_surf.width();
  int icon_h = bar_surf.height();

  // Scroll using raw tick counter for smooth animation independent of fill
  // speed
  int scroll = (anim_tick_ * 2) % std::max(icon_w, 1);
  if (is_left) {
    scroll = icon_w - scroll;
  }

  // Tile the icon with scroll offset to fill BAR_W, clipping to bar bounds
  for (int x = -scroll; x < BAR_W; x += icon_w) {
    int src_x = 0;
    int dest_x = arrow_pos.x + x;
    int w = icon_w;
    if (dest_x < arrow_pos.x) {
      src_x = arrow_pos.x - dest_x;
      w -= src_x;
      dest_x = arrow_pos.x;
    }
    if (dest_x + w > arrow_pos.x + BAR_W) {
      w = arrow_pos.x + BAR_W - dest_x;
    }
    if (w > 0) {
      ctx.surface->blit(bar_surf, ViewRect{src_x, 0, w, icon_h},
                        ViewPoint{dest_x, arrow_pos.y});
    }
  }

  // Arrowhead at the end of the animated arrow body
  if (is_left) {
    blitmap4.render_icon(Blitmap4Icon::ARROW_RIGHT, *ctx.surface,
                         ViewPoint{arrow_pos.x + BAR_W, arrow_pos.y - (3 * 2)});
  }
  else {
    blitmap4.render_icon(
        Blitmap4Icon::ARROW_LEFT, *ctx.surface,
        ViewPoint{arrow_pos.x - (9 * 2), arrow_pos.y - (3 * 2)});
  }
}

void AnimatedStats::render_reputation(const RenderContext& ctx) {
  ctx.surface->palette({7});
  draw_decorated_background(ctx, 29);
  const auto& blitmap4 = context_.blitmap->get4();
  const auto& comps = restaurant_->popularity().as_array();

  constexpr int WL = LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  constexpr int WT = LegacyConstants::WOOD_BORDER_HEIGHT_TOP;

  context_.font->render_text_onto(*ctx.surface, "restaurant popularity",
                                  ViewPoint{192, 18}, FontFace::ASTANDAR, 240,
                                  AlignX::CenterPoint);

  static constexpr std::array<Blitmap4Icon, 8> COMP_ICONS = {
      Blitmap4Icon::ALARM_CLOCK,     // 0: WaitingTime
      Blitmap4Icon::STACK_OF_COINS,  // 1: PizzaPrices
      Blitmap4Icon::UNROLLED_SCROLL, // 2: RecipesOnMenu
      Blitmap4Icon::TELEVISION,      // 3: Advertising
      Blitmap4Icon::MEDAL,           // 4: StarRating
      Blitmap4Icon::CHAIR,           // 5: Furnishing
      Blitmap4Icon::ORDER_TO_PIZZA,  // 6: DemandFulfillment
      Blitmap4Icon::QUESTION_MARK,   // 7: UnexpectedEvents
  };

  // Icon draw positions
  static constexpr std::array<ViewPoint, 8> ICON_POS = {{
      {(30 * 2) - WL, (38 * 2) - WT},   // 0: ALARM_CLOCK
      {(30 * 2) - WL, (78 * 2) - WT},   // 1: STACK_OF_COINS
      {(30 * 2) - WL, (111 * 2) - WT},  // 2: UNROLLED_SCROLL
      {(30 * 2) - WL, (141 * 2) - WT},  // 3: TELEVISION
      {(155 * 2) - WL, (145 * 2) - WT}, // 4: MEDAL
      {(155 * 2) - WL, (106 * 2) - WT}, // 5: CHAIR
      {(155 * 2) - WL, (80 * 2) - WT},  // 6: ORDER_TO_PIZZA
      {(155 * 2) - WL, (38 * 2) - WT},  // 7: QUESTION_MARK
  }};

  // Arrow bar positions
  static constexpr std::array<ViewPoint, 8> ARROW_POS = {{
      {(50 * 2) - WL, (50 * 2) - WT},   // 0
      {(50 * 2) - WL, (90 * 2) - WT},   // 1
      {(50 * 2) - WL, (130 * 2) - WT},  // 2
      {(50 * 2) - WL, (170 * 2) - WT},  // 3
      {(134 * 2) - WL, (170 * 2) - WT}, // 4
      {(130 * 2) - WL, (130 * 2) - WT}, // 5
      {(130 * 2) - WL, (90 * 2) - WT},  // 6
      {(130 * 2) - WL, (50 * 2) - WT},  // 7
  }};

  for (int i = 0; i < REPUTATION_NUM_COMPONENTS; i++) {
    blitmap4.render_icon(COMP_ICONS[i], *ctx.surface, ICON_POS[i]);
  }

  draw_thermometer(ctx);

  // Accumulate fill from completed + in-progress components
  int fill_level = 0;
  for (int i = 0; i < anim_sub_progress_ && i < REPUTATION_NUM_COMPONENTS;
       i++) {
    fill_level += comps[i];
  }
  if (anim_sub_progress_ < REPUTATION_NUM_COMPONENTS) {
    int comp_val = static_cast<int>(comps[anim_sub_progress_]);
    if (comp_val != 0) {
      int sign = (comp_val >= 0) ? 1 : -1;
      int progress = std::min(anim_progress_, std::abs(comp_val));
      fill_level += sign * progress;
    }
  }
  draw_thermometer_fill(ctx, fill_level);

  // Arrow for the currently animating component only
  if (anim_sub_progress_ < REPUTATION_NUM_COMPONENTS) {
    int comp_val = static_cast<int>(comps[anim_sub_progress_]);
    if (comp_val != 0 && anim_progress_ > 0) {
      bool is_left = (anim_sub_progress_ < 4);
      draw_component_arrow(ctx, ARROW_POS[anim_sub_progress_], is_left);
    }
  }
}

void AnimatedStats::render_pizza_quality(const RenderContext& ctx) {
  ctx.surface->palette({7});
  draw_decorated_background(ctx, 29);
  const auto& blitmap4 = context_.blitmap->get4();

  constexpr int WL = LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  constexpr int WT = LegacyConstants::WOOD_BORDER_HEIGHT_TOP;

  context_.font->render_text_onto(*ctx.surface, "quality of pizzas",
                                  ViewPoint{192, 18}, FontFace::ASTANDAR, 240,
                                  AlignX::CenterPoint);

  static constexpr std::array<Blitmap4Icon, 4> QUALITY_ICONS = {
      Blitmap4Icon::INGREDIENTS, // 0: ingredient quality
      Blitmap4Icon::CHEF,        // 1: chef quality
      Blitmap4Icon::OVEN,        // 2: oven quality
      Blitmap4Icon::BUGS,        // 3: bugs/hygiene
  };

  // Icon draw positions
  static constexpr std::array<ViewPoint, 4> ICON_POS = {{
      {(28 * 2) - WL, (60 * 2) - WT},   // 0: INGREDIENTS (top-left)
      {(28 * 2) - WL, (115 * 2) - WT},  // 1: CHEF (bottom-left)
      {(158 * 2) - WL, (123 * 2) - WT}, // 2: OVEN (bottom-right)
      {(146 * 2) - WL, (60 * 2) - WT},  // 3: BUGS (top-right)
  }};

  // Arrow bar positions
  static constexpr std::array<ViewPoint, 4> ARROW_POS = {{
      {(50 * 2) - WL, (70 * 2) - WT},   // 0: INGREDIENTS
      {(50 * 2) - WL, (130 * 2) - WT},  // 1: CHEF
      {(135 * 2) - WL, (120 * 2) - WT}, // 2: OVEN
      {(135 * 2) - WL, (70 * 2) - WT},  // 3: BUGS
  }};

  for (int i = 0; i < PIZZA_QUALITY_NUM_COMPONENTS; i++) {
    blitmap4.render_icon(QUALITY_ICONS[i], *ctx.surface, ICON_POS[i]);
  }

  draw_thermometer(ctx);

  // Accumulate fill from completed + in-progress components
  int fill_level = 0;
  for (int i = 0; i < anim_sub_progress_ && i < PIZZA_QUALITY_NUM_COMPONENTS;
       i++) {
    fill_level += pizza_quality_values_[i];
  }
  if (anim_sub_progress_ < PIZZA_QUALITY_NUM_COMPONENTS) {
    int comp_val = pizza_quality_values_[anim_sub_progress_];
    if (comp_val != 0) {
      int sign = (comp_val >= 0) ? 1 : -1;
      int progress = std::min(anim_progress_, std::abs(comp_val));
      fill_level += sign * progress;
    }
  }
  draw_thermometer_fill(ctx, fill_level);

  // Arrow for the currently animating component only
  if (anim_sub_progress_ < PIZZA_QUALITY_NUM_COMPONENTS) {
    int comp_val = pizza_quality_values_[anim_sub_progress_];
    if (comp_val != 0 && anim_progress_ > 0) {
      bool is_left = (anim_sub_progress_ < 2);
      draw_component_arrow(ctx, ARROW_POS[anim_sub_progress_], is_left);
    }
  }
}

void AnimatedStats::render_capacity(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back37"));

  const auto& bob = context_.bob->get("bob/lebbo.bob");
  auto demand_bar = bob.frame(17, PaletteHandle{40});
  auto unfilled_bar = bob.frame(19, PaletteHandle{40});
  auto capacity_bar = bob.frame(18, PaletteHandle{40});

  // Layout constants
  const auto demand_bar_start = ViewPoint{92, 52};
  const auto category_bar_start = ViewPoint{92, 102};
  const auto capacity_bar_start = ViewPoint{92, 296};
  const auto bar_height = 36;

  // Width of the "trail" segment we repeat to build the growing bar
  const int bar_segment_width = 2;

  const int demand_value = capacity_demand_value_;
  const auto& category_values = capacity_category_values_;

  // Color palette indices for red gradient fill
  const uint8_t fill_colors[CAPACITY_BAR_HEIGHT] = {
      0x16, 0x17, 0x18, 0x17, 0x1A, 0x1D, 0x1D, 0x1D, 0x1A,
      0x18, 0x17, 0x17, 0x18, 0x18, 0x17, 0x16, 0x16, 0x0A};

  const int final_capacity = capacity_final_value_;

  // First, draw all 5 category bars immediately at their FULL capacity
  // Unfilled bars show what we could serve (total capacity per category)
  // Red fill will show what we actually serve (limited by demand)
  for (int i = 0; i < CAPACITY_NUM_CATEGORIES; i++) {
    int bar_y = category_bar_start.y + (i * bar_height);

    int bar_width_pixels = category_values[i] * 2;

    // Draw the unfilled bar segments to fill the width, then the end cap
    for (int x = 0; x < bar_width_pixels; x += bar_segment_width) {
      ctx.surface->blit(unfilled_bar,
                        ViewRect{0, 0, bar_segment_width, bar_height},
                        ViewPoint{category_bar_start.x + x, bar_y});
    }
    // Draw the end cap (full sprite at the end)
    ctx.surface->blit_masked(
        unfilled_bar,
        ViewPoint{category_bar_start.x + bar_width_pixels - 4, bar_y});
  }

  // anim_phase_: 0=DemandBar, 1=RedFill, 2=CapacityBar, 3+=done
  // anim_progress_: progress within current phase

  // Calculate how much of demand bar to show based on animation phase
  int demand_pixels_to_show = 0;
  if (anim_phase_ == 0) {
    // Animating demand bar
    demand_pixels_to_show = std::min(
        anim_progress_ * CAPACITY_DEMAND_BAR_SPEED * 2, demand_value * 2);
  }
  else {
    // Past this phase: show full demand bar
    demand_pixels_to_show = demand_value * 2;
  }

  // Draw demand bar
  if (demand_pixels_to_show > 0) {
    for (int x = 0; x < demand_pixels_to_show - bar_segment_width;
         x += bar_segment_width) {
      ctx.surface->blit(demand_bar,
                        ViewRect{0, 0, bar_segment_width, demand_bar.height()},
                        ViewPoint{demand_bar_start.x + x, demand_bar_start.y});
    }
    ctx.surface->blit_masked(demand_bar, ViewPoint{demand_bar_start.x +
                                                       demand_pixels_to_show -
                                                       demand_bar.width(),
                                                   demand_bar_start.y});
  }

  // Red fill animation through category bars
  int total_rows_to_fill = 0;
  int max_rows = CAPACITY_NUM_CATEGORIES * CAPACITY_BAR_HEIGHT;
  if (anim_phase_ == 1) {
    total_rows_to_fill =
        std::min(anim_progress_ * CAPACITY_RED_FILL_SPEED, max_rows);
  }
  else if (anim_phase_ >= 2) {
    total_rows_to_fill = max_rows;
  }

  // Draw red fills for each category bar
  int running_capacity = demand_value;
  for (int cat = 0; cat < CAPACITY_NUM_CATEGORIES; cat++) {
    running_capacity = std::min(running_capacity, category_values[cat]);
    int fill_width = running_capacity * 2;

    int bar_y = category_bar_start.y + (cat * bar_height);
    int rows_for_this_bar = total_rows_to_fill - (cat * CAPACITY_BAR_HEIGHT);
    rows_for_this_bar =
        std::max(0, std::min(rows_for_this_bar, CAPACITY_BAR_HEIGHT));

    for (int row = 0; row < rows_for_this_bar; row++) {
      uint8_t color = fill_colors[row];
      int y = bar_y + (row * 2);
      ctx.surface->fill_rect(ViewRect{category_bar_start.x, y, fill_width, 2},
                             color);
    }
  }

  // Capacity bar animation at bottom
  int capacity_pixels_to_show = 0;
  if (anim_phase_ == 2) {
    capacity_pixels_to_show =
        std::min(anim_progress_ * CAPACITY_BAR_SPEED * 2, final_capacity * 2);
  }
  else if (anim_phase_ >= 3) {
    capacity_pixels_to_show = final_capacity * 2;
  }

  // Draw capacity bar - growing from left to right
  if (capacity_pixels_to_show > 0) {
    for (int x = 0; x < capacity_pixels_to_show - bar_segment_width;
         x += bar_segment_width) {
      ctx.surface->blit(
          capacity_bar,
          ViewRect{0, 0, bar_segment_width, capacity_bar.height()},
          ViewPoint{capacity_bar_start.x + x, capacity_bar_start.y});
    }
    ctx.surface->blit_masked(
        capacity_bar, ViewPoint{capacity_bar_start.x + capacity_pixels_to_show -
                                    capacity_bar.width(),
                                capacity_bar_start.y});
  }

  // Hover tooltips over the row icons (left of each bar)
  if (context_.config->options().qol_improvements) {
    static constexpr std::array<std::string_view, 5> CAT_LABELS = {
        "Seating", "Ingredient stock", "Chefs", "Ovens", "Waiters"};

    auto tooltip = [&](ViewRect r, std::string_view id, std::string_view text) {
      interactions_.create_frame_item(
          r, id, [](const views::ui::InteractionEvent&) {},
          [text](const views::ui::InteractionEvent& ie) {
            return TooltipRequest{.text = std::string(text),
                                  .anchor = TooltipAnchor::Mouse,
                                  .screenPos = ie.pos,
                                  .palette = 62,
                                  .bg_color = 15,
                                  .text_color = 240,
                                  .border_color = 240,
                                  .padding = 4};
          });
    };

    static constexpr std::array<std::string_view, 5> CAT_IDS = {
        "cap_cat_0", "cap_cat_1", "cap_cat_2", "cap_cat_3", "cap_cat_4"};

    for (int i = 0; i < CAPACITY_NUM_CATEGORIES; i++) {
      int bar_y = category_bar_start.y + (i * bar_height);
      tooltip({0, bar_y, 40, bar_height}, CAT_IDS[i], CAT_LABELS[i]);
    }
  }

  // Draw labels once both bars have finished animating.
  // If the bar is wide enough (>= 52 scaled units = 104px in 2x), the label
  // fits inside the bar; otherwise it appears just after the bar end.
  // Mirrors the original's threshold: scaled bar width + 58 >= 110.
  if (anim_phase_ >= 3) {
    constexpr int LABEL_INSIDE_THRESHOLD = 104; // 52 scaled units * 2x
    constexpr int LABEL_INSIDE_X_OFFSET = 12;   // 6 original pixels * 2x
    constexpr int LABEL_OUTSIDE_GAP = 26;       // 13 original pixels * 2x
    constexpr int LABEL_Y_OFFSET = 14;

    int demand_label_x =
        (demand_pixels_to_show >= LABEL_INSIDE_THRESHOLD)
            ? demand_bar_start.x + LABEL_INSIDE_X_OFFSET
            : demand_bar_start.x + demand_pixels_to_show + LABEL_OUTSIDE_GAP;
    context_.font->render_text_onto(
        *ctx.surface, "demand",
        ViewPoint{demand_label_x, demand_bar_start.y + LABEL_Y_OFFSET},
        FontFace::PIZ_NUMB, 240, AlignX::Left);

    int capacity_label_x = (capacity_pixels_to_show >= LABEL_INSIDE_THRESHOLD)
                               ? capacity_bar_start.x + LABEL_INSIDE_X_OFFSET
                               : capacity_bar_start.x +
                                     capacity_pixels_to_show +
                                     LABEL_OUTSIDE_GAP;
    context_.font->render_text_onto(
        *ctx.surface, "capacity",
        ViewPoint{capacity_label_x, capacity_bar_start.y + LABEL_Y_OFFSET},
        FontFace::PIZ_NUMB, 240, AlignX::Left);
  }
}

void AnimatedStats::render_popularity(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));
  ctx.surface->palette({7});

  context_.font->render_text_onto(*ctx.surface, "Popularity", ViewPoint{92, 20},
                                  FontFace::RUBY, 12, AlignX::Left);
  context_.font->render_text_onto(*ctx.surface, "with customers",
                                  ViewPoint{50, 42}, FontFace::RUBY, 12,
                                  AlignX::Left);

  const auto& stanbobs = context_.bob->get("bob/stanbobs.bob");
  const int edge_offset = 12;
  views::ui::render_decorated_corners(
      *ctx.surface, stanbobs,
      ViewRect{{6, 6},
               {views::base::MAIN_PANEL_WIDTH - 12,
                views::base::MAIN_PANEL_HEIGHT - 12}},
      edge_offset, {11});

  const auto& bar = context_.bob->get("bob/lebbo.bob").frame(20);

  ViewPoint bar_position{40, 70};
  ViewPoint icon_position{40, 292};
  const int spacing_x = 52;
  const auto& blitmap = context_.blitmap->get2();

  const auto& strength = restaurant_->economy().competitive_strength();

  constexpr auto demo_count = static_cast<std::size_t>(ConsumerType::Count);
  std::array<int, demo_count> target_y_values{};
  for (std::size_t i = 0; i < demo_count; ++i) {
    int value = strength[static_cast<ConsumerType>(i)];
    target_y_values[i] = popularity_target_y(value);
  }

  // Loop over the demographics
  for (auto demo : ALL_CONSUMER_TYPES) {
    int i = static_cast<int>(demo);
    const int icon_x = icon_position.x + (i * spacing_x);
    const int bar_x = bar_position.x + (i * spacing_x);

    // Draw the bar background first
    ctx.surface->blit_masked(bar, ViewPoint{bar_x, bar_position.y});

    // Calculate how much to fill based on animation state
    // Animation fills one demographic at a time, from bottom to top
    int current_fill_y = POPULARITY_BAR_FILL_START; // Start with no fill

    if (i < anim_sub_progress_) {
      current_fill_y = target_y_values[i];
    }
    else if (i == anim_sub_progress_) {
      current_fill_y = POPULARITY_BAR_FILL_START - anim_progress_;
      current_fill_y = std::max(current_fill_y, target_y_values[i]);
    }

    // Draw fill using PIXEL_LINE for each row
    const int fill_x = bar_x + 6; // Offset into thermometer
    for (int y = POPULARITY_BAR_FILL_START; y > current_fill_y; y -= 2) {
      blitmap.render_icon(Blitmap2Icon::PIXEL_LINE, *ctx.surface,
                          ViewPoint{fill_x, y});
    }

    // Draw demographic icon at the bottom
    const auto demo_icon = static_cast<Blitmap2Icon>(i);
    blitmap.render_icon(demo_icon, *ctx.surface,
                        ViewPoint{icon_x, icon_position.y});
  }
}

void AnimatedStats::render_turnover(const RenderContext& ctx) {
  ctx.surface->palette({42});
  draw_decorated_background(ctx, 29);

  const auto& stats = restaurant_->stats();

  // Data sources
  int64_t takings_value = stats.day_before_yesterday.revenue.whole();
  int64_t expenses_value = stats.yesterday.total_expenditure().whole();

  // Scale down by 10 until both are <= 15
  while (takings_value > 15 || expenses_value > 15) {
    takings_value /= 10;
    expenses_value /= 10;
  }

  const int takings_text_x = (28 * 2) - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  const int takings_text_y = (30 * 2) - LegacyConstants::WOOD_BORDER_HEIGHT_TOP;
  const int expenses_text_x =
      (120 * 2) - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  const int expenses_text_y =
      (30 * 2) - LegacyConstants::WOOD_BORDER_HEIGHT_TOP;

  // Render text headers
  context_.font->render_text_onto(*ctx.surface, "takings",
                                  ViewPoint{takings_text_x, takings_text_y},
                                  FontFace::ASTANDAR, 13, AlignX::Left);
  context_.font->render_text_onto(*ctx.surface, "expenses",
                                  ViewPoint{expenses_text_x, expenses_text_y},
                                  FontFace::ASTANDAR, 13, AlignX::Left);

  // Draw horizontal dividing line
  const int line_x = 188;
  const int line_y1 = 42;
  const int line_y2 = 334;
  gfx::draw::line(*ctx.surface, ViewPoint{line_x, line_y1},
                  ViewPoint{line_x, line_y2}, 13, 2);

  // Coin rendering constants
  const int takings_coin_base_x =
      (40 * 2) - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  const int expenses_coin_base_x =
      (130 * 2) - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  const int coin_base_y = (138 * 2) - LegacyConstants::WOOD_BORDER_HEIGHT_TOP;
  const int coin_spacing_y = 7 * 2;

  // Render coins
  for (int i = 0; i < 15; i++) {
    int coin_y = coin_base_y - (coin_spacing_y * i);

    // Takings coins (left side)
    if (takings_value > i) {
      // Add random horizontal jitter
      int jitter_x = engine::random::rand_max_n(6);
      int coin_x = takings_coin_base_x + jitter_x;

      // Sprite selection: first coin (i=0) uses sprite 21, rest use sprite 22
      int sprite_index = (i == 0) ? 21 : 22;
      auto coin_sprite = lebbo_bob_.frame(sprite_index);
      ctx.surface->blit_masked(coin_sprite, ViewPoint{coin_x, coin_y});
    }

    // Expenses coins (right side)
    if (expenses_value > i) {
      int jitter_x = engine::random::rand_max_n(6);
      int coin_x = expenses_coin_base_x + jitter_x;

      int sprite_index = (i == 0) ? 21 : 22;
      auto coin_sprite = lebbo_bob_.frame(sprite_index);
      ctx.surface->blit_masked(coin_sprite, ViewPoint{coin_x, coin_y});
    }
  }

  const int value_y = (45 * 2) - LegacyConstants::WOOD_BORDER_HEIGHT_TOP;
  const int takings_value_x =
      (60 * 2) - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  const int expenses_value_x =
      (160 * 2) - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;

  context_.font->render_text_onto(
      *ctx.surface, compat::format("{}", stats.day_before_yesterday.revenue),
      ViewPoint{takings_value_x, value_y}, FontFace::ASTANDAR, 12,
      AlignX::Left);

  context_.font->render_text_onto(
      *ctx.surface, compat::format("{}", stats.yesterday.total_expenditure()),
      ViewPoint{expenses_value_x, value_y}, FontFace::ASTANDAR, 12,
      AlignX::Left);
}

void AnimatedStats::render_production(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back6"));
  context_.font->render_text_onto(*ctx.surface, "output", ViewPoint{150, 18},
                                  FontFace::ASTANDAR, 244, AlignX::Left);

  const int order_count = production_data_.order_count;
  const int oven_count =
      static_cast<int>(production_data_.oven_capacities.size());
  const int bill_count = production_data_.bill_count;

  // North Box: Orders (paper notes)
  if (order_count == 0) {
    context_.font->render_text_onto(*ctx.surface, "NO|ORDERS",
                                    ViewPoint{136, 64}, FontFace::CSTANDAR, 244,
                                    AlignX::Left);
  }
  else {
    // Render paper notes (sprite 13) in grid
    const int grid_start_x = 136;
    const int grid_start_y = 44;
    const int spacing_x = 20;
    const int spacing_y = 22;
    const int columns = 5;

    auto sprite = lebbo_bob_.frame(13);
    for (int i = 0; i < order_count; ++i) {
      int col = i % columns;
      int row = i / columns;
      int x = grid_start_x + (col * spacing_x);
      int y = grid_start_y + (row * spacing_y);

      ctx.surface->blit_masked(sprite, ViewPoint{x, y});
    }
  }

  // East Box: Production (ovens & pizzas)
  // Ovens are positioned horizontally, pizzas stack vertically below each
  const int oven_y = 174 - LegacyConstants::WOOD_BORDER_HEIGHT_TOP;
  const int oven_base_x = 278 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  const int oven_spacing_x = 26;

  const int pizza_base_x = 284 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  const int pizza_oven_spacing_x = 28;
  const int pizza_base_y = 204 - LegacyConstants::WOOD_BORDER_HEIGHT_TOP;
  const int pizza_spacing_y = 10;

  const auto oven_sprite = lebbo_bob_.frame(8);
  const auto pizza_sprite = lebbo_bob_.frame(14);

  for (int oven_idx = 0; oven_idx < oven_count; ++oven_idx) {
    // Render oven icon - horizontal positioning
    int oven_x = oven_base_x + (oven_idx * oven_spacing_x);
    ctx.surface->blit_masked(oven_sprite, ViewPoint{oven_x, oven_y});

    // Get capacity for this oven (already scaled by fulfillment rate)
    int pizzas_to_show = production_data_.oven_capacities[oven_idx];

    // Render pizza icons
    int pizza_x = pizza_base_x + (oven_idx * pizza_oven_spacing_x);
    for (int pizza_idx = 0; pizza_idx < pizzas_to_show; ++pizza_idx) {
      int pizza_y = pizza_base_y + (pizza_idx * pizza_spacing_y);
      ctx.surface->blit_masked(pizza_sprite, ViewPoint{pizza_x, pizza_y});
    }
  }

  // West Box: Money (Dollar Bills)
  const int money_start_x = 12;
  const int money_start_y = 148;
  const int money_spacing_x = 24;
  const int money_spacing_y = 14;
  const int money_columns = 5;

  auto money_sprite = lebbo_bob_.frame(16);
  for (int i = 0; i < bill_count; ++i) {
    int col = i % money_columns;
    int row = i / money_columns;
    int x = money_start_x + (col * money_spacing_x);
    int y = money_start_y + (row * money_spacing_y);

    ctx.surface->blit_raw(money_sprite, ViewPoint{x, y});
  }

  // Arrow Animation; sprites 0-3 (off), 4-7 (on)
  const int arrow_coords_x[4] = {
      296 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT,
      292 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT,
      48 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT,
      42 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT}; // NE, SE, SW, NW
  const int arrow_coords_y[4] = {68 - LegacyConstants::WOOD_BORDER_HEIGHT_TOP,
                                 284 - LegacyConstants::WOOD_BORDER_HEIGHT_TOP,
                                 284 - LegacyConstants::WOOD_BORDER_HEIGHT_TOP,
                                 68 - LegacyConstants::WOOD_BORDER_HEIGHT_TOP};

  for (int i = 0; i < 4; ++i) {
    auto arrow_sprite = lebbo_bob_.frame(i);
    ctx.surface->blit_masked(arrow_sprite,
                             ViewPoint{arrow_coords_x[i], arrow_coords_y[i]});
  }
  if (anim_progress_ < PRODUCTION_TOTAL_FRAMES) {
    // Calculate which arrow should be lit
    int arrow_cycle = anim_progress_ / PRODUCTION_FRAMES_PER_ARROW;
    int current_arrow = arrow_cycle % 4; // 0=NE, 1=SE, 2=SW, 3=NW
    int previous_arrow = (current_arrow + 3) % 4;

    // Render previous arrow as OFF (sprites 0-3)
    auto prev_arrow_sprite = lebbo_bob_.frame(previous_arrow);
    ctx.surface->blit_masked(prev_arrow_sprite,
                             ViewPoint{arrow_coords_x[previous_arrow],
                                       arrow_coords_y[previous_arrow]});

    // Render current arrow as ON (sprites 4-7)
    auto curr_arrow_sprite = lebbo_bob_.frame(current_arrow + 4);
    ctx.surface->blit_masked(curr_arrow_sprite,
                             ViewPoint{arrow_coords_x[current_arrow],
                                       arrow_coords_y[current_arrow]});
  }

  // Hover tooltips to give some sense of what's going on on this screen
  if (context_.config->options().qol_improvements) {
    constexpr int WL = LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
    constexpr int WT = LegacyConstants::WOOD_BORDER_HEIGHT_TOP;

    auto tip = [&](ViewRect r, std::string_view id, std::string_view text) {
      interactions_.create_frame_item(
          r, id, [](const views::ui::InteractionEvent&) {},
          [text](const views::ui::InteractionEvent& ie) {
            return TooltipRequest{.text = std::string(text),
                                  .anchor = TooltipAnchor::Mouse,
                                  .screenPos = ie.pos,
                                  .palette = 62,
                                  .bg_color = 15,
                                  .text_color = 240,
                                  .border_color = 240,
                                  .padding = 4};
          });
    };

    tip({(76 * 2) - WL, (34 * 2) - WT, 63 * 2, 44 * 2}, "prod_north",
        "Orders from customers");
    tip({(15 * 2) - WL, (86 * 2) - WT, 63 * 2, 44 * 2}, "prod_west",
        "Pizzas paid for");
    tip({(139 * 2) - WL, (87 * 2) - WT, 61 * 2, 42 * 2}, "prod_east",
        "Oven output");
    tip({(76 * 2) - WL, (139 * 2) - WT, 63 * 2, 44 * 2}, "prod_south",
        "Delivered pizzas");
  }

  // South Box: baked pizza count
  {
    constexpr int WL = LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
    constexpr int WT = LegacyConstants::WOOD_BORDER_HEIGHT_TOP;

    const int pizza_box_count = production_data_.baked_pizza_count;

    constexpr int bg_color = 29;
    ctx.surface->fill_rect(
        ViewRect{(76 * 2) - WL, (139 * 2) - WT, 63 * 2, 44 * 2}, bg_color);

    // Little pizza box, drawing code as per the original: the code was there it
    // just drawing in the wrong spot, making it invisible
    auto draw_pizza_box = [&](int sx, int sy) {
      gfx::draw::line(*ctx.surface, {sx + 2, sy}, {sx + 18, sy}, 14, 2);
      gfx::draw::line(*ctx.surface, {sx, sy + 2}, {sx + 18, sy + 2}, 14, 2);
      gfx::draw::line(*ctx.surface, {sx, sy + 4}, {sx + 16, sy + 4}, 12, 2);
    };

    constexpr int base_x = (82 * 2) - WL;
    int count = std::min(pizza_box_count, 10);
    for (int i = 0; i < count; i++) {
      int sy = ((180 - i * 3) * 2) - WT;
      draw_pizza_box(base_x, sy);
    }
  }
}

void AnimatedStats::reset_animation() {
  anim_progress_ = 0;
  anim_phase_ = 0;
  anim_sub_progress_ = 0;
  anim_tick_ = 0;

  // Update demographics when switching to popularity screen
  // This ensures demographic_thresholds_ are current before rendering
  if (current_screen_ == AnimatedStatsScreen::Popularity && restaurant_) {
    restaurant_->update_demographics(state_.active_player()->recipes());
  }

  // Calculate capacity stats when switching to capacity screen
  if (current_screen_ == AnimatedStatsScreen::Capacity && restaurant_) {
    calculate_capacity_stats();
  }

  if (current_screen_ == AnimatedStatsScreen::Production && restaurant_) {
    production_data_ = restaurant_->get_production_screen_data(
        state_.active_player()->recipes());
  }

  // Recalculate and cache pizza quality factors when switching to quality
  // screen. The factors are derived values that may not be up-to-date after
  // loading a save (they're only recalculated at end of day).
  if (current_screen_ == AnimatedStatsScreen::Pizza_Quality && restaurant_) {
    restaurant_->calculate_pizza_quality_rating(
        state_.active_player()->recipes());
    pizza_quality_values_ = restaurant_->pizza_quality_factors().as_array();
    Logger::debug("[AnimatedStats] Pizza quality factors: ingredients={}, "
                  "chef={}, oven={}, contamination={} (overall={})",
                  pizza_quality_values_[0], pizza_quality_values_[1],
                  pizza_quality_values_[2], pizza_quality_values_[3],
                  restaurant_->pizza_quality_rating());
  }
}

void AnimatedStats::navigate_to_restaurant(core::RestaurantId new_id) {
  state_.active_restaurant(new_id);
  state_.ui().replace_top<AnimatedStatsState>(new_id, current_screen_);
}

void AnimatedStats::calculate_capacity_stats() {
  const auto& recipes = state_.active_player()->recipes();
  auto service_data = restaurant_->calculate_service_time_data();
  auto analysis =
      restaurant_->calculate_capacity_analysis(recipes, service_data);

  Logger::debug("[AnimatedStats Capacity] Stock capacity (raw): {}",
                analysis.stock_cap);

  // Scale values to bar range
  auto scale_to_bar = [](int value) {
    // Divide by 8
    int scaled = value >> 3;
    return std::min(scaled, 139);
  };

  capacity_demand_value_ = scale_to_bar(analysis.demand);
  capacity_category_values_[0] = scale_to_bar(analysis.seating_cap);
  capacity_category_values_[1] = scale_to_bar(analysis.stock_cap);
  capacity_category_values_[2] = scale_to_bar(analysis.chef_cap);
  capacity_category_values_[3] = scale_to_bar(analysis.oven_cap);
  capacity_category_values_[4] = scale_to_bar(analysis.waiter_cap);

  // Calculate final capacity (min of demand and all category values)
  capacity_final_value_ = capacity_demand_value_;
  for (int i = 0; i < CAPACITY_NUM_CATEGORIES; i++) {
    capacity_final_value_ =
        std::min(capacity_final_value_, capacity_category_values_[i]);
  }
}
