/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/StaffView.h"

#include <algorithm>

#include "core/character/Character.h"
#include "core/city/JobMarket.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/RestaurantStats.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/util/LegacyConstants.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionEvent.h"
#include "factories/CharacterFactory.h"
#include "factories/TrainingFactory.h"
#include "gfx/ColorIndex.h"
#include "gfx/SurfaceBuilder.h"
#include "magic_enum/magic_enum.hpp"
#include "resources/BlitmapIcons.h"
#include "resources/FontTypes.h"
#include "views/base/ViewDefs.h"
#include "views/helpers/CardGfx.h"
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

// Training course list layout (card-local coordinates). The list opens with a
// column-header row, then one row per course TRAINING_ROW_H apart.
static constexpr int TRAINING_LIST_Y = 172; // column-header row
static constexpr int TRAINING_ROW_H = 20;
static constexpr int TRAINING_COURSE_Y0 = TRAINING_LIST_Y + TRAINING_ROW_H;
static constexpr int TRAINING_BTN_X = 18;
static constexpr int TRAINING_TEXT_X = 50;   // left edge of the name column
static constexpr int TRAINING_SCORE_X = 245; // right edge of required-score col
static constexpr int TRAINING_COST_X = 330;  // right edge of cost column
static constexpr int TRAINING_DAYS_X = 360;  // right edge of days column

namespace {

// The cook or waiter slot array matching a job. Manager is a single slot and
// handled separately by callers.
auto& staff_array_for(Restaurant& r, core::character::JobRole job) {
  return job == core::character::JobRole::CHEF ? r.cooks() : r.waiters();
}

// Map a job role to the skill its training courses improve.
SkillType skill_for_job(core::character::JobRole job) {
  switch (job) {
  case core::character::JobRole::CHEF:
    return SkillType::COOKING;
  case core::character::JobRole::WAITER:
    return SkillType::SERVING;
  case core::character::JobRole::MANAGER:
    return SkillType::MANAGEMENT;
  case core::character::JobRole::NONE:
    return SkillType::NONE;
  }
  return SkillType::NONE;
}

// The character's score in the skill relevant to their job. Trainings gate on
// this value against their min_level requirement.
int relevant_skill_value(const core::character::Character& c,
                         core::character::JobRole job) {
  switch (job) {
  case core::character::JobRole::CHEF:
    return c.cooking();
  case core::character::JobRole::WAITER:
    return c.serving();
  case core::character::JobRole::MANAGER:
    return c.management();
  case core::character::JobRole::NONE:
    return 0;
  }
  return 0;
}

std::string_view skill_label(core::character::JobRole job) {
  switch (job) {
  case core::character::JobRole::CHEF:
    return "COOKING";
  case core::character::JobRole::WAITER:
    return "SERVING";
  case core::character::JobRole::MANAGER:
    return "MANAGEMENT";
  case core::character::JobRole::NONE:
    return "";
  }
  return "";
}

} // namespace

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

static constexpr int hire_time_blocks = 4;
static constexpr int applicant_exit_time_blocks = 1;

StaffView::StaffView(GameContext& context, GameState& state)
    : RestaurantSubView(context, state, state.active_restaurant()->id()),
      blitmap3_(context_.blitmap->get<Blitmap3Icon>()) {
  load_applicant_pool();
  create_input_panel();
  // When a card finishes sliding away, drop the selection and return to the
  // overview screen.
  card_slider_.on_hidden([this] {
    selected_slot_index_.reset();
    pressed_training_row_.reset();
    if (screen_ != StaffScreen::OVERVIEW) {
      change_screen(StaffScreen::OVERVIEW);
    }
  });
  interactions_.create_persistent_item(
      ViewRect{LegacyConstants::WOOD_BORDER_WIDTH_LEFT,
               LegacyConstants::WOOD_BORDER_HEIGHT_TOP,
               views::base::MAIN_PANEL_WIDTH, views::base::MAIN_PANEL_HEIGHT},
      "staffview_catchall", [this](views::ui::InteractionEvent ie) {
        if (ie.type == views::ui::InteractionEvent::Type::Click &&
            ie.button == views::ui::MouseButton::Left) {
          handle_left_mouse_click(ie.local);
          return;
        }
        if (ie.type == views::ui::InteractionEvent::Type::Scroll) {
          const bool on_applicant_screen =
              screen_ == StaffScreen::APPLICANT_CARD ||
              screen_ == StaffScreen::APPLICATIONS;
          if (!on_applicant_screen) {
            return;
          }
          if (ie.scroll_delta > 0) {
            handle_next_applicant();
          }
          else if (ie.scroll_delta < 0) {
            handle_previous_applicant();
          }
        }
      });
}

