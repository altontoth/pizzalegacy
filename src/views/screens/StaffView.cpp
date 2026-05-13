/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/StaffView.h"

#include <algorithm>

#include "core/character/Character.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/RestaurantStats.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/util/LegacyConstants.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionEvent.h"
#include "factories/CharacterFactory.h"
#include "gfx/SurfaceBuilder.h"
#include "magic_enum/magic_enum.hpp"
#include "resources/BlitmapIcons.h"
#include "views/base/RawRenderer.h"
#include "views/base/ViewDefs.h"
#include "views/ui/PopupManager.h"
#include "views/ui/components/BeveledButton.h"
#include "views/ui/popups/AlertPopup.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/popups/NumberInputPopup.h"
#include "views/ui/widgets/StatBar.h"

#include <cassert>
#include <cstdlib>
#include <set>

using gfx::SurfaceBuilder;

static constexpr int TOP_BAR_HEIGHT = 30;
static constexpr int OVERVIEW_OFFSET = 194;
static constexpr int MANAGER_SETTINGS_OFFSET_Y = 74;
static constexpr int NUMBER_OF_STAFF_SLOTS = 6;
static constexpr int STAFF_SLOT_HEIGHT = 165;
static constexpr int STAFF_OVERVIEW_HEIGHT = 385;
static constexpr int MANAGER_SETTINGS_HEIGHT = 280;
static constexpr int CARD_OFFSET_X = 10;
static constexpr int CARD_OFFSET_Y = StaffView::PANEL_SPACE_ON_TOP;
static constexpr int CARD_WIDTH = 366;
static constexpr int MAX_CARD_HEIGHT =
    views::base::MAIN_PANEL_HEIGHT - StaffView::PANEL_SPACE_ON_TOP;
static constexpr int TEXT_COLOR_BLACK = 240;

// Manager settings panel layout (shared between rendering and interactions)
static constexpr int MS_LABEL_X = 22;
static constexpr int MS_ROW_H = 20;
static constexpr int MS_HEADER_H = 26;
static constexpr int MS_BUTTON_Y_INSET = 2;
static constexpr int MS_BUTTON_H = 18;
static constexpr int MS_BOOL_BUTTON_X = 240;
static constexpr int MS_BOOL_BUTTON_W = 78;
static constexpr int MS_WIDE_BUTTON_X = 200;
static constexpr int MS_WIDE_BUTTON_W = 160;
static constexpr int MS_GAP_BEFORE_BEHAVIOR = 36;

StaffView::StaffView(GameContext& context, GameState& state)
    : RestaurantSubView(context, state, state.active_restaurant()->id()),
      blitmap3_(context_.blitmap->get<Blitmap3Icon>()) {
  load_applicant_pool();
  create_input_panel();
  interactions_.create_persistent_item(
      ViewRect{LegacyConstants::WOOD_BORDER_WIDTH_LEFT,
               LegacyConstants::WOOD_BORDER_HEIGHT_TOP,
               views::base::MAIN_PANEL_WIDTH, views::base::MAIN_PANEL_HEIGHT},
      "staffview_catchall", [this](views::ui::InteractionEvent ie) {
        if (ie.type == views::ui::InteractionEvent::Type::Click &&
            ie.button == views::ui::MouseButton::Left) {
          handle_left_mouse_click(ie.local);
        }
      });
}

StaffView::~StaffView() { state_.advance_time_blocks(4); }

void StaffView::tick() {
  auto state_before = card_animator_.state();
  if (card_animator_.update()) {
    if (card_animator_.state() == SlideAnimator::SlideState::HIDDEN) {
      Logger::info("Animator hidden, resetting both cards.");
      fg_card_.reset();
      bg_card_.reset();
      // Clear selection only when animation completes
      selected_slot_index_.reset();
    }
    else if (card_animator_.state() == SlideAnimator::SlideState::VISIBLE &&
             bg_card_) {
      if (state_before == SlideAnimator::SlideState::DISAPPEARING) {
        // pop() finished; background is now the only visible card
        Logger::info("Moving back to forward card");
        fg_card_ = std::move(*bg_card_);
        bg_card_.reset();
      }
      else {
        // push() finished; discard the old background card
        Logger::info("Push complete, discarding background card");
        bg_card_.reset();
      }
    }
    mark_dirty();
  }
  if (input_panel_needs_update_) {
    Logger::debug("Updating input panel");
    create_input_panel();
    input_panel_->mark_dirty();
    mark_dirty();
  }
}

void StaffView::render(const RenderContext& ctx) {
  ctx.surface->palette(PaletteHandle{13});
  ctx.surface->fill(240);
  assert(input_panel_ != nullptr);
  auto* restaurant = state_.active_restaurant();
  assert(restaurant);

  // Build the base surface based on job type
  // Don't show title when viewing applicant cards or applications
  bool show_title = (screen_ != StaffScreen::APPLICANT_CARD &&
                     screen_ != StaffScreen::APPLICATIONS);

  Surface surface = [&]() -> Surface {
    if (job_ == core::character::JobRole::MANAGER) {
      // Manager-specific layout: single slot + settings panel
      std::string title = show_title ? "manager" : "";
      return SurfaceBuilder::create(views::base::MAIN_PANEL_WIDTH,
                                    views::base::MAIN_PANEL_HEIGHT)
          .blit(create_top_bar(title), {0, 0})
          .blit(create_staff_slots(restaurant), {0, TOP_BAR_HEIGHT - 1})
          .blit(create_manager_settings_panel(restaurant),
                {0, MANAGER_SETTINGS_OFFSET_Y})
          .build();
    }
    // Chef/Waiter layout: 6 slots + stats overview
    std::string title;
    if (show_title) {
      title = (job_ == core::character::JobRole::CHEF) ? "chefs" : "waiters";
    }
    return SurfaceBuilder::create(views::base::MAIN_PANEL_WIDTH,
                                  views::base::MAIN_PANEL_HEIGHT)
        .blit(create_top_bar(title), {0, 0})
        .blit(create_staff_slots(restaurant), {0, TOP_BAR_HEIGHT - 1})
        .blit(create_staff_overview(restaurant), {0, OVERVIEW_OFFSET - 1})
        .build();
  }();

  ctx.surface->blit_raw(surface);

  // Set up interactions
  interactions_.begin_frame();
  views::ui::InteractionContext ictx{.layers = interactions_,
                                     .invalidate = [this] { mark_dirty(); }};

  // Add interaction items for staff slots (only on overview screen when no card
  // is animating)
  if (screen_ == StaffScreen::OVERVIEW &&
      card_animator_.state() == SlideAnimator::SlideState::HIDDEN) {
    constexpr int slot_start_y =
        TOP_BAR_HEIGHT + 20; // Matches create_staff_slots
    constexpr int slot_height = 25;
    constexpr int slot_width =
        views::base::MAIN_PANEL_WIDTH - 22; // Full width minus margins

    if (job_ == core::character::JobRole::MANAGER) {
      // Single manager slot
      const auto& manager_opt = restaurant->manager();
      if (manager_opt.has_value()) {
        ViewRect slot_rect{11, slot_start_y, slot_width, slot_height};
        interactions_.create_frame_item(
            slot_rect, "Manager slot", [this](views::ui::InteractionEvent ie) {
              if (ie.type == views::ui::InteractionEvent::Type::Click &&
                  ie.button == views::ui::MouseButton::Left) {
                on_staff_slot_clicked(core::character::JobRole::MANAGER, 0);
              }
            });
      }
    }
    else {
      // Chef/Waiter slots (6 slots)
      const auto& staff_array = job_ == core::character::JobRole::CHEF
                                    ? restaurant->cooks()
                                    : restaurant->waiters();

      for (size_t i = 0; i < staff_array.size(); ++i) {
        if (staff_array[i].has_value()) {
          int slot_y = slot_start_y + (static_cast<int>(i) * slot_height);
          ViewRect slot_rect{11, slot_y, slot_width, slot_height};
          std::string label = job_ == core::character::JobRole::CHEF
                                  ? "Chef slot " + std::to_string(i)
                                  : "Waiter slot " + std::to_string(i);
          interactions_.create_frame_item(
              slot_rect, label, [this, i](views::ui::InteractionEvent ie) {
                if (ie.type == views::ui::InteractionEvent::Type::Click &&
                    ie.button == views::ui::MouseButton::Left) {
                  on_staff_slot_clicked(job_, static_cast<int>(i));
                }
              });
        }
      }
    }
  }

  // Manager settings toggle button interactions
  if (job_ == core::character::JobRole::MANAGER &&
      screen_ == StaffScreen::OVERVIEW &&
      card_animator_.state() == SlideAnimator::SlideState::HIDDEN) {
    draw_manager_settings_interactions();
  }

  // optionally draw background card
  if (card_animator_.bg_visible() && bg_card_) {
    ctx.surface->blit_masked(*bg_card_, {CARD_OFFSET_X, CARD_OFFSET_Y});
  }

  // foreground card (may be sliding)
  if (card_animator_.state() != SlideAnimator::SlideState::HIDDEN && fg_card_) {
    int y = card_animator_.offset();
    ctx.surface->blit_masked(*fg_card_, {CARD_OFFSET_X, y});
  }
}

