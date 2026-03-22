/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/PizzaManager.h"
#include "core/Demographics.h"
#include "core/GameTypes.h"
#include "core/city/Property.h"
#include "core/finance/MoneyParse.h"
#include "core/food/RecipeCodec.h"
#include "core/state/GameState.h"
#include "core/util/LegacyConstants.h"
#include "engine/Random.h"
#include "engine/audio/AudioManager.h"
#include "engine/geom/Spaces.h"
#include "engine/input/InputManager.h"
#include "engine/ui/ICursorManager.h"
#include "factories/PizzaFactory.h"
#include "gfx/Blit.h"
#include "gfx/Draw.h"
#include "gfx/PaletteBank.h"
#include "resources/BobSizeFn.h"
#include "resources/util/Platform.h"
#include "views/base/ViewDefs.h"
#include "views/screens/restaurant/RecipeListRow.h"
#include "views/ui/PopupManager.h"
#include "views/ui/components/BeveledButton.h"
#include "views/ui/popups/AlertPopup.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/popups/NumberInputPopup.h"
#include "views/ui/popups/TextInputPopup.h"
#include "views/ui/widgets/StatBar.h"

#include <cassert>
#include <memory>
#include <optional>

static constexpr int CHOP_FLIP_PERIOD_MS = 55;
static constexpr int NUMBER_OF_CHOP_MS = 600;
static constexpr int CHOPPER_HEIGHT = 86;
static constexpr int CHOPPER_WIDTH = 86;
static constexpr int CHOPPER_OFFSET_X = 8;
static constexpr int CHOPPER_OFFSET_Y = 256;

static constexpr int PIZZA_CENTER_X =
    234 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
static constexpr int PIZZA_CENTER_Y =
    232 - LegacyConstants::WOOD_BORDER_HEIGHT_TOP;
static constexpr int PIZZA_RADIUS = 122;

using namespace core::food;
using core::finance::parse::from_input;
using io::recipe::save_prez;

namespace {
inline bool is_selected(const RecipeListRow& row,
                        const std::optional<RecipeKey>& active) {
  if (!active) {
    return false;
  }
  if (row.recipe_id && std::holds_alternative<core::RecipeId>(*active)) {
    return *row.recipe_id == std::get<core::RecipeId>(*active);
  }
  if (row.canonical &&
      std::holds_alternative<core::food::CanonicalPizza>(*active)) {
    return *row.canonical == std::get<core::food::CanonicalPizza>(*active);
  }
  return !row.recipe_id && !row.canonical &&
         std::holds_alternative<NewRowTag>(*active);
}

inline const RecipeListRow*
find_row_by_key(const std::vector<RecipeListRow>& rows, const RecipeKey& key) {
  for (const auto& r : rows) {
    if (std::holds_alternative<core::RecipeId>(key) && r.recipe_id &&
        *r.recipe_id == std::get<core::RecipeId>(key)) {
      return &r;
    }
    if (std::holds_alternative<core::food::CanonicalPizza>(key) &&
        r.canonical &&
        *r.canonical == std::get<core::food::CanonicalPizza>(key)) {
      return &r;
    }
    if (std::holds_alternative<NewRowTag>(key) && !r.recipe_id &&
        !r.canonical) {
      return &r; // "<NEW>"
    }
  }
  return nullptr;
}

// build an override that copies 0xC0..0xDF from src palette (e.g. 13)
gfx::PaletteOverride make_bar_ui_ramp_override(uint8_t src_pal_id) {
  std::vector<uint8_t> r = {101, 103, 104, 108, 109, 111};
  return gfx::PaletteOverride{
      .dst_indices = r, .src_indices = r, .src_palette_id = src_pal_id};
}
struct IngredientInfo {
  PizzaManager::IngredientDropdown category;
  IngredientType id;
  std::string_view name;
};
using enum IngredientType;
using enum PizzaManager::IngredientDropdown;
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
    {.category = FRUIT, .id = PEARS, .name = "PEARS"},
    {.category = FRUIT, .id = MAN_ORANGES, .name = "MAN ORANGES"},
    {.category = FRUIT, .id = PLUMS, .name = "PLUMS"},
    {.category = FRUIT, .id = CHERRIES, .name = "CHERRIES"},
    {.category = FRUIT, .id = BANANAS, .name = "BANANAS"},
    {.category = FRUIT, .id = LEMONS, .name = "LEMONS"},
    {.category = FRUIT, .id = PINEAPPLE, .name = "PINEAPPLE"},
    {.category = FRUIT, .id = KIWI, .name = "KIWI"},
    {.category = FRUIT, .id = STRAWBERRIES, .name = "STRAWBERRIES"},
    {.category = FRUIT, .id = BLACKBERRIES, .name = "BLACKBERRIES"},
    {.category = FRUIT, .id = GOOSEBERRIES, .name = "GOOSEBERRIES"},
    {.category = FRUIT, .id = MOZZARELLA, .name = "MOZZARELLA"},
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

PizzaManager::PizzaManager(GameContext& context, GameState& state,
                           PizzaManagerState pms)
    : MainPanelView(context, state), mode_(pms.initial_mode),
      design_mode_{pms.initial_mode == PizzaManagerMode::Design},
      zutaten_bob_(context_.bob->get("bob/zutaten.bob")),
      blade_horizontal_{[this] {
        ViewRect src_rect{CHOPPER_OFFSET_X, CHOPPER_OFFSET_Y, CHOPPER_WIDTH,
                          CHOPPER_HEIGHT};

        Surface dst{CHOPPER_WIDTH, CHOPPER_HEIGHT};

        // copy the background window into dst
        dst.blit(context_.vga->get("gfx/back1"), src_rect);

        // overlay the blade sprite
        Surface blade = zutaten_bob_.frame(193, PaletteHandle{6});
        dst.blit_masked(blade, ViewPoint{30, 8});

        return dst; // moves into blade_horizontal_
      }()},
      blade_vertical_{[this] {
        ViewRect src_rect{CHOPPER_OFFSET_X, CHOPPER_OFFSET_Y, CHOPPER_WIDTH,
                          CHOPPER_HEIGHT};

        Surface dst{CHOPPER_WIDTH, CHOPPER_HEIGHT};
        dst.blit(context_.vga->get("gfx/back1"), src_rect);

        Surface blade = zutaten_bob_.frame(194, PaletteHandle{6});
        dst.blit_masked(blade, ViewPoint{8, 30});

        return dst;
      }()},
      active_key_{pms.active_key.value_or(new_row_tag)},
      bake_(pms.bake ? std::make_optional(std::move(*pms.bake)) : std::nullopt),
      listener_(context.input->register_listener(*this)),
      status_(state_.status()) {

  active_topping_ = nullptr;

  create_dropdowns();

  previous_topping_ = nullptr;

  create_input_panel();

  // Check if we're returning from judging
  if (mode_ == PizzaManagerMode::OwnPizzaJudging ||
      mode_ == PizzaManagerMode::CanonicalPizzaJudging ||
      mode_ == PizzaManagerMode::CompetitionJudging) {
    // Handle post-judging immediately
    handle_post_judging();
  }
  else {
    status_.show_restaurant({.state = &state_,
                             .rest = state_.active_restaurant(),
                             .label = "RECIPES"});
  }
}

PizzaManager::~PizzaManager() { context_.cursor->clear_custom_cursor(); }

void PizzaManager::tick() {
  if (dropdowns_.tick()) {
    mark_dirty();
  }

  if (!chopping_) {
    return;
  }

  // End condition, total duration reached (optionally only when horizontal)
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

bool PizzaManager::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }

  // F1: Load user recipe from game/ directory
  if (ev.action == input::Action::LoadRecipe) {
    if (mode_ == PizzaManagerMode::Design) {
      // Debug mode: Load user recipe for testing
      context_.popup->create<NumberInputPopup>(
          "Load recipe?", "", 0, 0, 99, 1,
          [this](int number) { load_user_recipe_for_debug(number); });
    }
    else {
      // Normal: Add to recipe list from game/ directory
      context_.popup->create<NumberInputPopup>(
          "Load user recipe (game/)?", "", 0, 0, 99, 1,
          [this](int number) { load_user_recipe(number); });
    }
    return true;
  }

  // F3: Load canonical/competition recipe from r/ directory
  if (ev.action == input::Action::LoadCanonicalRecipe) {
    if (mode_ == PizzaManagerMode::Design) {
      // Debug mode: Load canonical pizza for testing scoring
      context_.popup->create<NumberInputPopup>(
          "Load canonical pizza?", "", 0, 0, 99, 1,
          [this](int number) { load_canonical_for_debug(number); });
    }
    else {
      // Normal: Add to recipe list from r/ directory
      context_.popup->create<NumberInputPopup>(
          "Load canonical recipe (r/)?", "", 0, 0, 99, 1,
          [this](int number) { load_canonical_recipe(number); });
    }
    return true;
  }

  // Block all other actions in Design mode
  if (mode_ == PizzaManagerMode::Design) {
    return false;
  }

  switch (ev.action) {
  case input::Action::SaveRecipe: {
    auto bob_size_fn =
        make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob_);
    context_.popup->create<NumberInputPopup>(
        "Save recipe as (game/)?", "", 0, 0, 99, 1,
        [this, bob_size_fn](int number) {
          const auto& self = *this; // non-const function, but self is const-ref
          const core::food::Pizza* pizza = self.current_pizza();
          if (!pizza) {
            Logger::warn("No current pizza!");
            return;
          }
          std::filesystem::path path("game/r" + std::to_string(number) +
                                     (platform::is_amiga() ? "" : ".pc"));
          if (fileutils::fexists(path.string())) {
            context_.popup->create<ConfirmPopup>(
                "File already exists, overwrite?",
                [pizza, path, bob_size_fn](bool overwrite) {
                  if (overwrite) {
                    save_prez(*pizza, path, bob_size_fn);
                  }
                });
          }
          else {
            save_prez(*pizza, path, bob_size_fn);
          }
        });
    break;
  }
  default:
    break;
  }
  return true;
}

void PizzaManager::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  switch (mode_) {
  case PizzaManagerMode::View:
    draw_pizza(ctx);
    break;
  case PizzaManagerMode::Design: {
    draw_pizza(ctx);
    // And render them:
    views::ui::InteractionContext ictx{interactions_, [this] { mark_dirty(); }};
    interactions_.create_frame_item(
        ViewRect{0, 0, views::base::MAIN_PANEL_WIDTH,
                 views::base::MAIN_PANEL_HEIGHT},
        "pizza fallback", [this](views::ui::InteractionEvent ui) {
          if (ui.type != views::ui::InteractionEvent::Type::Click) {
            return;
          }
          if (ui.button == views::ui::MouseButton::Left) {
            handle_left_mouse_click(ui.local);
          }
          else if (ui.button == views::ui::MouseButton::Right) {
            handle_right_mouse_click(ui.local);
          }
        });
    dropdowns_.add_interactions(ictx);
    dropdowns_.render(*ctx.surface, context_.font);

    // Chopper
    interactions_.create_frame_item(
        // ViewRect{LegacyConstants::WOOD_BORDER_WIDTH_LEFT + 5, 250 +
        // LegacyConstants::WOOD_BORDER_HEIGHT_TOP, CHOPPER_WIDTH,
        // CHOPPER_HEIGHT},
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
    break;
  }
  case PizzaManagerMode::Ingredients:
    draw_ingredients(ctx);
    break;
  case PizzaManagerMode::Info:
    draw_pizza_info(ctx);
    break;
  case PizzaManagerMode::Recipes:
    draw_recipes(ctx);
    break;
  default:
    Logger::debug("Unhandled pizza mode");
    break;
  }
  if (mode_ == PizzaManagerMode::Info) {
    // Not sure how the original did it but the color indexes for the statbars
    // in index 18 are not correct, so we have to steal the colors from 13.
    auto ovr = make_bar_ui_ramp_override(/*src_pal_id=*/13);
    auto pal_for_screen = gfx::PaletteBank::instance().colors({18}, ovr);
    ctx.surface->colors(pal_for_screen);
  }
  else {
    ctx.surface->clear_custom_palette();
  }
}