void StaffView::tick() {
  if (card_slider_.update()) {
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
  if (screen_ == StaffScreen::OVERVIEW && card_slider_.idle()) {
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
      const auto& staff_array = staff_array_for(*restaurant, job_);

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
      screen_ == StaffScreen::OVERVIEW && card_slider_.idle()) {
    draw_manager_settings_interactions();
  }

  // Training course buttons (only once the card has finished sliding up)
  if (screen_ == StaffScreen::TRAINING && card_slider_.settled()) {
    draw_training_interactions();
  }

  // Background card (during push/pop) and the foreground card (may be sliding).
  card_slider_.render(*ctx.surface, CARD_OFFSET_X);

  // While a training button is held down, overlay the pressed (flipped-bevel)
  // sprite over its resting one. Only meaningful when the card sits still.
  if (screen_ == StaffScreen::TRAINING && pressed_training_row_ &&
      card_slider_.settled()) {
    auto pressed = blitmap3_.icon_surface(Blitmap3Icon::DIFFERENT_GRAY_BOX);
    int row_y = TRAINING_COURSE_Y0 + (*pressed_training_row_ * TRAINING_ROW_H);
    ctx.surface->blit_raw(
        pressed, {CARD_OFFSET_X + TRAINING_BTN_X, CARD_OFFSET_Y + row_y});
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
        pressed_settings_row_ == i
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
        pressed_settings_row_ == settings_row
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
        pressed_settings_row_ == settings_row
            ? views::ui::components::ButtonState::Pressed
            : views::ui::components::ButtonState::Normal);
  }
  y += MS_ROW_H;
  draw_green_area(surface, y, MAX_CARD_HEIGHT - y);

  return surface;
}

views::ui::InteractionLayers::Callback
StaffView::settings_button_handler(int settings_row,
                                   std::function<void()> on_click) {
  return [this, settings_row,
          on_click = std::move(on_click)](views::ui::InteractionEvent ie) {
    switch (ie.type) {
    case views::ui::InteractionEvent::Type::ButtonDown:
      pressed_settings_row_ = settings_row;
      mark_dirty();
      break;
    case views::ui::InteractionEvent::Type::Click:
      on_click();
      pressed_settings_row_.reset();
      mark_dirty();
      break;
    case views::ui::InteractionEvent::Type::Cancelled:
      pressed_settings_row_.reset();
      mark_dirty();
      break;
    default:
      break;
    }
  };
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
        settings_button_handler(settings_row, [val] { *val = !*val; }));
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
        settings_button_handler(settings_row, [val, next_reaction] {
          *val = next_reaction(*val);
        }));
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
        settings_button_handler(settings_row, [&settings] {
          int next =
              (static_cast<int>(settings.behavior_towards_staff) + 1) %
              static_cast<int>(magic_enum::enum_count<ManagerBehavior>());
          settings.behavior_towards_staff = static_cast<ManagerBehavior>(next);
        }));
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
  auto& staff_array = staff_array_for(*r, job_);

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

  // Time per pizza: operating minutes / total capacity (0 for waiters)
  int time_per_pizza = 0;
  if (is_chef && total_capacity > 0) {
    time_per_pizza = r->operating_minutes_per_day() / total_capacity;
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
                                    text_color, TextAnchor::End);
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

  // Render gray-ish background over the green one for selected slot while a
  // slot is selected (until its animation completes).
  if (selected_slot_index_.has_value() && card_slider_.on_screen()) {
    int selected_y = slot_start_y + (*selected_slot_index_ * slot_height);
    draw_gray_header(this_staff_slots, selected_y);
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
          TEXT_COLOR_BLACK, TextAnchor::End);
    }
  }
  else {
    const auto& staff_array = staff_array_for(*r, job_);

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
            TEXT_COLOR_BLACK, TextAnchor::End);

        context_.font->render_text_onto(
            this_staff_slots, compat::format("{}", staff->payment()),
            ViewPoint{pay_x, slot_y + text_y_offset}, FontFace::PIZ_NUMB,
            TEXT_COLOR_BLACK);
      }
    }
  }
  return this_staff_slots;
}