Surface StaffView::create_top_bar(const std::string& title) {
  if (!top_bar_) {
    auto bg_surf = blitmap3_.icon_surface(Blitmap3Icon::BLACK_BG);
    top_bar_ = Surface(views::base::MAIN_PANEL_WIDTH, bg_surf.height());
    for (int i = 0; i <= (views::base::MAIN_PANEL_WIDTH / bg_surf.width());
         ++i) {
      top_bar_->blit_raw(bg_surf, {bg_surf.width() * i, 0});
    }
  }
  auto this_top_bar = top_bar_->clone();
  context_.font->render_text_onto(this_top_bar, title, ViewPoint{28, 8},
                                  FontFace::ASTANDAR, 251);
  return this_top_bar;
}

static constexpr int SLOT_X_OFFSET = 11;

void StaffView::draw_gray_header(Surface& dst, int y, int height) {
  auto gl = blitmap3_.icon_surface(Blitmap3Icon::GRAY_FILE_TOP_LEFT);
  auto gm = blitmap3_.icon_surface(Blitmap3Icon::GRAY_FILE_TOP);
  auto gr = blitmap3_.icon_surface(Blitmap3Icon::GRAY_FILE_TOP_RIGHT);
  assert(height <= gl.height());

  dst.blit_raw(gl, {SLOT_X_OFFSET, y});
  for (int i = 0;
       i < (views::base::MAIN_PANEL_WIDTH - 10 - gl.width()) / gm.width();
       ++i) {
    dst.blit_raw(gm, ViewPoint{10 + gl.width() + (i * gm.width()), y});
  }
  dst.blit_raw(
      gr,
      ViewPoint{views::base::MAIN_PANEL_WIDTH - gr.width() - SLOT_X_OFFSET, y});
}

void StaffView::draw_green_area(Surface& dst, int y, int height) {
  auto gm = blitmap3_.icon_surface(Blitmap3Icon::GREEN_BG);
  int area_x = SLOT_X_OFFSET;
  int area_w = views::base::MAIN_PANEL_WIDTH - SLOT_X_OFFSET - 10;
  for (int dy = 0; dy < height; dy += gm.height()) {
    int tile_h = std::min(gm.height(), height - dy);
    for (int dx = 0; dx < area_w; dx += gm.width()) {
      int tile_w = std::min(gm.width(), area_w - dx);
      ViewRect src_rect{0, 0, tile_w, tile_h};
      dst.blit(gm, src_rect, ViewPoint{area_x + dx, y + dy});
    }
  }
}

void StaffView::draw_green_rows(Surface& dst, int y, int count) {
  auto gl = blitmap3_.icon_surface(Blitmap3Icon::GREEN_FILE_TOP_LEFT);
  auto gm = blitmap3_.icon_surface(Blitmap3Icon::GREEN_FILE_TOP);
  auto gr = blitmap3_.icon_surface(Blitmap3Icon::GREEN_FILE_TOP_RIGHT);

  int y_offset = y;
  for (int i = 0; i < count; ++i) {
    dst.blit_raw(gl, ViewPoint{SLOT_X_OFFSET, y_offset});
    for (int j = 0;
         j < (views::base::MAIN_PANEL_WIDTH - 10 - gl.width()) / gm.width();
         ++j) {
      dst.blit_raw(gm, ViewPoint{10 + gl.width() + (j * gm.width()), y_offset});
    }
    dst.blit_raw(gr, ViewPoint{views::base::MAIN_PANEL_WIDTH - gr.width() -
                                   SLOT_X_OFFSET,
                               y_offset});
    y_offset += gl.height() - 1;
  }
}

Surface StaffView::create_manager_settings_panel(Restaurant* r) {
  auto surface =
      Surface(views::base::MAIN_PANEL_WIDTH, MANAGER_SETTINGS_HEIGHT);

  // Fill with green/black background
  auto bg_surf = blitmap3_.icon_surface(Blitmap3Icon::BLACK_GREEN_BG);
  for (int y = 0; y < MANAGER_SETTINGS_HEIGHT; y += bg_surf.height()) {
    surface.blit_raw(bg_surf, ViewPoint{0, y});
  }
  ViewRect brown_box_rect{372, 0, 12, MANAGER_SETTINGS_HEIGHT};
  surface.fill_rect(brown_box_rect, 9);

  auto& settings = r->manager_settings();

  constexpr int text_y_offset = 6;
  int y = 0;

  // Authority
  draw_gray_header(surface, y, MS_HEADER_H);
  context_.font->render_text_onto(surface, "authority",
                                  ViewPoint{MS_LABEL_X, y + text_y_offset},
                                  FontFace::PIZ_NUMB, TEXT_COLOR_BLACK);
  y += MS_HEADER_H;

  struct BoolToggle {
    const char* label;
    bool* value;
  };
  BoolToggle authority_toggles[] = {
      {.label = "shopping if needed", .value = &settings.shopping_if_needed},
      {.label = "change prices", .value = &settings.change_prices},
      {.label = "overdraw account", .value = &settings.overdraw_account},
  };

  draw_green_area(surface, y, 3 * MS_ROW_H);
  for (int i = 0; i < 3; ++i) {
    int row_y = y + (i * MS_ROW_H);
    context_.font->render_text_onto(
        surface, authority_toggles[i].label,
        ViewPoint{MS_LABEL_X, row_y + text_y_offset}, FontFace::CSTANDAR,
        TEXT_COLOR_BLACK);

    std::string_view btn_label = *authority_toggles[i].value ? "yes" : "no";
    ViewRect btn_rect{MS_BOOL_BUTTON_X, row_y + MS_BUTTON_Y_INSET,
                      MS_BOOL_BUTTON_W, MS_BUTTON_H};
    views::ui::components::draw_beveled_button(
        context_.font, context_.vga->get("gfx/permap"), surface, btn_rect,
        btn_label,
        (pressed_settings_row_ == i)
            ? views::ui::components::ButtonState::Pressed
            : views::ui::components::ButtonState::Normal);
  }
  y += 3 * MS_ROW_H;

  // Reactions
  draw_gray_header(surface, y, MS_HEADER_H);
  context_.font->render_text_onto(surface, "reactions",
                                  ViewPoint{MS_LABEL_X, y + text_y_offset},
                                  FontFace::PIZ_NUMB, TEXT_COLOR_BLACK);
  y += MS_HEADER_H;

  struct ReactionToggle {
    const char* label;
    ManagerReaction* value;
  };
  ReactionToggle reaction_toggles[] = {
      {.label = "turnover decrease", .value = &settings.turnover_decrease},
      {.label = "turnover increase", .value = &settings.turnover_increase},
      {.label = "supply shortage", .value = &settings.supply_shortage},
      {.label = "staff shortage", .value = &settings.staff_shortage},
  };

  auto reaction_label = [](ManagerReaction r) -> std::string_view {
    switch (r) {
    case ManagerReaction::DoNothing:
      return "do nothing";
    case ManagerReaction::CallMe:
      return "call me";
    case ManagerReaction::ActYourself:
      return "act yourself";
    }
    return "???";
  };

  draw_green_area(surface, y, 4 * MS_ROW_H);
  for (int i = 0; i < 4; ++i) {
    int row_y = y + (i * MS_ROW_H);
    int settings_row = 3 + i;
    context_.font->render_text_onto(
        surface, reaction_toggles[i].label,
        ViewPoint{MS_LABEL_X, row_y + text_y_offset}, FontFace::CSTANDAR,
        TEXT_COLOR_BLACK);

    ViewRect btn_rect{MS_WIDE_BUTTON_X, row_y + MS_BUTTON_Y_INSET,
                      MS_WIDE_BUTTON_W, MS_BUTTON_H};
    views::ui::components::draw_beveled_button(
        context_.font, context_.vga->get("gfx/permap"), surface, btn_rect,
        reaction_label(*reaction_toggles[i].value),
        (pressed_settings_row_ == settings_row)
            ? views::ui::components::ButtonState::Pressed
            : views::ui::components::ButtonState::Normal);
  }
  y += 4 * MS_ROW_H;
  draw_green_area(surface, y, MS_GAP_BEFORE_BEHAVIOR);
  y += MS_GAP_BEFORE_BEHAVIOR;

  // Behavior towards staff
  draw_gray_header(surface, y, MS_HEADER_H);
  context_.font->render_text_onto(surface, "behavior towards staff",
                                  ViewPoint{MS_LABEL_X, y + text_y_offset},
                                  FontFace::PIZ_NUMB, TEXT_COLOR_BLACK);
  y += MS_HEADER_H;

  auto behavior_label = [](ManagerBehavior b) -> std::string_view {
    switch (b) {
    case ManagerBehavior::Merciless:
      return "merciless";
    case ManagerBehavior::Severe:
      return "severe";
    case ManagerBehavior::Strict:
      return "strict";
    case ManagerBehavior::Moderate:
      return "moderate";
    case ManagerBehavior::Cooperative:
      return "cooperative";
    case ManagerBehavior::Friendly:
      return "friendly";
    case ManagerBehavior::Warmly:
      return "warmly";
    }
    return "???";
  };

  draw_green_area(surface, y, MS_ROW_H);
  {
    int settings_row = 7; // offset past authority + reaction rows
    ViewRect btn_rect{MS_WIDE_BUTTON_X, y + MS_BUTTON_Y_INSET, MS_WIDE_BUTTON_W,
                      MS_BUTTON_H};
    views::ui::components::draw_beveled_button(
        context_.font, context_.vga->get("gfx/permap"), surface, btn_rect,
        behavior_label(settings.behavior_towards_staff),
        (pressed_settings_row_ == settings_row)
            ? views::ui::components::ButtonState::Pressed
            : views::ui::components::ButtonState::Normal);
  }
  y += MS_ROW_H;
  draw_green_area(surface, y, MAX_CARD_HEIGHT - y);

  return surface;
}