void PizzaManager::start_chop() {
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

void PizzaManager::draw_ingredients(const RenderContext& ctx) const {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back4"));

  const auto* pizza = current_pizza();
  if (!pizza) {
    Logger::warn("No current pizza!");
    return;
  }

  context_.font->render_text_onto(*ctx.surface, "recipe " + pizza->name(),
                                  ViewPoint{40, 40}, FontFace::ASTANDAR,
                                  TEXT_DARK_BROWN);

  std::unordered_map<std::string, uint32_t> summary_map;
  uint32_t total_weight = 0;
  for (const auto& topping : pizza->toppings()) {
    std::string ingredient_name = topping->ingredient()->name();
    auto weight = topping->weight();
    summary_map[ingredient_name] += weight;
    total_weight += weight;
  }
  int weight_x = -250;
  int name_x = 140;
  int y = 70;

  context_.font->render_text_onto(*ctx.surface, "TOTAL WEIGHT",
                                  ViewPoint{name_x, y}, FontFace::CSTANDAR,
                                  TEXT_DARK_BROWN);
  context_.font->render_text_onto(
      *ctx.surface, std::to_string(total_weight) + " G", ViewPoint{weight_x, y},
      FontFace::CSTANDAR, TEXT_DARK_BROWN, AlignX::Right);

  for (const auto& item : summary_map) {
    y += 14;
    context_.font->render_text_onto(*ctx.surface, item.first,
                                    ViewPoint{name_x, y}, FontFace::CSTANDAR,
                                    TEXT_DARK_BROWN);
    context_.font->render_text_onto(*ctx.surface,
                                    std::to_string(item.second) + " G",
                                    ViewPoint{weight_x, y}, FontFace::CSTANDAR,
                                    TEXT_DARK_BROWN, AlignX::Right);
  }

  context_.font->render_text_onto(
      *ctx.surface, std::to_string(summary_map.size()) + " ingredients used",
      ViewPoint{130, 306}, FontFace::CSTANDAR, TEXT_DARK_BROWN);
}

void PizzaManager::draw_pizza_info(const RenderContext& ctx) const {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back4"));

  const Pizza* pizza = current_pizza();
  if (!pizza) {
    Logger::warn("No current pizza!");
    return;
  }

  context_.font->render_text_onto(*ctx.surface, "Info: " + pizza->name(),
                                  ViewPoint{40, 40}, FontFace::ASTANDAR,
                                  TEXT_DARK_BROWN);

  // Let's start by drawing the table lines
  uint8_t line_color = 27;
  gfx::draw::line(*ctx.surface, {150 / 2, 154 / 2}, {652 / 2, 154 / 2},
                  line_color);
  gfx::draw::line(*ctx.surface, {328 / 2, 122 / 2}, {328 / 2, 372 / 2},
                  line_color);
  gfx::draw::line(*ctx.surface, {496 / 2, 122 / 2}, {496 / 2, 372 / 2},
                  line_color);

  context_.font->render_text_onto(*ctx.surface, "here",
                                  ViewPoint{372 / 2, 114 / 2},
                                  FontFace::PIZ_NUMB, TEXT_DARK_BROWN);
  context_.font->render_text_onto(*ctx.surface, "all",
                                  ViewPoint{516 / 2, 114 / 2},
                                  FontFace::PIZ_NUMB, TEXT_DARK_BROWN);

  // Get the recipe ID if this is a saved recipe
  std::optional<core::RecipeId> recipe_id;
  if (active_key_ && std::holds_alternative<core::RecipeId>(*active_key_)) {
    recipe_id = std::get<core::RecipeId>(*active_key_);
  }

  // Get data from current restaurant
  auto* restaurant = state_.active_restaurant();
  const auto& menu = restaurant->menu();
  const auto& stats = restaurant->stats();

  // Calculate ingredient cost for this pizza
  // Uses restaurant's weighted-average vendor prices, falls back to city base
  // prices
  auto* city = state_.active_city();
  const auto& inventory = restaurant->inventory();
  Money pizza_cost{0};

  for (const auto& topping : pizza->toppings()) {
    const auto& ingredient = topping->ingredient();
    auto type = ingredient->type();
    int weight_grams = topping->weight();

    // Try to get price from restaurant inventory first (vendor-specific
    // weighted average)
    const auto* stock = inventory.get_ingredient_stock(type);
    Money price_per_1000g;

    if (stock && stock->quantity > 0 && stock->weighted_avg_cost.cents > 0) {
      // Use restaurant's weighted-average cost from vendor purchases
      // This is automatically calculated when vendors deliver ingredients
      price_per_1000g = stock->weighted_avg_cost;
    }
    else {
      // Fall back to city base price if no inventory
      price_per_1000g = city->ingredient_base_price(type);
    }

    pizza_cost += price_per_1000g * weight_grams / 1000;
  }

  // Calculate values for "here" (this restaurant)
  Money here_price{0};
  int here_sales = 0;
  Money here_costs{0};
  Money here_profit{0};
  bool on_current_menu = false;

  if (recipe_id) {
    // Check if pizza is on this restaurant's menu
    if (const auto* menu_entry = menu.find(*recipe_id)) {
      on_current_menu = true;
      here_price = menu_entry->price;

      // Get sales data from yesterday
      if (auto it = stats.yesterday.pizza_sales.find(*recipe_id);
          it != stats.yesterday.pizza_sales.end()) {
        here_sales = it->second.count_sold;
        here_costs = it->second.ingredient_cost;
        here_profit = it->second.profit;
      }
    }
  }

  // Calculate values for "all"
  Money all_price{0};
  int all_sales = 0;
  Money all_costs{0};
  Money all_profit{0};
  int restaurants_with_pizza = 0;

  if (recipe_id) {
    auto* player = state_.active_player();

    // Sum across all player's restaurants in this city that have this pizza on
    // menu
    for (auto* rest : state_.restaurants_for(player->id())) {
      // Only count restaurants in the same city
      if (rest->property()->city()->id() != city->id()) {
        continue;
      }

      // Only include restaurants that have this pizza on their menu
      if (const auto* menu_entry = rest->menu().find(*recipe_id)) {
        restaurants_with_pizza++;
        all_price = all_price + menu_entry->price;

        // Add yesterday's sales data
        if (auto it = rest->stats().yesterday.pizza_sales.find(*recipe_id);
            it != rest->stats().yesterday.pizza_sales.end()) {
          all_sales += it->second.count_sold;
          all_costs = all_costs + it->second.ingredient_cost;
          all_profit = all_profit + it->second.profit;
        }
      }
    }

    // Calculate averages
    if (restaurants_with_pizza > 0) {
      all_price = Money{all_price.cents / restaurants_with_pizza};
    }
  }

  int label_x = 75;
  int label_y = 85;
  int here_x_offset = -220; // "here" column
  int all_x_offset = -140;  // "all" column (further left)

  // Row 1: costs - ALWAYS show ingredient cost, even if not on menu
  context_.font->render_text_onto(*ctx.surface, "costs",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, TEXT_DARKER_BROWN);

  // Show calculated cost in "here" column (always visible)
  context_.font->render_text_onto(
      *ctx.surface, compat::format("{:n}", pizza_cost),
      ViewPoint{label_x + here_x_offset, label_y}, FontFace::PIZ_NUMB,
      TEXT_DARKER_BROWN, AlignX::Right);

  // Show aggregated costs in "all" column (only if on at least one menu)
  if (restaurants_with_pizza > 0) {
    // Use actual costs if available from sales data, otherwise use calculated
    // cost
    Money display_cost = (all_costs.cents > 0) ? all_costs : pizza_cost;
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", display_cost),
        ViewPoint{label_x + all_x_offset, label_y}, FontFace::PIZ_NUMB,
        TEXT_DARKER_BROWN, AlignX::Right);
  }
  label_y += 21;

  // Row 2: price - only show if on menu
  context_.font->render_text_onto(*ctx.surface, "price",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, TEXT_DARKER_BROWN);

  if (on_current_menu) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", here_price),
        ViewPoint{label_x + here_x_offset, label_y}, FontFace::PIZ_NUMB,
        TEXT_DARKER_BROWN, AlignX::Right);
  }

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", all_price),
        ViewPoint{label_x + all_x_offset, label_y}, FontFace::PIZ_NUMB,
        TEXT_DARKER_BROWN, AlignX::Right);
  }
  label_y += 21;

  // Row 3: sales (count, not money) - only show if on menu
  context_.font->render_text_onto(*ctx.surface, "sales",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, TEXT_DARKER_BROWN);

  if (on_current_menu) {
    context_.font->render_text_onto(*ctx.surface, std::to_string(here_sales),
                                    ViewPoint{label_x + here_x_offset, label_y},
                                    FontFace::PIZ_NUMB, TEXT_DARKER_BROWN,
                                    AlignX::Right);
  }

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(*ctx.surface, std::to_string(all_sales),
                                    ViewPoint{label_x + all_x_offset, label_y},
                                    FontFace::PIZ_NUMB, TEXT_DARKER_BROWN,
                                    AlignX::Right);
  }
  label_y += 21;

  // Row 4: profit - only show if on menu
  context_.font->render_text_onto(*ctx.surface, "profit",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, TEXT_DARKER_BROWN);

  if (on_current_menu) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", here_profit),
        ViewPoint{label_x + here_x_offset, label_y}, FontFace::PIZ_NUMB,
        TEXT_DARKER_BROWN, AlignX::Right);
  }

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", all_profit),
        ViewPoint{label_x + all_x_offset, label_y}, FontFace::PIZ_NUMB,
        TEXT_DARKER_BROWN, AlignX::Right);
  }

  context_.font->render_text_onto(*ctx.surface, "popularity",
                                  ViewPoint{154, 200}, FontFace::PIZ_NUMB,
                                  TEXT_DARKER_BROWN);
  constexpr int max_rows_per_column = 3;
  int y_start = 232;
  int x_start = 14;
  // Move this much sidewas for the second column
  constexpr int column_offset = 172;
  constexpr int row_offset = 30;
  Popularity p = pizza->popularity();
  StatBar statbar(context_.blitmap, context_.font);
  int y_offset = 0;
  for (std::size_t i = 0; i < static_cast<std::size_t>(ConsumerType::Count);
       ++i) {
    auto d = static_cast<ConsumerType>(i);
    auto bar = statbar.create_stat_bar(to_string(d), static_cast<int>(p[d]));
    ctx.surface->blit_masked(bar, ViewPoint{x_start, y_start + y_offset});
    // ctx.surface->palette({7});
    y_offset += row_offset;
    if (i + 1 == max_rows_per_column) {
      x_start += column_offset;
      y_offset = 0;
    }
  }
}