void StaffView::draw_card_name(Surface& card, std::string_view first,
                               std::string_view last) {
  constexpr int name_x = 160;
  context_.font->render_text_onto(card, first, ViewPoint{name_x, 34},
                                  FontFace::ASTANDAR, TEXT_COLOR_BLACK);
  context_.font->render_text_onto(card, last, ViewPoint{name_x, 56},
                                  FontFace::ASTANDAR, TEXT_COLOR_BLACK);
}

Surface StaffView::create_data_card(const Staff& staff) {
  const auto& character = staff.character();
  auto snapshot = character.to_character_data();
  // Use effective capacity (base + skill bonus) so card matches list CAP column
  snapshot.pizzas_per_day = staff.pizzas_per_day();

  return create_data_card_impl(character, snapshot, staff.payment(), "");
}

Surface StaffView::create_data_card(core::character::CharacterData* character,
                                    core::character::JobRole role) {
  return create_data_card_impl(*character, *character,
                               character->starting_payment(job_),
                               to_string(role));
}

Surface
StaffView::create_data_card_impl(const RawRenderable& portrait,
                                 const core::character::CharacterData& data,
                                 Money payment, std::string_view tab_label) {
  auto card = create_empty_card(tab_label);

  views::helpers::draw_framed_portrait(card, portrait, {8, 30});
  draw_card_name(card, data.first_name, data.last_name);

  // Age, payment, pizzas/day
  constexpr int name_x_offset = 160;
  constexpr int label_x_offset = name_x_offset - 8;

  context_.font->render_text_onto(card, std::to_string(data.age) + " YEARS",
                                  ViewPoint{label_x_offset, 90},
                                  FontFace::CSTANDAR, TEXT_COLOR_BLACK);

  context_.font->render_text_onto(card, "PAYMENT",
                                  ViewPoint{label_x_offset, 120},
                                  FontFace::CSTANDAR, TEXT_COLOR_BLACK);

  context_.font->render_text_onto(
      card, compat::format("{:i}", payment), ViewPoint{card.width() - 32, 120},
      FontFace::CSTANDAR, TEXT_COLOR_BLACK, TextAnchor::End);

  context_.font->render_text_onto(card, "PIZZAS/DAY",
                                  ViewPoint{label_x_offset, 138},
                                  FontFace::CSTANDAR, TEXT_COLOR_BLACK);

  context_.font->render_text_onto(card, std::to_string(data.pizzas_per_day),
                                  ViewPoint{card.width() - 32, 138},
                                  FontFace::CSTANDAR, TEXT_COLOR_BLACK,
                                  TextAnchor::End);

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

Staff* StaffView::selected_staff() {
  auto* restaurant = state_.active_restaurant();
  if (!restaurant || !selected_slot_index_.has_value()) {
    return nullptr;
  }

  if (job_ == core::character::JobRole::MANAGER) {
    auto& manager = restaurant->manager();
    return manager.has_value() ? &manager.value() : nullptr;
  }

  auto& staff_array = staff_array_for(*restaurant, job_);
  int slot = *selected_slot_index_;
  if (slot >= 0 && slot < static_cast<int>(staff_array.size()) &&
      staff_array[slot].has_value()) {
    return &staff_array[slot].value();
  }
  return nullptr;
}

Surface StaffView::create_training_card(const Staff& staff) {
  const auto& character = staff.character();
  auto card = create_empty_card("Courses");

  views::helpers::draw_framed_portrait(card, character, {8, 30});
  draw_card_name(card, character.first_name(), character.last_name());

  // Bar for the skill relevant to this job role.
  constexpr int name_x = 160;
  int score = relevant_skill_value(character, job_);
  StatBar statbar(context_.blitmap, context_.font);
  auto bar = statbar.create_stat_bar(skill_label(job_), score);
  card.blit_masked(bar, ViewPoint{name_x - 8, 96});

  // List of available courses. A gray button sits in front of every course
  // the staff member currently qualifies for (score >= required level); an
  // ineligible course shows the same row, just without the button.
  auto gray_button = blitmap3_.icon_surface(Blitmap3Icon::GRAY_BOX);
  auto courses = TrainingFactory::trainings_by_skill(skill_for_job(job_));

  // Column-header row.
  context_.font->render_line_onto(card, "course name    scores  costs days",
                                  ViewPoint{TRAINING_TEXT_X, TRAINING_LIST_Y},
                                  FontFace::PIZ_NUMB, TEXT_COLOR_BLACK);

  int y = TRAINING_COURSE_Y0;
  for (const auto& course : courses) {
    if (score >= course.min_level()) {
      card.blit_raw(gray_button, ViewPoint{TRAINING_BTN_X, y});
    }
    context_.font->render_text_onto(card, course.name(),
                                    ViewPoint{TRAINING_TEXT_X, y},
                                    FontFace::PIZ_NUMB, TEXT_COLOR_BLACK);
    context_.font->render_text_onto(card, std::to_string(course.min_level()),
                                    ViewPoint{TRAINING_SCORE_X, y},
                                    FontFace::PIZ_NUMB, TEXT_COLOR_BLACK,
                                    TextAnchor::End);
    context_.font->render_text_onto(card, compat::format("{}", course.cost()),
                                    ViewPoint{TRAINING_COST_X, y},
                                    FontFace::PIZ_NUMB, TEXT_COLOR_BLACK,
                                    TextAnchor::End);
    context_.font->render_text_onto(
        card, std::to_string(course.duration()), ViewPoint{TRAINING_DAYS_X, y},
        FontFace::PIZ_NUMB, TEXT_COLOR_BLACK, TextAnchor::End);
    y += TRAINING_ROW_H;
  }

  return card;
}

void StaffView::show_training_card() {
  Staff* staff = selected_staff();
  if (!staff) {
    return;
  }

  card_slider_.push(create_training_card(*staff));
  change_screen(StaffScreen::TRAINING);
  mark_dirty();
}

void StaffView::on_training_clicked(const Training& course) {
  Staff* staff = selected_staff();
  if (!staff) {
    return;
  }

  context_.popup->origin(ViewPoint{30, 40});
  context_.popup->create<ConfirmPopup>(
      "Send on this course?",
      [this, course](bool yes) {
        if (yes) {
          if (Staff* s = selected_staff()) {
            s->send_on_training(course);
          }
          // Card slides away and we drop back to the staff overview.
          card_slider_.dismiss();
          change_screen(StaffScreen::OVERVIEW);
        }
        mark_dirty();
      },
      &staff->character());
}

void StaffView::draw_training_interactions() {
  Staff* staff = selected_staff();
  if (!staff) {
    return;
  }

  int score = relevant_skill_value(staff->character(), job_);
  auto gray_button = blitmap3_.icon_surface(Blitmap3Icon::GRAY_BOX);
  auto courses = TrainingFactory::trainings_by_skill(skill_for_job(job_));

  int row = 0;
  int y = TRAINING_COURSE_Y0;
  for (const auto& course : courses) {
    if (score >= course.min_level()) {
      ViewRect btn_rect{CARD_OFFSET_X + TRAINING_BTN_X, CARD_OFFSET_Y + y,
                        gray_button.width(), gray_button.height()};
      Training course_copy = course;
      interactions_.create_frame_item(
          btn_rect, "training_" + course.name(),
          [this, course_copy, row](views::ui::InteractionEvent ie) {
            if (ie.type == views::ui::InteractionEvent::Type::ButtonDown) {
              pressed_training_row_ = row;
              mark_dirty();
            }
            else if (ie.type == views::ui::InteractionEvent::Type::Click &&
                     ie.button == views::ui::MouseButton::Left) {
              pressed_training_row_.reset();
              on_training_clicked(course_copy);
            }
            else if (ie.type == views::ui::InteractionEvent::Type::Cancelled) {
              pressed_training_row_.reset();
              mark_dirty();
            }
          });
    }
    y += TRAINING_ROW_H;
    ++row;
  }
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
  auto card = create_empty_card(to_string(job_));

  views::helpers::draw_framed_portrait(card, *char_data, {8, 30});
  draw_card_name(card, char_data->first_name, char_data->last_name);

  // Render "Job Application" label (where age would be on data card)
  constexpr int name_x = 160;
  constexpr int label_x = name_x - 8;
  context_.font->render_text_onto(card, "JOB APPLICATION",
                                  ViewPoint{label_x, 90}, FontFace::CSTANDAR,
                                  TEXT_COLOR_BLACK);

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

  FlowColumns flow{
      .cutoff_y = picture_y + picture_height + 10, // 10px below picture
      .left_margin_above = text_start_x,           // Next to picture
      .left_margin_below = text_left_margin,       // Full width below picture
      .right_margin = text_right_margin};

  // bewerb*.e marks split points with a dash between two letters.
  constexpr TextStyle application_style{.color = gfx::ColorIndex{5},
                                        .soft_hyphens = true};
  context_.font->render_text_onto(card, full_text,
                                  ViewPoint{text_start_x, text_start_y},
                                  FontFace::CSTANDAR, application_style, flow);

  return card;
}

Surface StaffView::create_empty_card(const std::string_view label) {
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
  if (screen_ == StaffScreen::OVERVIEW && card_slider_.settled() &&
      selected_slot_index_.has_value()) {
    card_slider_.dismiss();
    // Selection is cleared by the slider's on_hidden callback.
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

      // Use Staff overload to show payment
      card_slider_.present(create_data_card(*manager_opt));
      // Stay on OVERVIEW so buttons remain visible
    }
    return;
  }

  // Get the appropriate staff list based on job
  const auto& staff_list = staff_array_for(*restaurant, job);

  // Check if slot is valid and has a staff member
  if (slot_index >= 0 && slot_index < static_cast<int>(staff_list.size())) {
    const auto& staff_opt = staff_list[slot_index];
    if (staff_opt.has_value()) {
      // Store the selected slot
      selected_slot_index_ = slot_index;

      // Create and show the character card using Staff overload
      card_slider_.present(create_data_card(*staff_opt));

      // Stay on OVERVIEW so fire/wages/training buttons remain visible
    }
  }
}

