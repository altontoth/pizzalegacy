/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// The interactive design canvas: topping placement, the chopper, and the
// ingredient dropdowns.

#include "core/state/GameState.h"
#include "core/util/LegacyConstants.h"
#include "engine/Logger.h"
#include "engine/audio/AudioManager.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/ICursorManager.h"
#include "gfx/Blit.h"
#include "views/base/ViewDefs.h"
#include "views/screens/pizza/PizzaDesign.h"
#include "views/screens/pizza/PizzaDesignHelpers.h"

#include <array>
#include <memory>
#include <string_view>

static constexpr int CHOP_FLIP_PERIOD_MS = 55;
static constexpr int NUMBER_OF_CHOP_MS = 600;

static constexpr int PIZZA_CENTER_X =
    234 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
static constexpr int PIZZA_CENTER_Y =
    232 - LegacyConstants::WOOD_BORDER_HEIGHT_TOP;
static constexpr int PIZZA_RADIUS = 122;

using namespace core::food;
using namespace pizza_design;

namespace {
struct IngredientInfo {
  PizzaDesign::IngredientDropdown category;
  IngredientType id;
  std::string_view name;
};
using enum IngredientType;
using enum PizzaDesign::IngredientDropdown;
constexpr IngredientInfo ingredients[] = {
    {.category = VEGETABLES, .id = TOMATOES, .name = "TOMATOES"},
    {.category = VEGETABLES, .id = OLIVES, .name = "OLIVES"},
    {.category = VEGETABLES, .id = PEPPERS, .name = "PEPPERS"},
    {.category = VEGETABLES, .id = ASPARAGUS, .name = "ASPARAGUS"},
    {.category = VEGETABLES, .id = COURGETTES, .name = "COURGETTES"},
    {.category = VEGETABLES, .id = CARROTS, .name = "CARROTS"},
    {.category = VEGETABLES, .id = CUCUMBERS, .name = "CUCUMBERS"},
    {.category = VEGETABLES, .id = RADISHES, .name = "RADISHES"},
    {.category = VEGETABLES, .id = PEAS, .name = "PEAS"},
    {.category = VEGETABLES, .id = BROCCOLI, .name = "BROCCOLI"},
    {.category = VEGETABLES, .id = POTATOES, .name = "POTATOES"},
    {.category = VEGETABLES, .id = CHILLIES, .name = "CHILLIES"},
    {.category = VEGETABLES, .id = SPINACH, .name = "SPINACH"},
    {.category = VEGETABLES, .id = SALAD, .name = "SALAD"},
    {.category = VEGETABLES, .id = LEEKS, .name = "LEEKS"},
    {.category = VEGETABLES, .id = ONIONS, .name = "ONIONS"},
    {.category = VEGETABLES, .id = GARLIC, .name = "GARLIC"},
    {.category = VEGETABLES, .id = SWEET_CORN, .name = "SWEET CORN"},
    {.category = VEGETABLES, .id = CAULIFLOWER, .name = "CAULIFLOWER"},
    {.category = VEGETABLES, .id = MUSHROOMS, .name = "MUSHROOMS"},
    {.category = VEGETABLES, .id = CHANTERELLES, .name = "CHANTERELLES"},
    {.category = VEGETABLES, .id = W_MUSHROOMS, .name = "W. MUSHROOMS"},
    {.category = MEAT, .id = SALAMI, .name = "SALAMI"},
    {.category = MEAT, .id = BACON, .name = "BACON"},
    {.category = MEAT, .id = HAM, .name = "HAM"},
    {.category = MEAT, .id = BEEF, .name = "BEEF"},
    {.category = MEAT, .id = PORK, .name = "PORK"},
    {.category = MEAT, .id = CHICKEN, .name = "CHICKEN"},
    {.category = MEAT, .id = MINCEMEAT, .name = "MINCEMEAT"},
    {.category = MEAT, .id = SAUSAGE, .name = "SAUSAGE"},
    {.category = MEAT, .id = SCAMPI, .name = "SCAMPI"},
    {.category = MEAT, .id = SHRIMPS, .name = "SHRIMPS"},
    {.category = MEAT, .id = SQUID, .name = "SQUID"},
    {.category = MEAT, .id = ANCHOVY, .name = "ANCHOVY"},
    {.category = MEAT, .id = SALMON, .name = "SALMON"},
    {.category = MEAT, .id = LOBSTER, .name = "LOBSTER"},
    {.category = MEAT, .id = CAVIAR, .name = "CAVIAR"},
    {.category = MEAT, .id = COCKLES, .name = "COCKLES"},
    {.category = MEAT, .id = MUSSELS, .name = "MUSSELS"},
    {.category = FRUIT, .id = APPLES, .name = "APPLES"},
    {.category = FRUIT, .id = PEARS, .name = "PEARS"},
    {.category = FRUIT, .id = MAN_ORANGES, .name = "MAN ORANGES"},
    {.category = FRUIT, .id = PLUMS, .name = "PLUMS"},
    {.category = FRUIT, .id = CHERRIES, .name = "CHERRIES"},
    {.category = FRUIT, .id = BANANAS, .name = "BANANAS"},
    {.category = FRUIT, .id = LEMONS, .name = "LEMONS"},
    {.category = FRUIT, .id = PINEAPPLE, .name = "PINEAPPLE"},
    {.category = FRUIT, .id = KIWI, .name = "KIWI"},
    {.category = FRUIT, .id = STRAWBERRIES, .name = "STRAWBERRIES"},
    {.category = FRUIT, .id = RASPBERRIES, .name = "RASPBERRIES"},
    {.category = FRUIT, .id = BLACKBERRIES, .name = "BLACKBERRIES"},
    {.category = FRUIT, .id = GOOSEBERRIES, .name = "GOOSEBERRIES"},
    {.category = CHEESE, .id = EGG, .name = "EGG"},
    {.category = CHEESE, .id = MOZZARELLA, .name = "MOZZARELLA"},
    {.category = CHEESE, .id = PROVOLONE, .name = "PROVOLONE"},
    {.category = CHEESE, .id = BEL_PAESE, .name = "BEL PAESE"},
    {.category = CHEESE, .id = GORGONZOLA, .name = "GORGONZOLA"},
    {.category = CHEESE, .id = PAMERINO, .name = "PAMERINO"},
    {.category = CHEESE, .id = PARMESAN, .name = "PARMESAN"},
    {.category = CHEESE, .id = GOATS_CHEESE, .name = "GOATS CHEESE"},
    {.category = CHEESE, .id = PARSLEY, .name = "PARSLEY"},
    {.category = CHEESE, .id = LEMON_BALM, .name = "LEMON BALM"},
    {.category = CHEESE, .id = SAGE, .name = "SAGE"},
    {.category = CHEESE, .id = GINGER, .name = "GINGER"},
    {.category = CHEESE, .id = OREGANO, .name = "OREGANO"},
};
} // namespace