void PizzaManager::draw_pizza(const RenderContext& ctx) const {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back1"));
  ctx.surface->blit_masked(chop_horizontal_ ? blade_horizontal_
                                            : blade_vertical_,
                           ViewPoint{CHOPPER_OFFSET_X, CHOPPER_OFFSET_Y});

  const Pizza* pizza = current_pizza();
  if (!pizza) {
    Logger::warn("No current pizza!");
    return;
  }

  for (const auto& topping : pizza->toppings()) {
    renderer_.render(*topping, *ctx.surface, camera_);
  }
}

bool PizzaManager::handle_left_mouse_click(ViewPoint vp) {
  if (mode_ != PizzaManagerMode::Design) {
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
    pizza->update_stats();
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

bool PizzaManager::handle_right_mouse_click(ViewPoint /* p */) {
  if (mode_ != PizzaManagerMode::Design) {
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
    pizza->update_stats();
    status_.show_pizza(PizzaCtx{
        .state = &state_,
        .wip_pizza = &bake_->working,
        .recipe = nullptr,
    });
    mark_dirty();
  }
  return false;
}

bool PizzaManager::is_inside_pizza(ViewPoint p) {
  // Check based on the radius of the pizza:
  return ((PIZZA_CENTER_X - p.x) * (PIZZA_CENTER_X - p.x) +
          (PIZZA_CENTER_Y - p.y) * (PIZZA_CENTER_Y - p.y)) <=
         PIZZA_RADIUS * PIZZA_RADIUS;
}

void PizzaManager::create_dropdowns() {
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
    IngredientDropdown dd = static_cast<IngredientDropdown>(idx);
    Logger::debug("Creating dropdown at x y {}, {}",
                  dd_btn_offset_x + (dd_btn_spacing_x * idx), dd_btn_offset_y);
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
      Logger::debug("Setting active_topping_ to {}",
                    static_cast<int>(ingredient));
      Logger::debug("You clicked {}",
                    active_topping_->ingredient()->name().c_str());
      context_.cursor->custom_cursor(renderer_.image(*active_topping_));
    });
  };
  for (const auto& ing : ingredients) {
    add_ingredient_item(ing.category, ing.id, ing.name);
  }
}