void StaffView::change_screen(StaffScreen screen) {
  Logger::debug("Change screen to {}", magic_enum::enum_name(screen));
  screen_ = screen;
  input_panel_needs_update_ = true;
  mark_dirty();
}

void StaffView::change_job(core::character::JobRole job) {
  Logger::info("Change job at state {}", magic_enum::enum_name(screen_));

  const bool on_card_screen =
      screen_ == StaffScreen::OVERVIEW || screen_ == StaffScreen::TRAINING;
  if (on_card_screen && selected_slot_index_.has_value()) {
    card_slider_.dismiss();
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
                               if (screen_ == StaffScreen::APPLICANT_CARD ||
                                   screen_ == StaffScreen::APPLICATIONS) {
                                 state_.advance_time_blocks(
                                     applicant_exit_time_blocks);
                               }
                               card_slider_.dismiss();
                               change_screen(StaffScreen::OVERVIEW);
                             }
                           });
}

void StaffView::apply_wage_change(Staff& staff, Money new_wage) {
  const Money old_wage = staff.payment();
  if (old_wage.whole() <= 0) {
    return;
  }
  // The original compares whole dollars.
  int wage_pct = static_cast<int>((new_wage.whole() * 100) / old_wage.whole());

  // raise_motivation_factor scales the raise (increase) response by difficulty
  int difficulty_divisor = (state_.difficulty_level() / 2) + 1;
  int raise_motivation_factor =
      (((wage_pct - 100) * 2) / difficulty_divisor) + 100;

  int old_motivation = staff.character().motivation();
  // No raise halves motivation; a cut drops it further.
  int motivation_factor =
      wage_pct > 100 ? raise_motivation_factor : (wage_pct / 2);
  int motivation_change =
      ((old_motivation * motivation_factor) / 100) - old_motivation;

  staff.payment(new_wage);
  staff.adjust_motivation(motivation_change); // clamps to 0-100

  // Only a raise changes throughput; cuts and no-change leave ppd alone.
  if (wage_pct > 100) {
    int old_ppd = staff.character().pizzas_per_day();
    int multiplier = raise_motivation_factor > 100
                         ? (raise_motivation_factor + 120) / 2
                         : raise_motivation_factor;
    int new_ppd = std::min((old_ppd * multiplier) / 100, 2000);
    staff.pizzas_per_day(new_ppd);
  }

  Logger::info("[Staff] Wage {:w} -> {:w} ({}%), motivation: {:+d}, ppd {}",
               old_wage, new_wage, wage_pct, motivation_change,
               wage_pct > 100 ? "updated" : "unchanged");
}