void PizzaDesign::start_chop() {
  if (!active_topping_ || chopping_ || !active_topping_->chop()) {
    return;
  }

  chopping_ = true;
  chop_horizontal_ = true;
  chop_start_ms_ = context_.clock.now_ms;
  chop_last_phase_ = 0;
  context_.cursor->hide_cursor();
  context_.audio->play_sound(audio::SoundId::CHOPPER);

  mark_dirty();
}

void PizzaDesign::tick_chop() {
  if (!chopping_) {
    return;
  }

  // End condition, total duration reached (only when horizontal)
  if (context_.clock.elapsed_since(chop_start_ms_, NUMBER_OF_CHOP_MS) &&
      chop_horizontal_) {
    chopping_ = false;
    if (active_topping_) {
      context_.cursor->custom_cursor(renderer_.image(*active_topping_));
      context_.cursor->show_cursor();
      update_status_ingredient();
    }
    mark_dirty();
    return;
  }

  // Flip orientation
  if (context_.clock.every_n_ms(CHOP_FLIP_PERIOD_MS, &chop_last_phase_)) {
    chop_horizontal_ = !chop_horizontal_;
    mark_dirty();
  }
}

void PizzaDesign::setup_pizza_design_interactions(const RenderContext& ctx) {
  views::ui::InteractionContext ictx{interactions_, [this] { mark_dirty(); }};
  interactions_.create_frame_item(
      ViewRect{0, 0, views::base::MAIN_PANEL_WIDTH,
               views::base::MAIN_PANEL_HEIGHT},
      "pizza fallback", [this](views::ui::InteractionEvent ui) {
        using Type = views::ui::InteractionEvent::Type;
        if (ui.type == Type::Enter) {
          if (active_topping_) {
            context_.cursor->custom_cursor(renderer_.image(*active_topping_));
          }
          return;
        }
        if (ui.type == Type::Leave) {
          if (active_topping_) {
            // Leave also fires when the cursor moves onto the chopper or the
            // dropdown items, so we only clear when actually leaving the panel.
            constexpr ViewRect panel{0, 0, views::base::MAIN_PANEL_WIDTH,
                                     views::base::MAIN_PANEL_HEIGHT};
            if (!panel.contains(ui.local)) {
              context_.cursor->clear_custom_cursor();
            }
          }
          return;
        }
        if (ui.type != Type::Click) {
          return;
        }
        if (ui.button == views::ui::MouseButton::Left) {
          handle_left_mouse_click(ui.local);
        }
        else if (ui.button == views::ui::MouseButton::Right) {
          handle_right_mouse_click(ui.local);
        }
      });
  // Chopper (registered before dropdowns so dropdowns take priority)
  interactions_.create_frame_item(
      ViewRect{5, 250, CHOPPER_WIDTH, CHOPPER_HEIGHT}, "chopper",
      [this](views::ui::InteractionEvent ie) {
        if (ie.type == views::ui::InteractionEvent::Type::Click &&
            ie.button == views::ui::MouseButton::Left) {
          // If no active topping, try to reactivate previous topping
          if (!active_topping_ && previous_topping_) {
            active_topping_ = std::move(previous_topping_);
            previous_topping_.reset();
            context_.cursor->custom_cursor(renderer_.image(*active_topping_));
            update_status_ingredient();
            mark_dirty();
          }
          start_chop();
        }
      });

  dropdowns_.add_interactions(ictx);
  dropdowns_.render(*ctx.surface, context_.font);
}