void PizzaManager::change_mode(PizzaManagerMode mode) {
  if (engine::random::rand_percentage_chance(20)) {
    state_.advance_time_blocks(1);
  }
  mode_ = mode;
  create_input_panel();
  mark_dirty();
}

void PizzaManager::create_input_panel() {
  // Make sure we start with a clean slate
  input_panel_->delete_buttons();
  // Then the three buttons (view/ingredients/info) that all
  // modes share:
  auto top_three_buttons = [this] {
    // view pizza
    input_panel_->add_button(Icon::PIZZA, ButtonType::Square,
                             ButtonPosition::TopLeft, "View pizza", [this] {
                               // When in design mode, return to Design view
                               // (editing canvas) from Ingredients or Info
                               // views. When not in design mode, show View
                               // (read-only).
                               if (design_mode_) {
                                 change_mode(PizzaManagerMode::Design);
                               }
                               else {
                                 change_mode(PizzaManagerMode::View);
                               }
                             });

    // ingredients
    input_panel_->add_button(
        Icon::STOCK, ButtonType::Square, ButtonPosition::TopCenter,
        "Ingredients",
        [this] { change_mode(mode_ = PizzaManagerMode::Ingredients); });

    // pizza info
    input_panel_->add_button(Icon::EXAMINE, ButtonType::Square,
                             ButtonPosition::TopRight, "Pizza info",
                             [this] { change_mode(PizzaManagerMode::Info); });
  };

  // Selection cycling (skips <NEW>), and keeps page_ in sync with the new
  // selection, even though the original did not do that.
  auto cycle_selection = [this](int delta) {
    auto* player = state_.active_player();
    const auto& recipes = player->recipes();

    // Rebuild rows same as the list does (TODO maybe refactor this)
    auto rows =
        build_recipe_list_rows(recipes, state_.restaurants_for(player->id()));
    rows.emplace_back(
        RecipeListRow({.name = "<NEW>"})); // last entry, not selectable

    // Build an index of selectable rows (those with recipe_id or canonical)
    std::vector<int> selectable;
    selectable.reserve(rows.size());
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
      if (rows[i].recipe_id) {
        selectable.push_back(i);
      }
    }
    if (selectable.empty()) {
      return;
    }

    // Find current selection index within `selectable`
    int cur_sel_pos = -1;
    for (int pos = 0; pos < static_cast<int>(selectable.size()); ++pos) {
      if (is_selected(rows[selectable[pos]], active_key_)) {
        cur_sel_pos = pos;
        break;
      }
    }

    // If nothing selected yet, start at first/last depending on direction
    if (cur_sel_pos < 0) {
      cur_sel_pos = (delta > 0) ? 0 : (static_cast<int>(selectable.size()) - 1);
    }

    // Move and clamp
    int new_pos = std::clamp(cur_sel_pos + delta, 0,
                             static_cast<int>(selectable.size()) - 1);
    int row_idx = selectable[new_pos];
    const auto& row = rows[row_idx];

    // Update active_key_ (can only be recipe_id since un-created canonical
    // pizzas can not be viewed):
    if (row.recipe_id) {
      change_active_key(RecipeKey{*row.recipe_id});
    }
    else {
      // Shouldn't happen:
      Logger::warn("We didn't find a recipe id in cycle_selection?!");
      return;
    }

    // Ensure page shows the newly selected row
    int target_page = row_idx / recipes_per_page_;
    if (target_page != page_) {
      page_ = target_page;
    }

    mark_dirty();
  };

  auto recipes_buttons = [this, cycle_selection] {
    // turn page
    input_panel_->add_button(
        Icon::ARROW_UP, ButtonType::Square, ButtonPosition::MiddleLeft,
        "Turn page", [this, cycle_selection] {
          if (mode_ == PizzaManagerMode::Recipes) {
            int total =
                state_.active_player()->recipes().size() + 1; // +1 for <NEW>
            int max_page = (total - 1) / recipes_per_page_;
            if (page_ < max_page) {
              page_++;
              mark_dirty();
            }
          }
          else {
            cycle_selection(+1);
          }
        });

    // turn page
    input_panel_->add_button(Icon::ARROW_DOWN, ButtonType::Square,
                             ButtonPosition::BottomLeft, "Turn page",
                             [this, cycle_selection] {
                               if (mode_ == PizzaManagerMode::Recipes) {
                                 if (page_ > 0) {
                                   page_--;
                                   mark_dirty();
                                 }
                               }
                               else {
                                 cycle_selection(-1);
                               }
                             });

    // recipes
    input_panel_->add_button(
        Icon::PARCHMENT, ButtonType::Square, ButtonPosition::MiddleCenter,
        "Recipe list", [this] { change_mode(PizzaManagerMode::Recipes); });

    // transfer
    input_panel_->add_button(
        Icon::SELL, ButtonType::Square, ButtonPosition::MiddleRight, "Transfer",
        [this] {
          if (active_key_ &&
              std::holds_alternative<core::RecipeId>(*active_key_)) {
            auto& menu = state_.active_restaurant()->menu();
            auto id = std::get<core::RecipeId>(*active_key_);

            context_.popup->origin(ViewPoint{30, 30});
            if (menu.find(id)) {
              context_.popup->create<AlertPopup>(
                  "This recipe is already in your menu!");
            }
            else {
              context_.popup->create<TextInputPopup>(
                  "Which price?", 5, [this, id](const std::string& input) {
                    auto price = from_input(input);
                    if (price) {
                      Logger::debug("Price is {}", *price);
                      state_.active_restaurant()->menu().add_or_set(id, *price);
                      mark_dirty();
                    }
                    else {
                      Logger::debug("Price is not valid :(");
                    }
                  });
            }
          }
          else {
            Logger::warn("No active key or not right type");
          }
        });

    // bake pizza
    input_panel_->add_button(Icon::POT, ButtonType::Square,
                             ButtonPosition::BottomCenter, "Bake pizza",
                             [this] { on_bake_pressed(); });
  };

  switch (mode_) {
  case PizzaManagerMode::Recipes:
    top_three_buttons();
    recipes_buttons();
    break;
  case PizzaManagerMode::Ingredients:
  case PizzaManagerMode::Design:
  case PizzaManagerMode::View:
  case PizzaManagerMode::Info:
    top_three_buttons();
    if (design_mode_) {
      // OK
      input_panel_->add_button(Icon::THUMBS_UP, ButtonType::Horizontal,
                               ButtonPosition::BottomLeft, "O.K.",
                               [this] { on_design_finished(); });
    }
    else {
      recipes_buttons();
    }
    break;
  case PizzaManagerMode::CompetitorChallenge:
  case PizzaManagerMode::MonthlyChallenge:
  case PizzaManagerMode::OwnPizzaJudging:
  case PizzaManagerMode::CanonicalPizzaJudging:
  case PizzaManagerMode::CompetitionJudging:
    break;
  }

  // Back
  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back", [this] {
                             if (design_mode_) {
                               state_.advance_time_blocks(5);
                             }
                             else {
                               state_.advance_time_blocks(1);
                             }
                             state_.ui().pop();
                           });
}