void StaffView::add_overview_buttons() {
  // Fire
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

        const Staff* staff_ptr = selected_staff();
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

        context_.popup->origin(ViewPoint{30, 40});
        context_.popup->create<ConfirmPopup>(
            "Do you really want to fire me, boss?",
            [this, slot_index, character_id](bool sure) {
              if (sure) {
                auto* restaurant = state_.active_restaurant();
                auto* city = state_.active_city();
                bool fired = restaurant->fire_staff(job_, slot_index);
                if (fired && character_id != 0) {
                  Logger::info("Fired staff member with ID {}", character_id);
                  city->blacklist_staff(character_id,
                                        state_.active_player()->id());
                  // Restore market slot so the character can be re-hired
                  // elsewhere
                  city->job_market().set_status(
                      character_id,
                      static_cast<core::character::MarketStatus>(job_));
                  selected_slot_index_.reset();
                  card_slider_.dismiss();
                }
                else {
                  Logger::warn("Failed to fire staff member");
                }
              }
              mark_dirty();
            },
            &staff_ptr->character());
      });

  // Wages
  input_panel_->add_button(
      Icon::MONEY_BAG, ButtonType::Square, ButtonPosition::MiddleCenter,
      "WAGES", [this]() {
        Staff* staff = selected_staff();
        if (!staff) {
          context_.popup->origin(ViewPoint{52, 80});
          context_.popup->create<AlertPopup>(
              "To change somebody's wages, you must first select that person!");
          return;
        }

        int current_wage = static_cast<int>(staff->payment().whole());
        int min_wage = current_wage / 2;
        int max_wage = current_wage * 2;
        constexpr int step = 50; // $50 increments

        context_.popup->origin(ViewPoint{52, 80});
        context_.popup->create<NumberInputPopup>(
            "Change payment", "$", current_wage, min_wage, max_wage, step,
            [this](int new_wage_dollars) {
              if (Staff* s = selected_staff()) {
                apply_wage_change(*s, Money::from_dollars(new_wage_dollars));
                card_slider_.dismiss();
                mark_dirty();
              }
            });
      });

  // Training
  input_panel_->add_button(
      Icon::FORM, ButtonType::Square, ButtonPosition::MiddleRight, "TRAINING",
      [this]() {
        if (screen_ == StaffScreen::TRAINING) {
          return;
        }
        if (!selected_slot_index_.has_value()) {
          context_.popup->origin(ViewPoint{52, 80});
          context_.popup->create<AlertPopup>(
              "To send somebody on a course, you must "
              "first select that person!");
          return;
        }
        Staff* staff = selected_staff();
        if (staff && staff->on_course()) {
          const auto& ch = staff->character();
          context_.popup->origin(ViewPoint{52, 80});
          context_.popup->create<AlertPopup>(compat::format(
              "{} {} already is on the '{}' course.", ch.first_name(),
              ch.last_name(), staff->current_course()->name()));
          return;
        }
        show_training_card();
      });

  // Recruit
  input_panel_->add_button(Icon::SHOPPING_CART, ButtonType::Horizontal,
                           ButtonPosition::BottomLeft, "RECRUIT",
                           [this]() { show_applicant_card(job_); });
}