void PizzaDesign::draw_pizza(const RenderContext& ctx) const {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back1"));
  ctx.surface->blit_masked(chop_horizontal_ ? blade_horizontal_
                                            : blade_vertical_,
                           ViewPoint{CHOPPER_OFFSET_X, CHOPPER_OFFSET_Y});

  const Pizza* pizza = current_pizza();
  if (!pizza) {
    // This can happen during end of competition, not an issue
    Logger::debug("No current pizza!");
    return;
  }

  for (const auto& topping : pizza->toppings()) {
    renderer_.render(*topping, *ctx.surface, camera_);
  }
}

bool PizzaDesign::handle_left_mouse_click(ViewPoint vp) {
  if (mode_ != PizzaDesignMode::Design &&
      mode_ != PizzaDesignMode::CompetitionDesign &&
      mode_ != PizzaDesignMode::AnnualCompetitionDesign) {
    return false;
  }
  Pizza* pizza = current_pizza();

  const auto p = camera_.view_to_world(vp);

  if (active_topping_ && is_inside_pizza(vp)) {
    auto& surface = renderer_.image(*active_topping_);
    context_.cursor->clear_custom_cursor();

    // Store CENTER in pizza-space:
    active_topping_->place(p);

    previous_topping_ = std::make_unique<Topping>(*active_topping_);
    pizza->add_topping(std::move(active_topping_));
    active_topping_ = nullptr;
    pizza->update_stats(state_.customer_taste_profiles());
    status_.show_pizza(PizzaCtx{
        .state = &state_,
        .wip_pizza = pizza,
        .recipe = nullptr,
    });
    mark_dirty();
  }
  else if (!active_topping_ && previous_topping_) {
    active_topping_ = std::move(previous_topping_);
    previous_topping_.reset();
    auto& surface = renderer_.image(*active_topping_);
    context_.cursor->custom_cursor(surface);
    update_status_ingredient();
    mark_dirty();
  }
  return false;
}