void PizzaManager::draw_recipes(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back4"));

  const TextStyle ts{.color = 6,
                     .align = AlignX::Center,
                     .shadow =
                         ShadowStyle{.offset = PxOffset{0, 1}, .color = 20}};

  const ViewPoint title_pos{-20, 42};

  context_.font->render_text_onto(*ctx.surface, "recipe list", title_pos,
                                  FontFace::ASTANDAR, ts);

  const TextStyle pg_ts{.color = 6, .align = AlignX::Left};
  const ViewPoint page_pos{276, 42};

  context_.font->render_text_onto(*ctx.surface,
                                  "page " + std::to_string(page_ + 1), page_pos,
                                  FontFace::CSTANDAR, pg_ts);

  ViewRect name_entry_box{80, 70, 200, 18};
  ViewRect count_entry_box{290, 70, 26, 18};

  auto* player = state_.active_player();
  const auto& recipes = player->recipes();
  auto rows =
      build_recipe_list_rows(recipes, state_.restaurants_for(player->id()));

  // Append <NEW> entry
  rows.emplace_back(RecipeListRow({.name = "<NEW>"}));

  // Calculate visible slice
  const int total = static_cast<int>(rows.size());
  const int start = page_ * recipes_per_page_;
  const int end = std::min(start + recipes_per_page_, total);

  // Adjust page if out of bounds (e.g., after deleting recipes)
  if (page_ > 0 && start >= total) {
    page_ = (total - 1) / recipes_per_page_;
  }

  for (int i = start; i < end; ++i) {
    const auto& row = rows[i];
    std::string interaction_id =
        row.recipe_id ? std::to_string(row.recipe_id->value) + row.name
                      : row.name;
    interactions_.create_frame_item(
        name_entry_box, interaction_id,
        [this, row](views::ui::InteractionEvent ie) {
          if (ie.type == views::ui::InteractionEvent::Type::Click &&
              ie.button == views::ui::MouseButton::Left) {
            if (row.recipe_id) {
              change_active_key(RecipeKey{*row.recipe_id});
            }
            else if (row.canonical) {
              change_active_key(RecipeKey{*row.canonical});
            }
            else {
              change_active_key(new_row_tag);
            }
            mark_dirty();
          }
          if (ie.type == views::ui::InteractionEvent::Type::DoubleClick &&
              ie.button == views::ui::MouseButton::Left) {
            Logger::warn("Double click!");
          }
        });

    const bool selected = is_selected(row, active_key_);
    views::ui::components::draw_beveled_button(
        context_.font, context_.vga->get("gfx/permap"), *ctx.surface,
        name_entry_box, row.name,
        selected ? views::ui::components::ButtonState::Pressed
                 : views::ui::components::ButtonState::Normal);
    if (row.recipe_id) {
      views::ui::components::draw_beveled_button(
          context_.font, context_.vga->get("gfx/permap"), *ctx.surface,
          count_entry_box, std::to_string(row.listed_count),
          views::ui::components::ButtonState::Normal);
    }
    name_entry_box.y += name_entry_box.h;
    count_entry_box.y += count_entry_box.h;
  }
}

void PizzaManager::on_bake_pressed() {
  auto* player = state_.active_player();
  auto rows = build_recipe_list_rows(player->recipes(),
                                     state_.restaurants_for(player->id()));
  rows.push_back(RecipeListRow{.name = "<NEW>"}); // last is <NEW>

  const auto* sel = find_row_by_key(rows, *active_key_);
  if (!sel) {
    Logger::warn("Something is wrong, I don't have a row?!");
    return;
  }
  const auto& row = *sel;

  BakeSession s{};
  if (!row.recipe_id && !row.canonical) {
    s.kind = BakeKind::NewUser;
    s.suggested_name = "";
  }
  else if (!row.recipe_id && row.canonical) {
    s.kind = BakeKind::InstantiateCanonical;
    s.canonical_id = row.canonical;
    s.suggested_name = row.name;
  }
  else {
    s.kind = BakeKind::RecreateExisting;
    s.existing_id = row.recipe_id;
    s.canonical_id = row.canonical;
    s.suggested_name = row.name;
    s.working = Pizza("work in progress pizza");
    auto sp = std::make_shared<BakeSession>(std::move(s));

    context_.popup->origin(ViewPoint{6, 14});
    // Ask first, continue in callback
    context_.popup->create<ConfirmPopup>("Recreate recipe?",
                                         [this, sp](bool ok) {
                                           if (!ok) {
                                             return;
                                           }
                                           this->begin_bake(std::move(*sp));
                                         });
    return;
  }

  begin_bake(std::move(s));
}