void StaffView::add_applicant_buttons() {
  // Up arrow: move to next candidate (push - new card slides down from top)
  input_panel_->add_button(Icon::ARROW_UP, ButtonType::Square,
                           ButtonPosition::MiddleLeft, "TURN PAGE",
                           [this]() { handle_next_applicant(); });

  input_panel_->add_button(
      Icon::BAR_CHART, ButtonType::Square, ButtonPosition::MiddleCenter, "DATA",
      [this]() {
        // Switch to data card (character stats)
        if (screen_ == StaffScreen::APPLICATIONS) {
          auto& vec = applicant_pool_[job_];
          if (!vec.empty() && applicant_cursor_[job_] < vec.size()) {
            auto* char_data = vec[applicant_cursor_[job_]];
            card_slider_.set_current(create_data_card(char_data, job_));
            change_screen(StaffScreen::APPLICANT_CARD);
          }
        }
      });

  input_panel_->add_button(
      Icon::FORM, ButtonType::Square, ButtonPosition::MiddleRight,
      "APPLICATIONS", [this]() {
        // Switch to application text card
        if (screen_ == StaffScreen::APPLICANT_CARD) {
          auto& vec = applicant_pool_[job_];
          if (!vec.empty() && applicant_cursor_[job_] < vec.size()) {
            auto* char_data = vec[applicant_cursor_[job_]];
            card_slider_.set_current(create_application_card(char_data->id));
            change_screen(StaffScreen::APPLICATIONS);
          }
        }
      });

  // Down arrow: move to previous candidate (pop - current slides up revealing
  // previous)
  input_panel_->add_button(Icon::ARROW_DOWN, ButtonType::Square,
                           ButtonPosition::BottomLeft, "TURN PAGE",
                           [this]() { handle_previous_applicant(); });

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

                if (city->is_blacklisted(char_data->id,
                                         state_.active_player()->id())) {
                  context_.popup->origin(ViewPoint{52, 80});
                  context_.popup->create<AlertPopup>(
                      "Sorry! I don't want to work for YOU again!");
                  return;
                }

                core::character::Character new_character(char_data);

                Staff new_staff(job_, new_character);
                new_staff.payment(char_data->starting_payment(job_));

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
                  state_.advance_time_blocks(hire_time_blocks);

                  // Mark character as hired in the city job market
                  city->job_market().set_status(
                      char_data->id, core::character::hired_status(
                                         state_.active_player()->id().value));

                  // Remove hired candidate from applicant pool
                  auto& vec = applicant_pool_[job_];
                  vec.erase(vec.begin() + applicant_cursor_[job_]);

                  // Adjust cursor if needed
                  if (applicant_cursor_[job_] >= vec.size() && !vec.empty()) {
                    applicant_cursor_[job_] = vec.size() - 1;
                  }

                  // Clear cards and go directly to overview (no animation)
                  card_slider_.clear();
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
  Logger::debug("Creating input panel for {}", magic_enum::enum_name(screen_));
  input_panel_->delete_buttons();

  add_common_buttons();

  // Add screen-specific buttons
  switch (screen_) {
  case StaffScreen::OVERVIEW:
  case StaffScreen::TRAINING:
    add_overview_buttons();
    break;
  case StaffScreen::APPLICANT_CARD:
  case StaffScreen::APPLICATIONS:
    add_applicant_buttons();
    break;
  }
}