bool PizzaDesign::handle_right_mouse_click(ViewPoint /* p */) {
  if (mode_ != PizzaDesignMode::Design &&
      mode_ != PizzaDesignMode::CompetitionDesign &&
      mode_ != PizzaDesignMode::AnnualCompetitionDesign) {
    return false;
  }
  Pizza* pizza = current_pizza();
  if (active_topping_ != nullptr) {
    active_topping_->rotate();
    context_.cursor->custom_cursor(renderer_.image(*active_topping_));
    update_status_ingredient();
    mark_dirty();
  }
  else {
    previous_topping_ = pizza->remove_latest_topping();
    pizza->update_stats(state_.customer_taste_profiles());
    status_.show_pizza(PizzaCtx{
        .state = &state_,
        .wip_pizza = &bake_->working,
        .recipe = nullptr,
    });
    mark_dirty();
  }
  return false;
}

bool PizzaDesign::is_inside_pizza(ViewPoint p) {
  // Check based on the radius of the pizza:
  return ((PIZZA_CENTER_X - p.x) * (PIZZA_CENTER_X - p.x) +
          (PIZZA_CENTER_Y - p.y) * (PIZZA_CENTER_Y - p.y)) <=
         PIZZA_RADIUS * PIZZA_RADIUS;
}

void PizzaDesign::create_dropdowns() {
  constexpr int dd_btn_width = 60;
  constexpr int dd_btn_height = 60;
  constexpr int dd_btn_offset_x = 40;
  constexpr int dd_btn_offset_y = 6;
  constexpr int dd_btn_spacing_x = 80;

  constexpr IngredientDropdown DROPDOWN_FIRST = IngredientDropdown::VEGETABLES;
  constexpr IngredientDropdown DROPDOWN_LAST = IngredientDropdown::CHEESE;

  static constexpr std::array<ViewPoint, 4> origins_{
      ViewPoint{8, 12},
      ViewPoint{56, 12},
      ViewPoint{128, 12},
      ViewPoint{184, 12},
  };

  const int base = static_cast<int>(DROPDOWN_FIRST);
  for (int i = base; i <= static_cast<int>(DROPDOWN_LAST); ++i) {
    const int idx = i - base;
    auto dd = static_cast<IngredientDropdown>(idx);
    ViewRect location{dd_btn_offset_x + (dd_btn_spacing_x * idx),
                      dd_btn_offset_y, dd_btn_width, dd_btn_height};
    dropdowns_.add_dropdown(dd, location, origins_[idx], [this] {
      // If they click the dropdown we want to revert to the regular cursor
      // again
      context_.cursor->clear_custom_cursor();
      active_topping_.reset();
    });
  }

  // Little helper lambda for adding items
  auto add_ingredient_item = [this](IngredientDropdown dd,
                                    IngredientType ingredient,
                                    const std::string_view name) {
    dropdowns_.add_item(dd, name, [this, ingredient] {
      if (active_topping_) {
        active_topping_.reset();
      }
      active_topping_ = std::make_unique<Topping>(ingredient);
      update_status_ingredient();
      context_.cursor->custom_cursor(renderer_.image(*active_topping_));
    });
  };
  for (const auto& ing : ingredients) {
    add_ingredient_item(ing.category, ing.id, ing.name);
  }
}