void StaffView::draw_manager_settings_interactions() {
  auto* restaurant = state_.active_restaurant();
  if (!restaurant) {
    return;
  }
  auto& settings = restaurant->manager_settings();

  // Track y the same way as create_manager_settings_panel
  const int panel_y = MANAGER_SETTINGS_OFFSET_Y;
  int y = panel_y;

  // Authority section
  y += MS_HEADER_H; // skip gray header
  bool* authority_values[] = {
      &settings.shopping_if_needed,
      &settings.change_prices,
      &settings.overdraw_account,
  };
  for (int i = 0; i < 3; ++i) {
    int row_y = y + (i * MS_ROW_H);
    ViewRect btn_rect{MS_BOOL_BUTTON_X, row_y + MS_BUTTON_Y_INSET,
                      MS_BOOL_BUTTON_W, MS_BUTTON_H};
    int settings_row = i;
    bool* val = authority_values[i];
    interactions_.create_frame_item(
        btn_rect, "authority_" + std::to_string(i),
        [this, val, settings_row](views::ui::InteractionEvent ie) {
          if (ie.type == views::ui::InteractionEvent::Type::ButtonDown) {
            pressed_settings_row_ = settings_row;
            mark_dirty();
          }
          else if (ie.type == views::ui::InteractionEvent::Type::Click) {
            *val = !*val;
            pressed_settings_row_ = -1;
            mark_dirty();
          }
          else if (ie.type == views::ui::InteractionEvent::Type::Cancelled) {
            pressed_settings_row_ = -1;
            mark_dirty();
          }
        });
  }
  y += 3 * MS_ROW_H;

  // Reaction section
  y += MS_HEADER_H; // skip gray header
  ManagerReaction* reaction_values[] = {
      &settings.turnover_decrease,
      &settings.turnover_increase,
      &settings.supply_shortage,
      &settings.staff_shortage,
  };
  auto next_reaction = [](ManagerReaction r) -> ManagerReaction {
    switch (r) {
    case ManagerReaction::DoNothing:
      return ManagerReaction::CallMe;
    case ManagerReaction::CallMe:
      return ManagerReaction::ActYourself;
    case ManagerReaction::ActYourself:
      return ManagerReaction::DoNothing;
    }
    return ManagerReaction::DoNothing;
  };
  for (int i = 0; i < 4; ++i) {
    int row_y = y + (i * MS_ROW_H);
    ViewRect btn_rect{MS_WIDE_BUTTON_X, row_y + MS_BUTTON_Y_INSET,
                      MS_WIDE_BUTTON_W, MS_BUTTON_H};
    int settings_row = 3 + i;
    ManagerReaction* val = reaction_values[i];
    interactions_.create_frame_item(
        btn_rect, "reaction_" + std::to_string(i),
        [this, val, settings_row,
         next_reaction](views::ui::InteractionEvent ie) {
          if (ie.type == views::ui::InteractionEvent::Type::ButtonDown) {
            pressed_settings_row_ = settings_row;
            mark_dirty();
          }
          else if (ie.type == views::ui::InteractionEvent::Type::Click) {
            *val = next_reaction(*val);
            pressed_settings_row_ = -1;
            mark_dirty();
          }
          else if (ie.type == views::ui::InteractionEvent::Type::Cancelled) {
            pressed_settings_row_ = -1;
            mark_dirty();
          }
        });
  }
  y += 4 * MS_ROW_H;

  // Gap before behavior
  y += MS_GAP_BEFORE_BEHAVIOR;

  // Behavior section
  y += MS_HEADER_H; // skip gray header
  {
    ViewRect btn_rect{MS_WIDE_BUTTON_X, y + MS_BUTTON_Y_INSET, MS_WIDE_BUTTON_W,
                      MS_BUTTON_H};
    int settings_row = 7;
    interactions_.create_frame_item(
        btn_rect, "behavior",
        [this, &settings, settings_row](views::ui::InteractionEvent ie) {
          if (ie.type == views::ui::InteractionEvent::Type::ButtonDown) {
            pressed_settings_row_ = settings_row;
            mark_dirty();
          }
          else if (ie.type == views::ui::InteractionEvent::Type::Click) {
            int next =
                (static_cast<int>(settings.behavior_towards_staff) + 1) %
                static_cast<int>(magic_enum::enum_count<ManagerBehavior>());
            settings.behavior_towards_staff =
                static_cast<ManagerBehavior>(next);
            pressed_settings_row_ = -1;
            mark_dirty();
          }
          else if (ie.type == views::ui::InteractionEvent::Type::Cancelled) {
            pressed_settings_row_ = -1;
            mark_dirty();
          }
        });
  }
}

Surface StaffView::create_staff_overview(Restaurant* r) {
  if (!empty_staff_overview_surface_) {
    // First get one of the corner pieces so we can figure out the exact colors
    // for drawing the boxes that they surround.
    auto stats_top_left = blitmap3_.icon_surface(Blitmap3Icon::STATS_TOP_LEFT);
    auto stats_top_right =
        blitmap3_.icon_surface(Blitmap3Icon::STATS_TOP_RIGHT);
    auto stats_bottom_left =
        blitmap3_.icon_surface(Blitmap3Icon::STATS_BOTTOM_LEFT);
    auto stats_bottom_right =
        blitmap3_.icon_surface(Blitmap3Icon::STATS_BOTTOM_RIGHT);

    // This is the main box that we'll be painting some edge pieces on
    constexpr ViewRect box = {38, 6, 300, 150};
    constexpr int line_width = 2;
    constexpr int padding = 12;
    constexpr int bg_color = 13;
    constexpr int line_color = 9;
    constexpr int outer_color = 15;
    constexpr int inner_color = 14;

    empty_staff_overview_surface_ =
        SurfaceBuilder::create(views::base::MAIN_PANEL_WIDTH,
                               STAFF_OVERVIEW_HEIGHT)
            .fill(bg_color)
            .line({0, 0}, {views::base::MAIN_PANEL_WIDTH - 1, 0}, line_color, 2)
            .fill_rect(box, line_color)
            .fill_rect(ViewRect{box.x + line_width, box.y + line_width,
                                box.w - (line_width * 2),
                                box.h - (line_width * 2)},
                       outer_color)
            .fill_rect(ViewRect{box.x + line_width + padding,
                                box.y + line_width + padding,
                                box.w - ((line_width + padding) * 2),
                                box.h - ((line_width + padding) * 2)},
                       inner_color)
            .blit(stats_top_left, ViewPoint{box.x, box.y})
            .blit(stats_top_right,
                  ViewPoint{box.x + box.w - stats_top_right.width(), box.y})
            .blit(stats_bottom_left,
                  ViewPoint{box.x, box.y + box.h - stats_bottom_left.height()})
            .blit(stats_bottom_right,
                  ViewPoint{box.x + box.w - stats_bottom_right.width(),
                            box.y + box.h - stats_bottom_right.height()})
            .build();
  }

  // Gather stats from the staff array matching the current job tab
  bool is_chef = (job_ == core::character::JobRole::CHEF);
  auto& staff_array = is_chef ? r->cooks() : r->waiters();

  int count = is_chef ? r->count_cooks() : r->count_waiters();
  int total_capacity = 0;
  int total_quality = 0;
  Money total_costs{0};

  for (const auto& slot : staff_array) {
    if (slot.has_value()) {
      if (!slot->is_away()) {
        total_capacity += slot->pizzas_per_day();
        total_quality +=
            is_chef ? slot->character().cooking() : slot->character().serving();
      }
      total_costs += slot->payment();
    }
  }

  int avg_quality = (count > 0) ? total_quality / count : 0;

  // Time per pizza: 720 minutes / total capacity (0 for waiters)
  constexpr int OPERATING_MINUTES_PER_DAY = 720;
  int time_per_pizza = 0;
  if (is_chef && total_capacity > 0) {
    time_per_pizza = OPERATING_MINUTES_PER_DAY / total_capacity;
  }

  // Work: percentage of this role's capacity actually used (yesterday's final)
  int pizzas_sold = r->stats().yesterday.orders_fulfilled;
  int work = (total_capacity > 0)
                 ? std::min(pizzas_sold * 100 / total_capacity, 100)
                 : 0;

  auto surface = empty_staff_overview_surface_->clone();
  struct Line {
    std::string_view label;
    int value;
  };
  std::array<Line, 6> lines = {
      Line{.label = "number:", .value = count},
      Line{.label = "total capacity:", .value = total_capacity},
      Line{.label = "average quality:", .value = avg_quality},
      Line{.label = "time per pizza:", .value = time_per_pizza},
      Line{.label = "work:", .value = work},
      Line{.label = "total costs",
           .value = static_cast<int>(total_costs.whole())},
  };
  constexpr int text_color = 10;
  constexpr int line_color = 13;
  const int label_x = 68;
  const int value_x = 302;
  int y = 24;
  int line_spacing = 20;
  for (auto line : lines) {
    context_.font->render_text_onto(surface, line.label, ViewPoint{label_x, y},
                                    FontFace::PIZ_NUMB, text_color);
    context_.font->render_text_onto(surface, std::to_string(line.value),
                                    ViewPoint{value_x, y}, FontFace::PIZ_NUMB,
                                    text_color, AlignX::RightEdge);
    gfx::draw::line(surface, ViewPoint{label_x, y + line_spacing - 4},
                    ViewPoint{value_x, y + line_spacing - 4}, line_color, 2);
    y += line_spacing;
  }

  return surface;
}