void PizzaManager::begin_bake(core::food::BakeSession s) {
  bake_ = std::move(s);
  design_mode_ = true;
  bake_->working.update_stats();
  status_.show_pizza(PizzaCtx{
      .state = &state_,
      .wip_pizza = &bake_->working,
      .recipe = nullptr,
  });
  change_mode(PizzaManagerMode::Design);
}

void PizzaManager::on_design_finished() {
  state_.advance_time_blocks(5);
  design_mode_ = false;
  if (!bake_) {
    Logger::warn("I don't have a bake?!");
    change_mode(PizzaManagerMode::View);
    return;
  }

  // If this is a canonical pizza, load the reference and score it
  if (bake_->canonical_id) {
    auto get_bob_size =
        make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob_);
    int pizza_index = static_cast<int>(*bake_->canonical_id);
    auto canonical_pizza =
        PizzaFactory::create_pizza(pizza_index, get_bob_size);

    if (canonical_pizza) {
      bake_->working.set_scores_from_canonical(*canonical_pizza);
    }
    else {
      Logger::warn("Failed to load canonical pizza {}", pizza_index);
    }
  }

  // Finalize the pizza but don't set name yet for NewUser
  if (bake_->kind == BakeKind::NewUser) {
    bake_->working.finalize("recipe"); // temporary name
  }
  else {
    bake_->working.finalize();
  }

  switch (bake_->kind) {
  case BakeKind::NewUser:
    // Just go to judging, naming happens after
    post_bake();
    return;

  case BakeKind::InstantiateCanonical: {
    auto id = state_.active_player()->recipes().create_from_canonical(
        *bake_->canonical_id, std::move(bake_->working));
    change_active_key(*bake_->canonical_id);
    post_bake();
    return;
  }

  case BakeKind::RecreateExisting: {
    // User already confirmed when pressing "Bake pizza", so just recreate
    finish_recreate_existing();
    return;
  }
  }
  change_mode(PizzaManagerMode::Recipes);
}

void PizzaManager::finish_new_user(std::string name) {
  // Pizza is already finalized with temporary name, just create it with real
  // name
  auto id = state_.active_player()->recipes().create_user(
      std::move(name), std::move(bake_->working));
  change_active_key(id);
  Logger::warn("Active recipe is now %d", id.value);

  // Now go to recipe list
  change_mode(PizzaManagerMode::Recipes);
  bake_.reset();
}

void PizzaManager::finish_recreate_existing() {
  auto& recipes = state_.active_player()->recipes();
  auto rid = *bake_->existing_id;

  // Check if this is a canonical or user recipe
  if (bake_->canonical_id) {
    // Recreating a canonical recipe - use create_from_canonical
    recipes.create_from_canonical(*bake_->canonical_id,
                                  std::move(bake_->working));
    change_active_key(*bake_->canonical_id);
  }
  else {
    // Recreating a user recipe
    const Recipe* existing = recipes.find(rid);
    if (!existing) {
      Logger::warn("Could not find recipe {} to recreate", rid.value);
      cancel_bake();
      return;
    }
    recipes.recreate_user(rid, std::string(existing->display_name()),
                          std::move(bake_->working));
    change_active_key(rid);
  }

  post_bake();
}

void PizzaManager::post_bake() {
  // Move bake session to shared_ptr for the callback
  auto bake_ptr = std::make_shared<core::food::BakeSession>(std::move(*bake_));
  auto active_key = active_key_;
  bool is_canonical = bake_ptr->canonical_id.has_value();

  // Push PizzaJudges state with scores and callback
  // The callback will pop judges and replace PizzaManagerState to trigger
  // post-judging
  state_.ui().push<PizzaJudgesState>(
      bake_ptr->working.scores(),
      [&state = state_, bake_ptr, active_key] {
        // Pop the PizzaJudges view
        state.ui().pop();

        // Replace the PizzaManagerState with updated state to trigger
        // post-judging
        state.ui().replace_top<PizzaManagerState>(
            PizzaManagerMode::OwnPizzaJudging, std::nullopt, bake_ptr,
            active_key);
      },
      is_canonical);
}

void PizzaManager::cancel_bake() {
  change_mode(PizzaManagerMode::View);
  bake_.reset();
}

// Helper: current pizza to render, if any
Pizza* PizzaManager::current_pizza() {
  if (bake_) {
    return &bake_->working;
  }
  Logger::warn(
      "You're asking for a non const pizza without being in bake mode!");
  return nullptr;
}

const Pizza* PizzaManager::current_pizza() const {
  if (bake_) {
    return &bake_->working;
  }
  auto* player = state_.active_player();
  auto& recipes = state_.active_player()->recipes();

  if (!active_key_) {
    return nullptr;
  }

  // Handle RecipeId
  if (std::holds_alternative<core::RecipeId>(*active_key_)) {
    auto id = std::get<core::RecipeId>(*active_key_);
    if (const core::food::Recipe* r = recipes.find(id)) {
      return &r->pizza();
    }
  }

  // Handle CanonicalPizza
  if (std::holds_alternative<core::food::CanonicalPizza>(*active_key_)) {
    auto cp = std::get<core::food::CanonicalPizza>(*active_key_);
    const auto& slot = recipes.canon[static_cast<int>(cp)];
    if (slot.created()) {
      return &slot.recipe->pizza();
    }
  }

  // NewRowTag: either no pizza yet, or render an empty base/template
  return nullptr;
}