void StaffView::load_applicant_pool() {
  applicant_pool_.clear();

  std::set<uint8_t> player_char_ids;
  for (const auto* player : state_.players()) {
    if (player) {
      player_char_ids.insert(player->character().id());
    }
  }

  auto* city = state_.active_city();
  if (!city) {
    return;
  }
  const auto& market = city->job_market();

  for (int id = 0; id < market.total_slots(); ++id) {
    auto s = market.status(id);
    if (!core::character::is_available(s)) {
      continue;
    }
    auto role = core::character::job_role(s);
    auto* cd = CharacterFactory::create_character(id, role);
    if (player_char_ids.contains(cd->id)) {
      continue;
    }
    applicant_pool_[role].push_back(cd);
  }
}

void StaffView::show_applicant_card(core::character::JobRole role) {
  const auto& vec = applicant_pool_[role];
  if (vec.empty()) {
    return;
  }

  std::size_t first = 0;
  Surface new_card = make_card(role, first);
  applicant_cursor_[role] = first;

  if (card_slider_.on_screen()) {
    card_slider_.push(std::move(new_card));
  }
  else {
    card_slider_.present(std::move(new_card));
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

void StaffView::handle_next_applicant() {
  if (card_slider_.animating()) {
    return;
  }
  auto& vec = applicant_pool_[job_];
  if (vec.empty()) {
    return;
  }
  applicant_cursor_[job_] = (applicant_cursor_[job_] + 1) % vec.size();
  Logger::debug("Moving to next candidate, index: {}", applicant_cursor_[job_]);

  card_slider_.push(make_card(job_, applicant_cursor_[job_]));
  if (screen_ == StaffScreen::APPLICATIONS) {
    change_screen(StaffScreen::APPLICANT_CARD);
  }
  mark_dirty();
}

void StaffView::handle_previous_applicant() {
  if (card_slider_.animating()) {
    return;
  }
  auto& vec = applicant_pool_[job_];
  if (vec.empty()) {
    return;
  }
  std::size_t prev = (applicant_cursor_[job_] == 0)
                         ? vec.size() - 1
                         : applicant_cursor_[job_] - 1;
  Logger::debug("Moving to previous candidate, index: {}", prev);
  applicant_cursor_[job_] = prev;

  card_slider_.pop(make_card(job_, prev));
  if (screen_ == StaffScreen::APPLICATIONS) {
    change_screen(StaffScreen::APPLICANT_CARD);
  }
  mark_dirty();
}