Surface StaffView::create_staff_slots(Restaurant* r) {
  const int num_slots =
      (job_ == core::character::JobRole::MANAGER) ? 1 : NUMBER_OF_STAFF_SLOTS;

  if (!empty_staff_slots_surface_) {
    auto bg_surf = blitmap3_.icon_surface(Blitmap3Icon::BLACK_GREEN_BG);

    empty_staff_slots_surface_ =
        Surface(views::base::MAIN_PANEL_WIDTH, STAFF_SLOT_HEIGHT);

    // Alright, first left side of the screen
    for (int i = 0; i <= (STAFF_SLOT_HEIGHT / bg_surf.height()); ++i) {
      empty_staff_slots_surface_->blit_raw(bg_surf,
                                           ViewPoint{0, bg_surf.height() * i});
    }
    ViewRect brown_box_rect{372, 0, 12, 326};
    uint8_t brown = 9;
    empty_staff_slots_surface_->fill_rect(brown_box_rect, brown);

    draw_gray_header(*empty_staff_slots_surface_, 0);
    draw_green_rows(*empty_staff_slots_surface_, 20, num_slots);
  }
  auto this_staff_slots = empty_staff_slots_surface_->clone();

  // Column positions and styling
  constexpr int name_x = 22;         // X position for name column
  constexpr int days_absent_x = 138; // X position for capacity (pizzas_per_day)
  constexpr int cap_header_x = 276;  // X position for capacity (pizzas_per_day)
  constexpr int cap_x = 300;         // X position for capacity (pizzas_per_day)
  constexpr int pay_x = 324;         // X position for payment
  constexpr int header_y = 8;        // Y position for column headers
  constexpr int slot_start_y = 20;   // First slot starts at y=20
  constexpr int slot_height = 25;    // Height between slots
  constexpr int text_y_offset = 6;   // Y offset within slot for text
  constexpr int x_offset = 11;

  // If a slot is selected, overlay gray-ish beige background on it
  // Keep that background until animation fully completes
  if (selected_slot_index_.has_value() &&
      card_animator_.state() != SlideAnimator::SlideState::HIDDEN) {
    int selected_idx = selected_slot_index_.value();

    // Calculate Y position for the selected slot
    int selected_y = slot_start_y + (selected_idx * slot_height);

    // Render gray-ish background over the green one for selected slot
    auto gray_left = blitmap3_.icon_surface(Blitmap3Icon::GRAY_FILE_TOP_LEFT);
    auto gray_mid = blitmap3_.icon_surface(Blitmap3Icon::GRAY_FILE_TOP);
    auto gray_right = blitmap3_.icon_surface(Blitmap3Icon::GRAY_FILE_TOP_RIGHT);

    this_staff_slots.blit_raw(gray_left, ViewPoint{x_offset, selected_y});
    for (int j = 0;
         j < (views::base::MAIN_PANEL_WIDTH - 10 - gray_left.width()) /
                 gray_mid.width();
         ++j) {
      this_staff_slots.blit_raw(
          gray_mid, ViewPoint{10 + gray_left.width() + (j * gray_mid.width()),
                              selected_y});
    }
    this_staff_slots.blit_raw(
        gray_right,
        ViewPoint{views::base::MAIN_PANEL_WIDTH - gray_right.width() - x_offset,
                  selected_y});
  }

  // Render column headers
  context_.font->render_text_onto(this_staff_slots, "NAME",
                                  ViewPoint{name_x, header_y},
                                  FontFace::CSTANDAR, TEXT_COLOR_BLACK);
  context_.font->render_text_onto(this_staff_slots, "DAYS ABSENT",
                                  ViewPoint{days_absent_x, header_y},
                                  FontFace::CSTANDAR, TEXT_COLOR_BLACK);
  context_.font->render_text_onto(this_staff_slots, "CAP",
                                  ViewPoint{cap_header_x, header_y},
                                  FontFace::CSTANDAR, TEXT_COLOR_BLACK);
  context_.font->render_text_onto(this_staff_slots, "PAY",
                                  ViewPoint{pay_x, header_y},
                                  FontFace::CSTANDAR, TEXT_COLOR_BLACK);

  // Render staff data into slots
  if (job_ == core::character::JobRole::MANAGER) {
    auto& manager = r->manager();
    std::string name = manager.has_value()
                           ? manager->character().first_name() + " " +
                                 manager->character().last_name()
                           : "  no manager";
    context_.font->render_text_onto(
        this_staff_slots, name, ViewPoint{name_x, slot_start_y + text_y_offset},
        FontFace::PIZ_NUMB, TEXT_COLOR_BLACK);
    if (manager.has_value()) {
      context_.font->render_text_onto(
          this_staff_slots, compat::format("{}", manager->payment()),
          ViewPoint{pay_x, slot_start_y + text_y_offset}, FontFace::PIZ_NUMB,
          TEXT_COLOR_BLACK);

      std::string capacity =
          manager->is_absent()
              ? compat::format("{} {}", manager->days_away_remaining(),
                               manager->absence_reason())
              : std::to_string(manager->pizzas_per_day());

      context_.font->render_text_onto(
          this_staff_slots, capacity,
          ViewPoint{cap_x, slot_start_y + text_y_offset}, FontFace::PIZ_NUMB,
          TEXT_COLOR_BLACK, AlignX::RightEdge);
    }
  }
  else {
    const auto& staff_array =
        job_ == core::character::JobRole::CHEF ? r->cooks() : r->waiters();

    for (size_t i = 0; i < staff_array.size(); ++i) {
      const auto& staff = staff_array[i];
      if (staff) {
        const auto& character = staff->character();
        int slot_y = slot_start_y + (static_cast<int>(i) * slot_height);
        std::string full_name =
            character.first_name() + " " + character.last_name();
        std::string capacity =
            staff->is_absent()
                ? compat::format("{} {}", staff->days_away_remaining(),
                                 staff->absence_reason())
                : std::to_string(staff->pizzas_per_day());

        context_.font->render_text_onto(
            this_staff_slots, full_name,
            ViewPoint{name_x, slot_y + text_y_offset}, FontFace::PIZ_NUMB,
            TEXT_COLOR_BLACK);

        context_.font->render_text_onto(
            this_staff_slots, capacity,
            ViewPoint{cap_x, slot_y + text_y_offset}, FontFace::PIZ_NUMB,
            TEXT_COLOR_BLACK, AlignX::RightEdge);

        context_.font->render_text_onto(
            this_staff_slots, compat::format("{}", staff->payment()),
            ViewPoint{pay_x, slot_y + text_y_offset}, FontFace::PIZ_NUMB,
            TEXT_COLOR_BLACK);
      }
    }
  }
  return this_staff_slots;
}