void PizzaManager::load_user_recipe(int number) {
  // Create the pizza from disk (from game/ directory)
  std::string filename =
      "game/r" + std::to_string(number) + (platform::is_amiga() ? "" : ".pc");

  // Check if file exists first
  if (!fileutils::fexists(filename)) {
    context_.popup->create<ConfirmPopup>(
        "File '" + filename + "' not|found! Try again?",
        [this, number](bool retry) {
          if (retry) {
            load_user_recipe(number); // Retry with same number
          }
        });
    return;
  }

  auto get_bob_size =
      make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob_);

  auto pizza = PizzaFactory::create_pizza(filename, get_bob_size);
  if (!pizza) {
    Logger::warn("PizzaFactory::create_pizza('{}') returned null", filename);
    return;
  }

  // Figure out what recipe slot is active
  if (!active_key_) {
    Logger::warn("No active recipe slot selected in PizzaManager");
    return;
  }

  auto* player = state_.active_player();
  auto& recipes = player->recipes();

  // Always treat game/ recipes as user recipes
  auto id = recipes.create_user(pizza->name(), std::move(*pizza));
  Logger::info("Loaded user recipe from {} into user slot {}", filename,
               id.value);

  mark_dirty();
}

void PizzaManager::load_canonical_recipe(int number) {
  // Create the pizza from disk (from r/ directory)
  std::string filename =
      "r/r" + std::to_string(number) + (platform::is_amiga() ? "" : ".pc");

  // Check if file exists first
  if (!fileutils::fexists(filename)) {
    context_.popup->create<ConfirmPopup>(
        "File '" + filename + "' not|found! Try again?",
        [this, number](bool retry) {
          if (retry) {
            load_canonical_recipe(number); // Retry with same number
          }
        });
    return;
  }

  auto get_bob_size =
      make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob_);

  auto pizza = PizzaFactory::create_pizza(filename, get_bob_size);
  if (!pizza) {
    Logger::warn("PizzaFactory::create_pizza('{}') returned null", filename);
    return;
  }

  // Figure out what recipe slot is active
  if (!active_key_) {
    Logger::warn("No active recipe slot selected in PizzaManager");
    return;
  }

  auto* player = state_.active_player();
  auto& recipes = player->recipes();

  // If number is 0-13, treat as canonical pizza
  if (number < 14 &&
      std::holds_alternative<core::food::CanonicalPizza>(*active_key_)) {
    auto cid = std::get<core::food::CanonicalPizza>(*active_key_);
    auto id = recipes.create_from_canonical(cid, std::move(*pizza));
    Logger::info("Loaded canonical recipe from {} into slot {}", filename,
                 id.value);
  }
  else {
    // Competition recipes (14+) are treated as user recipes
    auto id = recipes.create_user(pizza->name(), std::move(*pizza));
    Logger::info("Loaded recipe from {} into user slot {}", filename, id.value);
  }

  mark_dirty();
}

void PizzaManager::load_recipe_for_debug(const std::string& filename) {
  if (!bake_) {
    Logger::warn("Not in bake mode, cannot load recipe for debug");
    return;
  }

  // Check if file exists first
  if (!fileutils::fexists(filename)) {
    context_.popup->create<ConfirmPopup>(
        "File '" + filename + "' not|found! Try again?",
        [this, filename](bool retry) {
          if (retry) {
            load_recipe_for_debug(filename); // Retry with same filename
          }
        });
    return;
  }

  auto get_bob_size =
      make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob_);

  // Load the pizza from the specified file
  auto pizza = PizzaFactory::create_pizza(filename, get_bob_size);
  if (!pizza) {
    Logger::warn("PizzaFactory::create_pizza('{}') returned null", filename);
    return;
  }

  // Clear all existing toppings from working pizza
  while (auto topping = bake_->working.remove_latest_topping()) {
    // Keep removing until empty
  }

  // Collect toppings in a temporary vector to preserve order
  std::vector<std::unique_ptr<Topping>> temp_toppings;
  while (auto topping = pizza->remove_latest_topping()) {
    temp_toppings.push_back(std::move(topping));
  }

  // Add toppings back in reverse order (preserving original order)
  for (auto it = temp_toppings.rbegin(); it != temp_toppings.rend(); ++it) {
    bake_->working.add_topping(std::move(*it));
  }

  // Update stats and refresh display
  bake_->working.update_stats();
  status_.show_pizza(PizzaCtx{
      .state = &state_,
      .wip_pizza = &bake_->working,
      .recipe = nullptr,
  });

  Logger::info("DEBUG: Loaded pizza from {} for testing", filename);
  mark_dirty();
}

void PizzaManager::load_user_recipe_for_debug(int number) {
  std::string filename =
      "game/r" + std::to_string(number) + (platform::is_amiga() ? "" : ".pc");
  load_recipe_for_debug(filename);
}

void PizzaManager::load_canonical_for_debug(int number) {
  std::string filename =
      "r/r" + std::to_string(number) + (platform::is_amiga() ? "" : ".pc");
  load_recipe_for_debug(filename);
}

void PizzaManager::change_active_key(RecipeKey active_key) {
  active_key_ = active_key;
  const auto& self = *this; // non-const function, but self is const-ref
  const auto& recipes = state_.active_player()->recipes();
  if (std::holds_alternative<core::RecipeId>(*active_key_)) {
    auto id = std::get<core::RecipeId>(*active_key_);
    const core::food::Recipe* r = recipes.find(id);
    if (bake_) {
      bake_->working.update_stats();
    }
    status_.show_pizza({.state = &state_,
                        .wip_pizza = (bake_ ? &bake_->working : nullptr),
                        .recipe = r});
  }
  else {
    status_.show_restaurant({.state = &state_,
                             .rest = state_.active_restaurant(),
                             .label = "RECIPES"});
  }
}

void PizzaManager::update_status_ingredient() {
  status_.show_topping(
      ToppingCtx{.state = &state_, .topping = active_topping_.get()});
}

void PizzaManager::handle_post_judging() {
  state_.advance_time_blocks(3);
  if (!bake_) {
    Logger::warn("No bake session after judging!");
    change_mode(PizzaManagerMode::Recipes);
    return;
  }

  // Handle post-judging flow based on bake kind
  if (bake_->kind == BakeKind::NewUser) {
    // Show Info screen first, then ask for name
    change_mode(PizzaManagerMode::Info);

    constexpr int max_len = 18;
    context_.popup->create<TextInputPopup>(
        "name of recipe:", max_len,
        [this](std::string input) { this->finish_new_user(std::move(input)); });
  }
  else {
    // For other cases, just go to Recipes
    change_mode(PizzaManagerMode::Recipes);
    bake_.reset();
  }
}