Surface StaffView::create_data_card(const Staff& staff) {
  const auto& character = staff.character();
  auto snapshot = character.to_character_data();
  // Use effective capacity (base + skill bonus) so card matches list CAP column
  snapshot.pizzas_per_day = staff.pizzas_per_day();

  return create_data_card_impl(character, snapshot, staff.payment().whole(),
                               "");
}

Surface StaffView::create_data_card(core::character::CharacterData* character,
                                    core::character::JobRole role) {
  return create_data_card_impl(*character, *character,
                               character->starting_payment(job_),
                               job_string(role));
}

Surface StaffView::create_data_card_impl(
    const RawRenderable& portrait, const core::character::CharacterData& data,
    int payment_dollars, const std::string& tab_label) {
  auto card = create_empty_card(tab_label);

  RawRenderer r;
  auto picture = r.surface(portrait);

  // Photo frame background colors
  uint8_t photo_white_bg = 15;
  uint8_t photo_darker_bg = 11;
  uint8_t photo_light_bg = 12;
  ViewRect photo_location(8, 30, 118, 132);

  // Local lambda to draw a frame layer
  auto draw_frame_layer = [&card](const ViewRect& base, int dx, int dy,
                                  uint8_t color) {
    ViewRect r{base.x + dx, base.y + dy, base.w, base.h};
    card.fill_rect(r, color);
  };

  // Draw frame from back to front
  draw_frame_layer(photo_location, 0, 0, photo_light_bg);
  draw_frame_layer(photo_location, 2, -2, photo_darker_bg);
  draw_frame_layer(photo_location, 4, -4, photo_white_bg);

  card.blit_raw(picture,
                ViewPoint{photo_location.x + 11, photo_location.y + 3});

  // Render character name
  constexpr int name_x_offset = 160;
  context_.font->render_text_onto(card, data.first_name,
                                  ViewPoint{name_x_offset, 34},
                                  FontFace::ASTANDAR, TEXT_COLOR_BLACK);
  context_.font->render_text_onto(card, data.last_name,
                                  ViewPoint{name_x_offset, 56},
                                  FontFace::ASTANDAR, TEXT_COLOR_BLACK);

  // Age, payment, pizzas/day
  constexpr int label_x_offset = name_x_offset - 8;

  context_.font->render_text_onto(card, std::to_string(data.age) + " YEARS",
                                  ViewPoint{label_x_offset, 90},
                                  FontFace::CSTANDAR, TEXT_COLOR_BLACK);

  context_.font->render_text_onto(card, "PAYMENT",
                                  ViewPoint{label_x_offset, 120},
                                  FontFace::CSTANDAR, TEXT_COLOR_BLACK);

  context_.font->render_text_onto(card, std::to_string(payment_dollars),
                                  ViewPoint{-32, 120}, FontFace::CSTANDAR,
                                  TEXT_COLOR_BLACK, AlignX::Right);

  context_.font->render_text_onto(card, "PIZZAS/DAY",
                                  ViewPoint{label_x_offset, 138},
                                  FontFace::CSTANDAR, TEXT_COLOR_BLACK);

  context_.font->render_text_onto(card, std::to_string(data.pizzas_per_day),
                                  ViewPoint{-32, 138}, FontFace::CSTANDAR,
                                  TEXT_COLOR_BLACK, AlignX::Right);

  // Character stat bars
  struct StatEntry {
    const char* label;
    uint8_t value;
  };

  const StatEntry stat_entries[] = {
      {"COOKING", data.cooking},
      {"SERVING", data.serving},
      {"MANAGEMENT", data.management},
      {"EXPERIENCE", data.experience},
      {"INTELLIGENCE", data.intelligence},
      {"TRUSTWORTHINESS", data.trustworthiness},
      {"HEALTH", data.health},
      {"MOTIVATION", data.motivation},
  };

  constexpr int statbar_x_start = 8;
  constexpr int statbar_y_start = 168;
  constexpr int statbar_y_spacing = 36;
  constexpr int statbar_column_offset = 178;
  constexpr int statbar_max_rows = 4;

  int x = statbar_x_start;
  int y = statbar_y_start;

  StatBar statbar(context_.blitmap, context_.font);
  for (std::size_t i = 0; i < std::size(stat_entries); ++i) {
    const auto& entry = stat_entries[i];
    auto bar = statbar.create_stat_bar(entry.label, entry.value);
    card.blit_masked(bar, ViewPoint{x, y});

    if ((i + 1) % statbar_max_rows == 0) {
      x += statbar_column_offset;
      y = statbar_y_start;
    }
    else {
      y += statbar_y_spacing;
    }
  }

  return card;
}

std::pair<std::string, std::string>
StaffView::load_application_text(uint8_t character_id) {
  // Algorithm to read the right lines from bewerb.txt
  const unsigned i = character_id;
  const unsigned j = i % 100;
  const unsigned bucket = j / 20; // 0..4
  const unsigned k = j % 20;      // 0..19
  const int line1 = static_cast<int>((2 * k) + 1);
  const int line2 = line1 + 1;

  // Build filename
  std::string filename = compat::format("t/bewerb{}.e", bucket);

  Logger::debug("Loading application text from {} lines {},{}", filename, line1,
                line2);

  auto is = fileutils::open(filename);

  // Read all lines
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(is, line)) {
    lines.push_back(line);
  }

  // Get the two lines (1-indexed in the file)
  std::string text1 = (line1 > 0 && line1 <= static_cast<int>(lines.size()))
                          ? lines[line1 - 1]
                          : "";
  std::string text2 = (line2 > 0 && line2 <= static_cast<int>(lines.size()))
                          ? lines[line2 - 1]
                          : "";

  return {text1, text2};
}

Surface StaffView::create_application_card(uint8_t character_id) {
  auto* char_data = CharacterFactory::create_character(character_id);
  auto card = create_empty_card(job_string(job_));

  // Render portrait with frame
  RawRenderer r;
  auto picture = r.surface(*char_data);

  // Photo frame background colors
  uint8_t photo_white_bg = 15;
  uint8_t photo_darker_bg = 11;
  uint8_t photo_light_bg = 12;
  ViewRect photo_location(8, 30, 118, 132);

  // Local lambda to draw a frame layer
  auto draw_frame_layer = [&card](const ViewRect& base, int dx, int dy,
                                  uint8_t color) {
    ViewRect r{base.x + dx, base.y + dy, base.w, base.h};
    card.fill_rect(r, color);
  };

  // Draw frame from back to front
  draw_frame_layer(photo_location, 0, 0, photo_light_bg);
  draw_frame_layer(photo_location, 2, -2, photo_darker_bg);
  draw_frame_layer(photo_location, 4, -4, photo_white_bg);

  // Blit character picture onto card
  card.blit_raw(picture,
                ViewPoint{photo_location.x + 11, photo_location.y + 3});

  // Render character name
  constexpr int name_x_offset = 160;
  context_.font->render_text_onto(card, char_data->first_name,
                                  ViewPoint{name_x_offset, 34},
                                  FontFace::ASTANDAR, TEXT_COLOR_BLACK);
  context_.font->render_text_onto(card, char_data->last_name,
                                  ViewPoint{name_x_offset, 56},
                                  FontFace::ASTANDAR, TEXT_COLOR_BLACK);

  // Render "Job Application" label (where age would be on data card)
  constexpr int name_x = 160;
  constexpr int label_x = name_x - 8;
  context_.font->render_text_onto(card, "JOB APPLICATION",
                                  ViewPoint{label_x, 90}, FontFace::CSTANDAR,
                                  TEXT_COLOR_BLACK);

  // Load the application text (with soft hyphens)
  auto [line1, line2] = load_application_text(character_id);
  std::string full_text = line1;
  if (!line2.empty()) {
    full_text += " " + line2;
  }

  // Set up text flow around the picture
  // Picture is at (19, 33) with size 104x118 (PICTURE_WIDTH*2,
  // PICTURE_HEIGHT*2)
  constexpr int picture_x = 19;
  constexpr int picture_y = 33;
  constexpr int picture_width = PICTURE_WIDTH * 2;   // 104
  constexpr int picture_height = PICTURE_HEIGHT * 2; // 118
  constexpr int text_start_x =
      picture_x + picture_width + 10; // 10px spacing after picture
  constexpr int text_start_y = 120;   // Below the "Job Application" label
  constexpr int text_left_margin = 20;
  constexpr int text_right_margin = CARD_WIDTH - 20;

  FlowColumns flow{.cutoff_y =
                       picture_y + picture_height + 10, // 10px below picture
                   .left_narrow = text_start_x,         // Next to picture
                   .left_wide = text_left_margin, // Full width below picture
                   .right = text_right_margin};

  // Render the flowing text with soft hyphen support
  context_.font->render_text_onto(card, full_text,
                                  ViewPoint{text_start_x, text_start_y},
                                  FontFace::CSTANDAR, 5, AlignX::Left, flow);

  return card;
}

Surface StaffView::create_empty_card(const std::string& label) {
  // Cache the base card structure (without the tab label)
  if (!empty_card_surface_) {
    empty_card_surface_ =
        Surface(views::base::MAIN_PANEL_WIDTH, views::base::MAIN_PANEL_HEIGHT);

    auto tab_surf = blitmap3_.icon_surface(Blitmap3Icon::FILE_TAB);
    // Blit the tab background (without text) onto the cached surface
    int dst_x = CARD_WIDTH - tab_surf.width();
    int dst_y = 0;
    empty_card_surface_->blit_raw(tab_surf, ViewPoint{dst_x, dst_y});

    auto bg_surf = blitmap3_.icon_surface(Blitmap3Icon::GREEN_BG);
    const int tile_w = bg_surf.width();
    const int tile_h = bg_surf.height();

    for (int y = 0; y < MAX_CARD_HEIGHT - tab_surf.height(); y += tile_h) {
      for (int x = 0; x < CARD_WIDTH; x += tile_w) {
        int dst_x_tile = x;
        int dst_y_tile = tab_surf.height() + y;

        // Compute the area we can safely draw
        int remaining_w = CARD_WIDTH - x;
        int remaining_h = MAX_CARD_HEIGHT - tab_surf.height() - y;

        ViewRect clip = {0, 0, std::min(tile_w, remaining_w),
                         std::min(tile_h, remaining_h)};

        empty_card_surface_->blit_raw(bg_surf,
                                      ViewPoint{dst_x_tile, dst_y_tile});
      }
    }
    auto binder_left = blitmap3_.icon_surface(Blitmap3Icon::BINDER_LEFT);
    empty_card_surface_->blit_raw(binder_left, ViewPoint{0, 300});

    auto binder_right = blitmap3_.icon_surface(Blitmap3Icon::BINDER_RIGHT);
    empty_card_surface_->blit_raw(
        binder_right, ViewPoint{CARD_WIDTH - binder_right.width(), 300});
  }

  // Clone the cached card and add the label (not cached)
  auto this_card = empty_card_surface_->clone();

  // Now render the label onto this specific card's tab
  if (!label.empty()) {
    int tab_x =
        CARD_WIDTH - blitmap3_.icon_surface(Blitmap3Icon::FILE_TAB).width();
    context_.font->render_text_onto(this_card, label, ViewPoint{tab_x + 10, 4},
                                    FontFace::CSTANDAR, TEXT_COLOR_BLACK);
  }

  return this_card;
}

void StaffView::handle_left_mouse_click(ViewPoint /*p*/) {
  assert(input_panel_ != nullptr);

  // If a hired employee card is visible on overview, clicking anywhere closes
  // it
  if (screen_ == StaffScreen::OVERVIEW &&
      card_animator_.state() == SlideAnimator::SlideState::VISIBLE &&
      selected_slot_index_.has_value()) {
    card_animator_.start_hide();
    // Keep selected_slot_index_ until animation completes (cleared in tick())
    return;
  }
}

void StaffView::on_staff_slot_clicked(core::character::JobRole job,
                                      int slot_index) {
  auto* restaurant = state_.active_restaurant();
  if (!restaurant) {
    return;
  }

  // Handle manager separately (single slot)
  if (job == core::character::JobRole::MANAGER) {
    const auto& manager_opt = restaurant->manager();
    if (manager_opt.has_value()) {
      selected_slot_index_ = 0;
      selected_character_ = nullptr; // Not needed for hired employees

      // Use Staff overload to show payment
      fg_card_ = create_data_card(*manager_opt);
      card_animator_.start_show();
      // Stay on OVERVIEW so buttons remain visible
    }
    return;
  }

  // Get the appropriate staff list based on job
  const auto& staff_list = (job == core::character::JobRole::CHEF)
                               ? restaurant->cooks()
                               : restaurant->waiters();

  // Check if slot is valid and has a staff member
  if (slot_index >= 0 && slot_index < static_cast<int>(staff_list.size())) {
    const auto& staff_opt = staff_list[slot_index];
    if (staff_opt.has_value()) {
      // Store the selected slot
      selected_slot_index_ = slot_index;
      selected_character_ = nullptr; // Not needed for hired employees

      // Create and show the character card using Staff overload
      fg_card_ = create_data_card(*staff_opt);
      card_animator_.start_show();

      // Stay on OVERVIEW so fire/wages/training buttons remain visible
    }
  }
}

void StaffView::change_screen(StaffScreen screen) {
  std::cout << "Change screen to " << magic_enum::enum_name(screen) << "\n";
  screen_ = screen;
  input_panel_needs_update_ = true;
  mark_dirty();
}

void StaffView::change_job(core::character::JobRole job) {
  if (job == job_) {
    return;
  }
  Logger::info("Change job at state {}", magic_enum::enum_name(screen_));

  // If viewing a hired employee card on overview, close it
  if (screen_ == StaffScreen::OVERVIEW && selected_slot_index_.has_value()) {
    card_animator_.start_hide();
    // Clear selection immediately when switching categories to avoid
    // highlighting wrong row in the new category during animation
    selected_slot_index_.reset();
  }

  job_ = job;
  empty_staff_slots_surface_.reset();

  // If viewing an applicant card or applications, switch to first candidate of
  // new job role
  if (screen_ == StaffScreen::APPLICANT_CARD ||
      screen_ == StaffScreen::APPLICATIONS) {
    Logger::info("Switching to first {} candidate\n",
                 magic_enum::enum_name(job));
    show_applicant_card(job);
  }

  mark_dirty();
}

void StaffView::show_character(uint8_t char_id) {
  active_character_id_ = char_id;

  // Build a temporary Staff wrapper just to reuse create_data_card()
  // CharacterData* cd = CharacterFactory::create_character(char_id);
  auto* cd = applicant_pool_[job_].front();
  // Staff dummy(job_, Character(cd));

  fg_card_ = create_data_card(cd, job_);
  card_animator_.reset();
  card_animator_.start_show(); // bring the card in
  mark_dirty();
}

void StaffView::add_common_buttons() {
  using jr = core::character::JobRole;
  // Job selection tabs (always present)
  input_panel_->add_button(Icon::CHEFS_HAT, ButtonType::Square,
                           ButtonPosition::TopLeft, "CHEFS",
                           [this]() { change_job(jr::CHEF); });
  input_panel_->add_button(Icon::BOW_TIE, ButtonType::Square,
                           ButtonPosition::TopCenter, "WAITERS",
                           [this]() { change_job(jr::WAITER); });
  input_panel_->add_button(Icon::TIE, ButtonType::Square,
                           ButtonPosition::TopRight, "MANAGERS",
                           [this]() { change_job(jr::MANAGER); });

  // Back button (always present)
  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "BACK", [this]() {
                             if (screen_ == StaffScreen::OVERVIEW) {
                               state_.ui().pop();
                             }
                             else {
                               card_animator_.start_hide();
                               change_screen(StaffScreen::OVERVIEW);
                             }
                           });
}

void StaffView::add_overview_buttons() {
  // Fire button - shows alert if no staff selected
  input_panel_->add_button(
      Icon::BOOT, ButtonType::Square, ButtonPosition::MiddleLeft, "FIRE",
      [this]() {
        if (!selected_slot_index_.has_value()) {
          context_.popup->origin(ViewPoint{52, 80});
          context_.popup->create<AlertPopup>(
              "To fire somebody, you must first select that person!");
          return;
        }

        int slot_index = selected_slot_index_.value();
        auto* restaurant = state_.active_restaurant();

        const Staff* staff_ptr = nullptr;
        if (job_ == core::character::JobRole::MANAGER) {
          if (restaurant->manager().has_value()) {
            staff_ptr = &restaurant->manager().value();
          }
        }
        else {
          const auto& staff_array = (job_ == core::character::JobRole::CHEF)
                                        ? restaurant->cooks()
                                        : restaurant->waiters();
          if (slot_index >= 0 &&
              slot_index < static_cast<int>(staff_array.size()) &&
              staff_array[slot_index].has_value()) {
            staff_ptr = &staff_array[slot_index].value();
          }
        }

        if (staff_ptr == nullptr) {
          return;
        }

        if (staff_ptr->is_absent()) {
          const auto& ch = staff_ptr->character();
          context_.popup->origin(ViewPoint{52, 80});
          context_.popup->create<AlertPopup>(
              compat::format("{} {} is away right now and cannot be fired!",
                             ch.first_name(), ch.last_name()));
          return;
        }

        uint8_t character_id = staff_ptr->character().id();

        context_.popup->origin(ViewPoint{52, 80});
        context_.popup->create<ConfirmPopup>(
            "Are you sure you want to fire me?",
            [this, slot_index, character_id](bool sure) {
              if (sure) {
                auto* restaurant = state_.active_restaurant();
                auto* city = state_.active_city();
                bool fired = restaurant->fire_staff(job_, slot_index);
                if (fired && character_id != 0) {
                  Logger::info("Fired staff member with ID {}", character_id);
                  city->add_to_blacklist(character_id, 30);
                  selected_slot_index_.reset();
                  card_animator_.start_hide();
                }
                else {
                  Logger::warn("Failed to fire staff member");
                }
              }
              mark_dirty();
            });
      });

  // Wages button - shows alert if no staff selected
  input_panel_->add_button(
      Icon::MONEY_BAG, ButtonType::Square, ButtonPosition::MiddleCenter,
      "WAGES", [this]() {
        if (!selected_slot_index_.has_value()) {
          context_.popup->origin(ViewPoint{52, 80});
          context_.popup->create<AlertPopup>(
              "To change somebody's wages, you must first select that person!");
          return;
        }

        // Get current staff payment to calculate dynamic range
        int current_wage = 320; // default fallback
        Restaurant* restaurant = state_.active_restaurant();
        if (restaurant) {
          if (job_ == core::character::JobRole::MANAGER) {
            auto& manager = restaurant->manager();
            if (manager.has_value()) {
              current_wage = manager->payment().whole();
            }
          }
          else {
            auto& staff_list = (job_ == core::character::JobRole::CHEF)
                                   ? restaurant->cooks()
                                   : restaurant->waiters();
            int slot = *selected_slot_index_;
            if (slot >= 0 && slot < static_cast<int>(staff_list.size()) &&
                staff_list[slot].has_value()) {
              current_wage = staff_list[slot]->payment().whole();
            }
          }
        }

        int min_wage = current_wage / 2;
        int max_wage = current_wage * 2;
        int step = 50; // $50 increments

        context_.popup->origin(ViewPoint{52, 80});
        context_.popup->create<NumberInputPopup>(
            "Change payment", "$", current_wage, min_wage, max_wage, step,
            [this](int new_wage_dollars) {
              if (!selected_slot_index_.has_value()) {
                return;
              }

              // Get the staff member being edited
              Restaurant* restaurant = state_.active_restaurant();
              if (!restaurant) {
                return;
              }

              Staff* staff = nullptr;
              int old_wage_dollars = 0;

              // Get staff based on current job and slot
              if (job_ == core::character::JobRole::MANAGER) {
                auto& manager = restaurant->manager();
                if (!manager.has_value()) {
                  return;
                }
                staff = &(*manager);
                old_wage_dollars = manager->payment().whole();
              }
              else {
                auto& staff_list = (job_ == core::character::JobRole::CHEF)
                                       ? restaurant->cooks()
                                       : restaurant->waiters();
                int slot = *selected_slot_index_;
                if (slot < 0 || slot >= static_cast<int>(staff_list.size()) ||
                    !staff_list[slot].has_value()) {
                  return;
                }
                staff = &(*staff_list[slot]);
                old_wage_dollars = staff_list[slot]->payment().whole();
              }

              // Calculate wage percentage change
              int wage_pct = (new_wage_dollars * 100) / old_wage_dollars;

              // Calculate motivation multiplier (ebp) based on wage change
              int difficulty = state_.difficulty_level(); // 1-5
              int difficulty_divisor = (difficulty / 2) + 1;

              int motivation_change = 0;

              if (wage_pct > 100) {
                int temp = (wage_pct - 100) * 2;
                int ebp = (temp / difficulty_divisor) + 100;

                // Calculate motivation change: (old_mot * ebp / 100) - old_mot
                int old_motivation = staff->character().motivation();
                int new_motivation_calc = (old_motivation * ebp) / 100;
                motivation_change = new_motivation_calc - old_motivation;
              }
              else {
                // For wage_pct=100 (no change): mot becomes half
                // For wage_pct=90 (10% cut): mot becomes 45% of original
                int half_pct = wage_pct / 2;
                int old_motivation = staff->character().motivation();
                int new_motivation_calc = (old_motivation * half_pct) / 100;
                motivation_change = new_motivation_calc - old_motivation;
              }

              // Update payment first
              staff->payment(Money{new_wage_dollars * 100});

              // Adjust motivation (will clamp to 0-100)
              staff->adjust_motivation(motivation_change);

              // Recalculate pizzas_per_day ONLY on wage increase
              // (wage decrease/no-change does NOT affect ppd, only motivation)
              if (wage_pct > 100) {
                int old_ppd = staff->character().pizzas_per_day();
                int new_ppd;

                // Wage increase: use (ebp + 120) / 2 multiplier (from lines
                // 329-334)
                int temp = (wage_pct - 100) * 2;
                int ebp = (temp / difficulty_divisor) + 100;

                if (ebp > 100) {
                  int multiplier =
                      (ebp + 120) / 2; // e.g., (124 + 120) / 2 = 122
                  new_ppd = (old_ppd * multiplier) / 100;
                }
                else {
                  new_ppd = (old_ppd * ebp) / 100;
                }

                // Cap at 2000
                new_ppd = std::min(new_ppd, 2000);

                staff->pizzas_per_day(new_ppd);
              }
              // else: wage decrease/no-change - ppd stays unchanged

              // Log the change for debugging
              if (wage_pct > 100) {
                Logger::info(
                    "[Staff] Wage ${} -> ${} ({}%), motivation: {:+d}, ppd "
                    "updated",
                    old_wage_dollars, new_wage_dollars, wage_pct,
                    motivation_change);
              }
              else {
                Logger::info(
                    "[Staff] Wage ${} -> ${} ({}%), motivation: {:+d}, ppd "
                    "unchanged",
                    old_wage_dollars, new_wage_dollars, wage_pct,
                    motivation_change);
              }

              // Refresh display
              card_animator_.start_hide();
              mark_dirty();
            });
      });

  // Training button - shows alert if no staff selected
  input_panel_->add_button(Icon::FORM, ButtonType::Square,
                           ButtonPosition::MiddleRight, "TRAINING", [this]() {
                             if (!selected_slot_index_.has_value()) {
                               context_.popup->origin(ViewPoint{52, 80});
                               context_.popup->create<AlertPopup>(
                                   "To send somebody on a course, you must "
                                   "first select that person!");
                               return;
                             }
                             change_screen(StaffScreen::TRAINING);
                           });

  // Recruit button
  input_panel_->add_button(Icon::SHOPPING_CART, ButtonType::Horizontal,
                           ButtonPosition::BottomLeft, "RECRUIT",
                           [this]() { show_applicant_card(job_); });
}

void StaffView::add_hired_employee_buttons() {
  // When viewing a hired employee card, we don't show action buttons
  // The card closes on click
}

void StaffView::add_applicant_buttons() {
  // Up arrow: move to next candidate (push - new card slides down from top)
  input_panel_->add_button(
      Icon::ARROW_UP, ButtonType::Square, ButtonPosition::MiddleLeft,
      "TURN PAGE", [this]() {
        if (card_animator_.is_animating()) {
          return;
        }
        auto& vec = applicant_pool_[job_];
        if (vec.empty()) {
          return;
        }
        // Move to next candidate
        applicant_cursor_[job_] = (applicant_cursor_[job_] + 1) % vec.size();
        Logger::debug("Moving to next candidate, index: {}",
                      applicant_cursor_[job_]);

        if (screen_ == StaffScreen::APPLICATIONS) {
          // In applications view, switch to data card with animation
          auto* char_data = vec[applicant_cursor_[job_]];
          auto new_card = create_data_card(char_data, job_);
          bg_card_ = std::move(*fg_card_);
          fg_card_.reset(); // Clear fg_card_ after moving from it
          fg_card_ = std::move(new_card);
          change_screen(StaffScreen::APPLICANT_CARD);
          card_animator_.push(); // Slide down from top
        }
        else {
          // In data card view, animate the transition
          auto new_card = make_card(job_, applicant_cursor_[job_]);
          bg_card_ = std::move(*fg_card_);
          fg_card_.reset(); // Clear fg_card_ after moving from it
          fg_card_ = std::move(new_card);
          card_animator_.push(); // Slide down from top
        }
        mark_dirty();
      });

  input_panel_->add_button(Icon::BAR_CHART, ButtonType::Square,
                           ButtonPosition::MiddleCenter, "DATA", [this]() {
                             // Switch to data card (character stats)
                             if (screen_ == StaffScreen::APPLICATIONS) {
                               // Get current candidate's ID
                               auto& vec = applicant_pool_[job_];
                               if (!vec.empty() &&
                                   applicant_cursor_[job_] < vec.size()) {
                                 auto* char_data = vec[applicant_cursor_[job_]];
                                 fg_card_ = create_data_card(char_data, job_);
                                 change_screen(StaffScreen::APPLICANT_CARD);
                               }
                             }
                           });

  input_panel_->add_button(
      Icon::FORM, ButtonType::Square, ButtonPosition::MiddleRight,
      "APPLICATIONS", [this]() {
        // Switch to application text card
        if (screen_ == StaffScreen::APPLICANT_CARD) {
          // Get current candidate's ID
          auto& vec = applicant_pool_[job_];
          if (!vec.empty() && applicant_cursor_[job_] < vec.size()) {
            auto* char_data = vec[applicant_cursor_[job_]];
            fg_card_ = create_application_card(char_data->id);
            change_screen(StaffScreen::APPLICATIONS);
          }
        }
      });

  // Down arrow: move to previous candidate (pop - current slides up revealing
  // previous)
  input_panel_->add_button(
      Icon::ARROW_DOWN, ButtonType::Square, ButtonPosition::BottomLeft,
      "TURN PAGE", [this]() {
        if (card_animator_.is_animating()) {
          return;
        }
        auto& vec = applicant_pool_[job_];
        if (vec.empty()) {
          return;
        }

        // Move to previous candidate
        std::size_t prev = (applicant_cursor_[job_] == 0)
                               ? vec.size() - 1
                               : applicant_cursor_[job_] - 1;
        Logger::debug("Moving to previous candidate, index: {}", prev);

        applicant_cursor_[job_] = prev;

        if (screen_ == StaffScreen::APPLICATIONS) {
          // In applications view, switch to data card with animation
          auto* char_data = vec[applicant_cursor_[job_]];
          bg_card_ = create_data_card(char_data, job_);
          change_screen(StaffScreen::APPLICANT_CARD);
          card_animator_.pop(); // Current slides up
        }
        else {
          // In data card view, animate the transition
          bg_card_ = make_card(job_, prev);
          card_animator_.pop(); // Current slides up
        }
        mark_dirty();
      });

  input_panel_->add_button(
      Icon::THUMBS_UP, ButtonType::Square, ButtonPosition::BottomCenter, "HIRE",
      [this]() {
        auto& vec = applicant_pool_[job_];
        if (vec.empty() || applicant_cursor_[job_] >= vec.size()) {
          return;
        }

        auto* char_data = vec[applicant_cursor_[job_]];
        std::string job_name = (job_ == core::character::JobRole::CHEF) ? "cook"
                               : (job_ == core::character::JobRole::WAITER)
                                   ? "waiter"
                                   : "manager";
        std::string message = compat::format(
            "Do you want to hire {} {} as {} in this restaurant?",
            char_data->first_name, char_data->last_name, job_name);

        context_.popup->origin(ViewPoint{26, 44});
        context_.popup->create<ConfirmPopup>(
            message,
            [this, char_data](bool confirmed) {
              if (confirmed) {
                Logger::debug("Hiring {} {}", char_data->first_name,
                              char_data->last_name);

                // Check if character is blacklisted
                auto* city = state_.active_city();
                if (!city) {
                  Logger::warn("No active city");
                  return;
                }

                if (city->is_blacklisted(char_data->id)) {
                  int days_remaining =
                      city->blacklist_days_remaining(char_data->id);
                  context_.popup->origin(ViewPoint{52, 80});
                  context_.popup->create<AlertPopup>(
                      "I don't want to work for you again!");
                  Logger::debug("Char id {} is refusing for {} days",
                                char_data->id, days_remaining);
                  return;
                }

                core::character::Character new_character(char_data);

                int starting_pay_dollars = char_data->starting_payment(job_);

                Staff new_staff(job_, new_character);
                new_staff.payment(Money{starting_pay_dollars * 100});

                // Add to restaurant
                auto* restaurant = state_.active_restaurant();
                if (!restaurant) {
                  Logger::warn("No active restaurant");
                  return;
                }

                HiringResult result = restaurant->add_staff(new_staff);

                if (result == HiringResult::Success) {
                  Logger::info("Successfully hired {} {}",
                               char_data->first_name, char_data->last_name);

                  // Remove hired candidate from applicant pool
                  auto& vec = applicant_pool_[job_];
                  vec.erase(vec.begin() + applicant_cursor_[job_]);

                  // Adjust cursor if needed
                  if (applicant_cursor_[job_] >= vec.size() && !vec.empty()) {
                    applicant_cursor_[job_] = vec.size() - 1;
                  }

                  // Clear cards and go directly to overview (no animation)
                  fg_card_.reset();
                  bg_card_.reset();
                  card_animator_.reset(); // Reset animator to HIDDEN state
                  change_screen(StaffScreen::OVERVIEW);
                }
                else if (result == HiringResult::NoSlotsAvailable) {
                  context_.popup->origin(ViewPoint{52, 80});
                  context_.popup->create<AlertPopup>(
                      "No slots available for this job role!");
                }
                else if (result == HiringResult::AlreadyHasManager) {
                  context_.popup->origin(ViewPoint{52, 80});
                  context_.popup->create<AlertPopup>(
                      "You already have a manager!");
                }
              }
            },
            char_data);
      });
}

void StaffView::create_input_panel() {
  input_panel_needs_update_ = false;
  std::cout << "Creating input panel for " << magic_enum::enum_name(screen_)
            << "\n";
  input_panel_->delete_buttons();

  add_common_buttons();

  // Add screen-specific buttons
  switch (screen_) {
  case StaffScreen::OVERVIEW:
    add_overview_buttons();
    break;
  case StaffScreen::HIRED_EMPLOYEE_CARD:
    add_hired_employee_buttons();
    break;
  case StaffScreen::APPLICANT_CARD:
  case StaffScreen::APPLICATIONS:
    // Both applicant screens use the same buttons
    add_applicant_buttons();
    break;
  case StaffScreen::TRAINING:
  case StaffScreen::MANAGER_SETTINGS:
    break;
  }
}

std::string StaffView::job_string(core::character::JobRole job) const {
  switch (job) {
  case core::character::JobRole::CHEF:
    return "COOK";
  case core::character::JobRole::WAITER:
    return "WAITER";
  case core::character::JobRole::MANAGER:
    return "MANAGER";
  case core::character::JobRole::NONE:
    return "NONE";
  }
  return "";
}

void StaffView::load_applicant_pool() {
  // Collect all player character IDs to exclude them from applicants
  std::set<uint8_t> player_char_ids;
  for (const auto* player : state_.players()) {
    if (player) {
      player_char_ids.insert(player->character().id());
      Logger::debug("Excluding player character ID: {} ({})",
                    player->character().id(), player->character().first_name());
    }
  }

  auto is = fileutils::open("chr/amarkt");
  if (!is) {
    fatal("Can't open chr/amarkt!");
  }

  uint8_t count = 0;
  is.read(reinterpret_cast<char*>(&count), 1);

  for (uint8_t i = 0; i < count; ++i) {
    uint8_t role_raw = 0;
    is.read(reinterpret_cast<char*>(&role_raw), 1);

    core::character::JobRole role;
    switch (role_raw) {
    case 0:
      role = core::character::JobRole::CHEF;
      break;
    case 1:
      role = core::character::JobRole::WAITER;
      break;
    case 2:
      role = core::character::JobRole::MANAGER;
      break;
    default:
      Logger::debug("Invalid role byte {} at index {}", role_raw, i);
      continue;
    }

    auto* char_data = CharacterFactory::create_character(i);

    // Skip if this character is one of the players
    if (player_char_ids.contains(char_data->id)) {
      Logger::debug("Skipping player character {} from applicant pool",
                    char_data->id);
      continue;
    }

    applicant_pool_[role].push_back(char_data);
  }
}

void StaffView::show_applicant_card(core::character::JobRole role) {
  const auto& vec = applicant_pool_[role];
  if (vec.empty()) {
    return;
  }

  // create waiter card surface
  std::size_t first = 0;
  Surface new_card = make_card(role, first);

  // case 1: a card is already visible
  if (card_animator_.is_visible()) {
    bg_card_ = std::move(fg_card_); // current chef card becomes bg
    fg_card_ = std::move(new_card); // waiter card on top
    applicant_cursor_[role] = first;

    card_animator_.push(); // fg slides in over bg
  }
  // case 2: no card showing yet
  else {
    fg_card_ = std::move(new_card);
    bg_card_.reset();
    applicant_cursor_[role] = first;

    card_animator_.start_show(); // slide it in from bottom
  }
  change_screen(StaffScreen::APPLICANT_CARD);
  mark_dirty();
}

Surface StaffView::make_card(core::character::JobRole role, std::size_t idx) {
  const auto& vec = applicant_pool_[role];
  if (idx >= vec.size()) {
    fatal("Invalid index in make_card");
  }

  Logger::debug("make_card: role={}, idx={}, character_id={}",
                magic_enum::enum_name(role), idx, vec[idx]->id);

  // Use the CharacterData directly from the applicant pool
  return create_data_card(vec[idx], role);
}

void StaffView::navigate_to_restaurant(core::RestaurantId new_id) {
  state_.active_restaurant(new_id);
  state_.ui().replace_top<ScreenTag>(ScreenTag::Kind::StaffView);
}
