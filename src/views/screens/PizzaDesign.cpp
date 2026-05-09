/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/PizzaDesign.h"

#include "core/Demographics.h"
#include "core/GameTypes.h"
#include "core/city/Property.h"
#include "core/events/LeventId.h"
#include "core/finance/Ledger.h"
#include "core/finance/MoneyParse.h"
#include "core/food/RecipeCodec.h"
#include "core/state/GameState.h"
#include "core/util/LegacyConstants.h"
#include "engine/PlatformPaths.h"
#include "engine/Random.h"
#include "engine/audio/AudioManager.h"
#include "engine/geom/Spaces.h"
#include "engine/input/InputManager.h"
#include "engine/ui/Alignment.h"
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

#include <algorithm>
#include <cassert>
#include <memory>
#include <optional>

static constexpr int CHOP_FLIP_PERIOD_MS = 55;
static constexpr uint64_t GET_READY_WAIT_MS = 2000;
static constexpr uint64_t RESULTS_ROW_INTERVAL_MS = 1200;
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

PizzaDesign::PizzaDesign(GameContext& context, GameState& state,
                         PizzaDesignState pms)
    : MainPanelView(context, state), mode_(pms.initial_mode),
      design_mode_{pms.initial_mode == PizzaDesignMode::Design},
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
      competing_player_id_(pms.competing_player_id),
      on_done_(std::move(pms.on_done)),
      listener_(context.input->register_listener(*this)),
      status_(state_.status()) {

  active_topping_ = nullptr;

  create_dropdowns();

  previous_topping_ = nullptr;

  create_input_panel();

  // Check if we're returning from judging
  if (mode_ == PizzaDesignMode::OwnPizzaJudging ||
      mode_ == PizzaDesignMode::CanonicalPizzaJudging ||
      mode_ == PizzaDesignMode::CompetitionJudging) {
    if (pms.competition_session) {
      competition_ = std::move(*pms.competition_session);
    }
    handle_post_judging();
  }
  else if (mode_ == PizzaDesignMode::MonthlyChallenge ||
           mode_ == PizzaDesignMode::CompetitorChallenge) {
    if (pms.competition_session) {
      begin_competition(pms.competition_session);
    }
    else {
      Logger::warn("Competition mode entered without a session!");
      state_.ui().pop();
    }
  }
  else if (mode_ == PizzaDesignMode::AnnualCompetitionDesign) {
    if (pms.competition_session) {
      begin_annual_competition(pms.competition_session);
    }
    else {
      Logger::warn("AnnualCompetitionDesign entered without a session!");
      state_.ui().pop();
    }
  }
  else {
    status_.show_restaurant({.state = &state_,
                             .rest = state_.active_restaurant(),
                             .label = "RECIPES"});
  }
}

PizzaDesign::~PizzaDesign() {
  context_.cursor->clear_custom_cursor();
  if (context_.audio->is_playing()) {
    context_.audio->stop_music();
  }
}

void PizzaDesign::tick() {
  if (dropdowns_.tick()) {
    mark_dirty();
  }

  if (mode_ == PizzaDesignMode::CompetitionReveal && competition_) {
    auto& c = *competition_;
    if (!c.reveal_done) {
      int topping_count =
          static_cast<int>(c.canonical_pizza->toppings().size());
      if (c.reveal_idx < topping_count) {
        int current_max = reveal_speed_boost_ ? 1 : c.delay_ticks;
        reveal_delay_remaining_ticks_ =
            std::min(reveal_delay_remaining_ticks_, current_max);
        if (reveal_delay_remaining_ticks_ > 0) {
          --reveal_delay_remaining_ticks_;
        }
        else {
          reveal_delay_remaining_ticks_ = current_max;
          c.reveal_idx++;
          update_competition_status();
          mark_dirty();
        }
      }
      else {
        c.reveal_done = true;
        c.get_ready = true;
        c.get_ready_start_ms = context_.clock.now_ms;
        update_competition_status();
        mark_dirty();
      }
    }
    else if (c.get_ready) {
      if (context_.clock.elapsed_since(c.get_ready_start_ms,
                                       GET_READY_WAIT_MS)) {
        c.get_ready = false;
        on_reveal_complete();
      }
    }
    return;
  }

  if (mode_ == PizzaDesignMode::CompetitionDesign) {
    competition_design_ticks_++;
  }

  if (mode_ == PizzaDesignMode::CompetitionResults && competition_) {
    auto& c = *competition_;
    int total = static_cast<int>(c.participants.size());
    if (c.results_rows_shown <= total) {
      if (context_.clock.elapsed_since(c.results_last_row_ms,
                                       RESULTS_ROW_INTERVAL_MS)) {
        c.results_rows_shown++;
        c.results_last_row_ms = context_.clock.now_ms;
        mark_dirty();
      }
    }
    return;
  }

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

bool PizzaDesign::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }

  // F1: Load user recipe from game/ directory
  if (ev.action == input::Action::LoadRecipe) {
    if (mode_ == PizzaDesignMode::Design) {
      // Debug mode: Load user recipe for testing
      context_.popup->create<NumberInputPopup>(
          "Load recipe?", "", 0, 0, 99, 1,
          [this](int number) { load_user_recipe_for_debug(number); });
    }
    else {
      // Normal: Add to recipe list from game/ directory
      context_.popup->create<NumberInputPopup>(
          "Load which recipe?", "", 0, 0, 99, 1,
          [this](int number) { load_user_recipe(number); });
    }
    return true;
  }

  // F3: Load canonical/competition recipe from r/ directory
  if (ev.action == input::Action::LoadCanonicalRecipe) {
    if (mode_ == PizzaDesignMode::Design) {
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
  if (mode_ == PizzaDesignMode::Design) {
    return false;
  }

  switch (ev.action) {
  case input::Action::SaveRecipe: {
    auto bob_size_fn =
        make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob_);
    context_.popup->create<NumberInputPopup>(
        "Save recipe as...", "", 0, 0, 99, 1, [this, bob_size_fn](int number) {
          const auto& self = *this; // non-const function, but self is const-ref
          const core::food::Pizza* pizza = self.current_pizza();
          if (!pizza) {
            Logger::warn("No current pizza!");
            return;
          }
          std::filesystem::path path(platform_paths::recipes_dir() + "r" +
                                     std::to_string(number) +
                                     (platform::is_amiga() ? "" : ".pc"));
          std::filesystem::create_directories(path.parent_path());
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

void PizzaDesign::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  switch (mode_) {
  case PizzaDesignMode::View:
    draw_pizza(ctx);
    break;
  case PizzaDesignMode::AnnualCompetitionDesign:
  case PizzaDesignMode::CompetitionDesign:
  case PizzaDesignMode::Design:
    draw_pizza(ctx);
    setup_pizza_design_interactions(ctx);
    break;
  case PizzaDesignMode::Ingredients:
    draw_ingredients(ctx);
    break;
  case PizzaDesignMode::Info:
    draw_pizza_info(ctx);
    break;
  case PizzaDesignMode::Recipes:
    draw_recipes(ctx);
    break;
  case PizzaDesignMode::CompetitionReveal: {
    draw_competition_reveal(ctx);
    views::ui::InteractionContext ictx{interactions_, [this] { mark_dirty(); }};
    interactions_.create_frame_item(
        ViewRect{0, 0, views::base::MAIN_PANEL_WIDTH,
                 views::base::MAIN_PANEL_HEIGHT},
        "reveal speed boost", [this](views::ui::InteractionEvent ui) {
          using Type = views::ui::InteractionEvent::Type;
          if (ui.type == Type::ButtonDown &&
              ui.button == views::ui::MouseButton::Left) {
            reveal_speed_boost_ = true;
          }
          else if (ui.type == Type::ButtonUp || ui.type == Type::Cancelled ||
                   ui.type == Type::Leave) {
            reveal_speed_boost_ = false;
          }
        });
    break;
  }
  case PizzaDesignMode::CompetitionResults:
    draw_competition_results(ctx);
    break;
  default:
    Logger::debug("Unhandled pizza mode");
    break;
  }
  if (mode_ == PizzaDesignMode::Info) {
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

void PizzaDesign::draw_ingredients(const RenderContext& ctx) const {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back4"));

  const auto* pizza = current_pizza();
  if (!pizza) {
    Logger::warn("No current pizza!");
    return;
  }

  constexpr int text_color = 5;
  context_.font->render_text_onto(*ctx.surface, "recipe " + pizza->name(),
                                  ViewPoint{40, 40}, FontFace::ASTANDAR,
                                  text_color);

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
                                  text_color);
  context_.font->render_text_onto(
      *ctx.surface, std::to_string(total_weight) + " G", ViewPoint{weight_x, y},
      FontFace::CSTANDAR, text_color, AlignX::Right);

  for (const auto& item : summary_map) {
    y += 14;
    context_.font->render_text_onto(*ctx.surface, item.first,
                                    ViewPoint{name_x, y}, FontFace::CSTANDAR,
                                    text_color);
    context_.font->render_text_onto(
        *ctx.surface, std::to_string(item.second) + " G",
        ViewPoint{weight_x, y}, FontFace::CSTANDAR, text_color, AlignX::Right);
  }

  context_.font->render_text_onto(
      *ctx.surface, std::to_string(summary_map.size()) + " ingredients used",
      ViewPoint{130, 306}, FontFace::CSTANDAR, text_color);
}

void PizzaDesign::draw_pizza_info(const RenderContext& ctx) const {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back4"));

  const Pizza* pizza = current_pizza();
  if (!pizza) {
    Logger::warn("No current pizza!");
    return;
  }

  int text_color = 5;

  context_.font->render_text_onto(*ctx.surface, "Info: " + pizza->name(),
                                  ViewPoint{40, 40}, FontFace::ASTANDAR,
                                  text_color);

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
                                  FontFace::PIZ_NUMB, text_color);
  context_.font->render_text_onto(*ctx.surface, "all",
                                  ViewPoint{516 / 2, 114 / 2},
                                  FontFace::PIZ_NUMB, text_color);

  // Get the recipe ID if this is a saved recipe
  std::optional<core::RecipeId> recipe_id;
  if (active_key_ && std::holds_alternative<core::RecipeId>(*active_key_)) {
    recipe_id = std::get<core::RecipeId>(*active_key_);
  }

  // Get data from current restaurant
  auto* restaurant = state_.active_restaurant();
  const auto& menu = restaurant->menu();
  const auto& stats = restaurant->stats();
  auto* city = state_.active_city();

  // Compute live ingredient cost for a pizza at a given restaurant.
  // Uses weighted-average vendor prices, falls back to city base prices.
  auto compute_pizza_cost = [&](const auto* rest) {
    const auto& inv = rest->inventory();
    Money cost{0};
    for (const auto& topping : pizza->toppings()) {
      auto type = topping->ingredient()->type();
      int weight_grams = topping->weight();
      const auto* stock = inv.get_ingredient_stock(type);
      Money price_per_1000g;
      if (stock && stock->quantity > 0 && stock->weighted_avg_cost > 0_usd) {
        price_per_1000g = stock->weighted_avg_cost;
      }
      else {
        price_per_1000g = city->ingredient_base_price(type);
      }
      cost += price_per_1000g * weight_grams / 1000;
    }
    return cost;
  };

  Money pizza_cost = compute_pizza_cost(restaurant);

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

    for (auto* rest : state_.restaurants_for(player->id())) {
      if (rest->property().city().id() != city->id()) {
        continue;
      }

      if (const auto* menu_entry = rest->menu().find(*recipe_id)) {
        restaurants_with_pizza++;
        all_price = all_price + menu_entry->price;
        all_costs = all_costs + compute_pizza_cost(rest);

        if (auto it = rest->stats().yesterday.pizza_sales.find(*recipe_id);
            it != rest->stats().yesterday.pizza_sales.end()) {
          all_sales += it->second.count_sold;
          all_profit = all_profit + it->second.profit;
        }
      }
    }

    if (restaurants_with_pizza > 0) {
      all_price = Money{all_price.cents / restaurants_with_pizza};
      all_costs = Money{all_costs.cents / restaurants_with_pizza};
    }
  }

  int label_x = 75;
  int label_y = 85;
  int here_x_offset = -220; // "here" column
  int all_x_offset = -140;  // "all" column (further left)
  text_color = 6;

  // costs
  context_.font->render_text_onto(*ctx.surface, "costs",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, text_color);

  // Show calculated cost in "here" column
  context_.font->render_text_onto(
      *ctx.surface, compat::format("{:n}", pizza_cost),
      ViewPoint{label_x + here_x_offset, label_y}, FontFace::PIZ_NUMB,
      text_color, AlignX::Right);

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", all_costs),
        ViewPoint{label_x + all_x_offset, label_y}, FontFace::PIZ_NUMB,
        text_color, AlignX::Right);
  }
  label_y += 21;

  // price - only show if on menu
  context_.font->render_text_onto(*ctx.surface, "price",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, text_color);

  if (on_current_menu) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", here_price),
        ViewPoint{label_x + here_x_offset, label_y}, FontFace::PIZ_NUMB,
        text_color, AlignX::Right);
  }

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", all_price),
        ViewPoint{label_x + all_x_offset, label_y}, FontFace::PIZ_NUMB,
        text_color, AlignX::Right);
  }
  label_y += 21;

  // sales (count)
  context_.font->render_text_onto(*ctx.surface, "sales",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, text_color);

  if (on_current_menu) {
    context_.font->render_text_onto(*ctx.surface, std::to_string(here_sales),
                                    ViewPoint{label_x + here_x_offset, label_y},
                                    FontFace::PIZ_NUMB, text_color,
                                    AlignX::Right);
  }

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(*ctx.surface, std::to_string(all_sales),
                                    ViewPoint{label_x + all_x_offset, label_y},
                                    FontFace::PIZ_NUMB, text_color,
                                    AlignX::Right);
  }
  label_y += 21;

  // profit
  context_.font->render_text_onto(*ctx.surface, "profit",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, text_color);

  if (on_current_menu) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", here_profit),
        ViewPoint{label_x + here_x_offset, label_y}, FontFace::PIZ_NUMB,
        text_color, AlignX::Right);
  }

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", all_profit),
        ViewPoint{label_x + all_x_offset, label_y}, FontFace::PIZ_NUMB,
        text_color, AlignX::Right);
  }

  context_.font->render_text_onto(*ctx.surface, "popularity",
                                  ViewPoint{154, 200}, FontFace::PIZ_NUMB,
                                  text_color);
  constexpr int max_rows_per_column = 3;
  int y_start = 232;
  int x_start = 14;
  // Move this much sideways for the second column
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
    y_offset += row_offset;
    if (i + 1 == max_rows_per_column) {
      x_start += column_offset;
      y_offset = 0;
    }
  }
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

void PizzaDesign::change_mode(PizzaDesignMode mode) {
  if (engine::random::rand_percentage_chance(20)) {
    state_.advance_time_blocks(1);
  }
  mode_ = mode;
  create_input_panel();
  mark_dirty();
}

void PizzaDesign::create_input_panel() {
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
                               if (!std::as_const(*this).current_pizza()) {
                                 return;
                               }
                               if (design_mode_) {
                                 change_mode(PizzaDesignMode::Design);
                               }
                               else {
                                 change_mode(PizzaDesignMode::View);
                               }
                             });

    // ingredients
    input_panel_->add_button(Icon::STOCK, ButtonType::Square,
                             ButtonPosition::TopCenter, "Ingredients", [this] {
                               if (!std::as_const(*this).current_pizza()) {
                                 return;
                               }
                               change_mode(mode_ =
                                               PizzaDesignMode::Ingredients);
                             });

    // pizza info
    input_panel_->add_button(Icon::EXAMINE, ButtonType::Square,
                             ButtonPosition::TopRight, "Pizza info", [this] {
                               if (!std::as_const(*this).current_pizza()) {
                                 return;
                               }
                               change_mode(PizzaDesignMode::Info);
                             });
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
          if (mode_ == PizzaDesignMode::Recipes) {
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
                               if (mode_ == PizzaDesignMode::Recipes) {
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
    input_panel_->add_button(Icon::PARCHMENT, ButtonType::Square,
                             ButtonPosition::MiddleCenter, "Recipe list",
                             [this] { change_mode(PizzaDesignMode::Recipes); });

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
  case PizzaDesignMode::Recipes:
    top_three_buttons();
    recipes_buttons();
    break;
  case PizzaDesignMode::Ingredients:
  case PizzaDesignMode::Design:
  case PizzaDesignMode::View:
  case PizzaDesignMode::Info:
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
  case PizzaDesignMode::CompetitorChallenge:
  case PizzaDesignMode::MonthlyChallenge:
  case PizzaDesignMode::OwnPizzaJudging:
  case PizzaDesignMode::CanonicalPizzaJudging:
  case PizzaDesignMode::CompetitionJudging:
  case PizzaDesignMode::CompetitionReveal:
  case PizzaDesignMode::CompetitionResults:
  case PizzaDesignMode::AnnualCompetitionDesign:
    // just the back button
    break;
  case PizzaDesignMode::CompetitionDesign:
    input_panel_->add_button(Icon::THUMBS_UP, ButtonType::Horizontal,
                             ButtonPosition::BottomLeft, "Done",
                             [this] { on_competition_design_finished(); });
    break;
  }

  // Back
  input_panel_->add_button(
      Icon::ARROW_BACK, ButtonType::Square, ButtonPosition::BottomRight, "Back",
      [this] {
        if (mode_ == PizzaDesignMode::AnnualCompetitionDesign) {
          on_design_finished();
          return;
        }
        if (mode_ == PizzaDesignMode::CompetitionReveal ||
            mode_ == PizzaDesignMode::CompetitionDesign ||
            mode_ == PizzaDesignMode::CompetitionResults) {
          competition_.reset();
          bake_.reset();
          state_.ui().pop();
          if (pending_competition_message_) {
            state_.deliver_message(*pending_competition_message_);
          }
          if (on_done_) {
            on_done_();
          }
          return;
        }
        if (bake_) {
          if (state_.options().qol_improvements &&
              !bake_->working.toppings().empty()) {
            context_.popup->create<ConfirmPopup>("Discard pizza?",
                                                 [this](bool agree) {
                                                   if (agree) {
                                                     cancel_bake();
                                                   }
                                                 });
          }
          else {
            cancel_bake();
          }
        }
        else {
          if (design_mode_) {
            state_.advance_time_blocks(5);
          }
          else {
            state_.advance_time_blocks(1);
          }
          state_.ui().pop();
        }
      });
}

void PizzaDesign::draw_recipes(const RenderContext& ctx) {
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

void PizzaDesign::on_bake_pressed() {
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
    s.working = Pizza(s.suggested_name);
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

void PizzaDesign::begin_bake(core::food::BakeSession s) {
  bake_ = std::move(s);
  design_mode_ = true;
  bake_->working.update_stats(state_.customer_taste_profiles());
  status_.show_pizza(PizzaCtx{
      .state = &state_,
      .wip_pizza = &bake_->working,
      .recipe = nullptr,
  });
  change_mode(PizzaDesignMode::Design);
}

void PizzaDesign::on_design_finished() {
  state_.advance_time_blocks(5);
  design_mode_ = false;
  if (!bake_) {
    Logger::warn("I don't have a bake?!");
    change_mode(PizzaDesignMode::View);
    return;
  }

  // If this is a canonical pizza, load the reference and score it
  if (bake_->canonical_id) {
    auto get_bob_size =
        make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob_);
    int pizza_index = static_cast<int>(*bake_->canonical_id);
    auto canonical_pizza = PizzaFactory::create_pizza(
        pizza_index, get_bob_size, state_.customer_taste_profiles());

    if (canonical_pizza) {
      bake_->working.set_scores_from_canonical(*canonical_pizza);
    }
    else {
      Logger::warn("Failed to load canonical pizza {}", pizza_index);
    }
  }

  // Finalize the pizza but don't set name yet for NewUser
  if (bake_->kind == BakeKind::NewUser) {
    bake_->working.finalize(state_.customer_taste_profiles(),
                            "recipe"); // temporary name
  }
  else {
    bake_->working.finalize(state_.customer_taste_profiles());
  }

  post_bake();
}

void PizzaDesign::finish_new_user(std::string name) {
  bake_->working.name(name);
  auto* cp = competing_player();
  auto id =
      cp->recipes().create_user(std::move(name), std::move(bake_->working));
  change_active_key(id);

  if (competition_) {
    // Annual competition: hand results to GameState and exit.
    state_.ui().pop();
    state_.finalize_annual_pizza_competition(std::move(*competition_));
    return;
  }

  change_mode(PizzaDesignMode::Recipes);
  bake_.reset();
}

void PizzaDesign::finish_recreate_existing() {
  auto& recipes = state_.active_player()->recipes();
  auto rid = *bake_->existing_id;

  // Check if this is a canonical or user recipe
  if (bake_->canonical_id) {
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
}

void PizzaDesign::post_bake() {
  auto bake_ptr = std::make_shared<core::food::BakeSession>(std::move(*bake_));
  auto active_key = active_key_;
  JudgingMode jmode = bake_ptr->canonical_id.has_value()
                          ? JudgingMode::Canonical
                          : JudgingMode::Own;

  // Carry the annual competition session through the judge screen so that the
  // PizzaDesign afterwards has access
  auto annual_session =
      competition_
          ? std::make_shared<CompetitionSession>(std::move(*competition_))
          : nullptr;
  competition_.reset();

  state_.ui().push<PizzaJudgesState>(
      bake_ptr->working.scores(),
      [&state = state_, bake_ptr, active_key, annual_session] {
        state.ui().pop();
        state.ui().replace_top<PizzaDesignState>(
            PizzaDesignMode::OwnPizzaJudging, std::nullopt, bake_ptr,
            active_key, annual_session);
      },
      jmode);
}

void PizzaDesign::cancel_bake() {
  active_topping_.reset();
  context_.cursor->clear_custom_cursor();
  change_mode(PizzaDesignMode::Recipes);
  bake_.reset();
}

Pizza* PizzaDesign::current_pizza() {
  if (bake_) {
    return &bake_->working;
  }
  Logger::warn(
      "You're asking for a non const pizza without being in bake mode!");
  return nullptr;
}

const Pizza* PizzaDesign::current_pizza() const {
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

  // NewRowTag: either no pizza yet, or render an empty pizza
  return nullptr;
}

void PizzaDesign::load_user_recipe(int number) {
  std::string filename = platform_paths::recipes_dir() + "r" +
                         std::to_string(number) +
                         (platform::is_amiga() ? "" : ".pc");

  // Check if file exists first
  if (!fileutils::fexists(filename)) {
    context_.popup->create<ConfirmPopup>("Pizza r'" + std::to_string(number) +
                                             "' not found! Try again?",
                                         [this, number](bool retry) {
                                           if (retry) {
                                             load_user_recipe(number);
                                           }
                                         });
    return;
  }

  auto get_bob_size =
      make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob_);

  auto pizza = PizzaFactory::create_pizza(filename, get_bob_size,
                                          state_.customer_taste_profiles());
  if (!pizza) {
    Logger::warn("PizzaFactory::create_pizza('{}') returned null", filename);
    return;
  }

  // Figure out what recipe slot is active
  if (!active_key_) {
    Logger::warn("No active recipe slot selected in PizzaDesign");
    return;
  }

  auto* player = state_.active_player();
  auto& recipes = player->recipes();

  const std::string& new_name = pizza->name();
  bool already_exists = std::any_of(recipes.user.begin(), recipes.user.end(),
                                    [&](const core::food::Recipe& r) {
                                      return r.display_name() == new_name;
                                    });
  if (already_exists) {
    context_.popup->create<AlertPopup>("This recipe is already in your list!");
    return;
  }

  // Always treat loaded recipes as user recipes
  auto id = recipes.create_user(new_name, std::move(*pizza));
  Logger::info("Loaded user recipe from {} into user slot {}", filename,
               id.value);

  mark_dirty();
}

void PizzaDesign::load_canonical_recipe(int number) {
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

  auto pizza = PizzaFactory::create_pizza(filename, get_bob_size,
                                          state_.customer_taste_profiles());
  if (!pizza) {
    Logger::warn("PizzaFactory::create_pizza('{}') returned null", filename);
    return;
  }

  // Figure out what recipe slot is active
  if (!active_key_) {
    Logger::warn("No active recipe slot selected in PizzaDesign");
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

void PizzaDesign::load_recipe_for_debug(const std::string& filename) {
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
  auto pizza = PizzaFactory::create_pizza(filename, get_bob_size,
                                          state_.customer_taste_profiles());
  if (!pizza) {
    Logger::warn("PizzaFactory::create_pizza('{}') returned null", filename);
    return;
  }

  // Clear all existing toppings from working pizza
  while (auto topping = bake_->working.remove_latest_topping()) {
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
  bake_->working.update_stats(state_.customer_taste_profiles());
  status_.show_pizza(PizzaCtx{
      .state = &state_,
      .wip_pizza = &bake_->working,
      .recipe = nullptr,
  });

  Logger::info("DEBUG: Loaded pizza from {} for testing", filename);
  mark_dirty();
}

void PizzaDesign::load_user_recipe_for_debug(int number) {
  std::string filename = platform_paths::recipes_dir() + "r" +
                         std::to_string(number) +
                         (platform::is_amiga() ? "" : ".pc");
  load_recipe_for_debug(filename);
}

void PizzaDesign::load_canonical_for_debug(int number) {
  std::string filename =
      "r/r" + std::to_string(number) + (platform::is_amiga() ? "" : ".pc");
  load_recipe_for_debug(filename);
}

void PizzaDesign::change_active_key(RecipeKey active_key) {
  active_key_ = active_key;
  const auto& self = *this; // non-const function, but self is const-ref
  const auto& recipes = state_.active_player()->recipes();
  if (std::holds_alternative<core::RecipeId>(*active_key_)) {
    auto id = std::get<core::RecipeId>(*active_key_);
    const core::food::Recipe* r = recipes.find(id);
    if (bake_) {
      bake_->working.update_stats(state_.customer_taste_profiles());
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

void PizzaDesign::update_status_ingredient() {
  status_.show_topping(
      ToppingCtx{.state = &state_, .topping = active_topping_.get()});
}

void PizzaDesign::handle_post_judging() {
  state_.advance_time_blocks(3);

  if (mode_ == PizzaDesignMode::CompetitionJudging) {
    if (!competition_) {
      Logger::warn("CompetitionJudging but no competition session!");
      state_.ui().pop();
      return;
    }
    if (bake_) {
      int match_score = bake_->working.imitation();
      auto& c = *competition_;
      if (c.human_slot >= 0 &&
          c.human_slot < static_cast<int>(c.participants.size())) {
        c.participants[c.human_slot].match_score = match_score;
        c.participants[c.human_slot].time_ticks =
            static_cast<int>(c.human_design_ticks) / 30;
        int speed_bonus = match_score * (c.speed_level * 20) / 100;
        int final_score =
            std::clamp(match_score + speed_bonus -
                           (c.participants[c.human_slot].time_ticks / 3),
                       0, 200);
        c.participants[c.human_slot].final_score = final_score;
      }
      bake_.reset();
    }

    // If more human players remain in the session, chain to the next one
    // instead of finalizing. AI scores are simulated only after the last human.
    auto& c = *competition_;
    int next_slot = c.human_slot + 1;
    if (next_slot < static_cast<int>(c.participants.size()) &&
        c.participants[next_slot].is_human) {
      c.human_slot = next_slot;
      core::PlayerId next_pid = c.participants[next_slot].player_id;
      auto session = std::make_shared<CompetitionSession>(std::move(c));
      competition_.reset();
      state_.ui().replace_top<PizzaDesignState>(PizzaDesignState{
          .initial_mode = PizzaDesignMode::MonthlyChallenge,
          .competition_session = std::move(session),
          .competing_player_id = next_pid,
      });
      return;
    }

    finalize_competition_results();
    return;
  }

  if (!bake_) {
    Logger::warn("No bake session after judging!");
    change_mode(PizzaDesignMode::Recipes);
    return;
  }

  // Handle post-judging flow based on bake kind
  if (bake_->kind == BakeKind::NewUser) {
    // For annual competition: record score before naming
    if (competition_) {
      const auto& scores = bake_->working.scores();
      int total = 0;
      for (int s : scores) {
        total += s;
      }
      auto& c = *competition_;
      if (c.human_slot >= 0 &&
          c.human_slot < static_cast<int>(c.participants.size())) {
        c.participants[c.human_slot].match_score = total;
        c.participants[c.human_slot].final_score = total;
      }
    }

    // Show Info screen first, then ask for name
    change_mode(PizzaDesignMode::Info);

    constexpr int max_len = 18;
    context_.popup->create<TextInputPopup>(
        "name of recipe:", max_len,
        [this](std::string input) { this->finish_new_user(std::move(input)); });
  }
  else {
    // Commit recipe now that judging is done
    if (bake_->kind == BakeKind::InstantiateCanonical) {
      state_.active_player()->recipes().create_from_canonical(
          *bake_->canonical_id, std::move(bake_->working));
      change_active_key(*bake_->canonical_id);
    }
    else if (bake_->kind == BakeKind::RecreateExisting) {
      finish_recreate_existing();
    }
    change_mode(PizzaDesignMode::Recipes);
    bake_.reset();
  }
}

Player* PizzaDesign::competing_player() const {
  if (competing_player_id_) {
    return state_.player(*competing_player_id_);
  }
  return state_.active_player();
}

void PizzaDesign::begin_competition(std::shared_ptr<CompetitionSession> s) {
  if (!context_.audio->is_playing()) {
    context_.audio->play_music(audio::MusicId::BREAK_IN_MUSIC_3);
  }
  competition_ = std::move(*s);
  reveal_delay_remaining_ticks_ = competition_->delay_ticks;

  GameDate today = state_.time().current_date();
  for (auto& p : competition_->participants) {
    auto* participant = state_.player(p.player_id);
    if (participant) {
      participant->financials().spend_cash(today, Account::Others, p.bet);
    }
  }

  update_competition_status();
  change_mode(PizzaDesignMode::CompetitionReveal);
}

void PizzaDesign::begin_annual_competition(
    std::shared_ptr<CompetitionSession> s) {
  competition_ = std::move(*s);
  bake_ = core::food::BakeSession{};
  bake_->kind = BakeKind::NewUser;
  bake_->working = Pizza("annual competition pizza");
  bake_->working.update_stats(state_.customer_taste_profiles());
  design_mode_ = true;
  status_.show_pizza(PizzaCtx{
      .state = &state_,
      .wip_pizza = &bake_->working,
      .recipe = nullptr,
  });
  change_mode(PizzaDesignMode::AnnualCompetitionDesign);

  auto* player = competing_player();
  std::string first_name = player->character().first_name();
  context_.popup->create<AlertPopup>(
      compat::format("Right, {}! Let's see what you can do!", first_name),
      [this] { competition_design_ticks_ = 0; });
}

void PizzaDesign::update_competition_status() {
  if (!competition_) {
    return;
  }
  auto& c = *competition_;
  std::string ingredient_name;
  if (!c.reveal_done) {
    int idx = c.reveal_idx - 1;
    if (idx >= 0 &&
        idx < static_cast<int>(c.canonical_pizza->toppings().size())) {
      ingredient_name =
          c.canonical_pizza->toppings()[idx]->ingredient()->name();
    }
  }
  status_.show_competition(CompetitionCtx{
      .pizza_name = c.canonical_pizza->name(),
      .speed_level = c.speed_level,
      .current_ingredient = ingredient_name,
  });
}

void PizzaDesign::on_reveal_complete() {
  if (!competition_) {
    return;
  }

  // Set up empty bake session and switch to design mode BEFORE showing the
  // popup so the background already shows an empty pizza canvas.
  bake_ = core::food::BakeSession{};
  bake_->kind = BakeKind::NewUser;
  bake_->working = Pizza("competition pizza");
  bake_->working.update_stats(state_.customer_taste_profiles());
  design_mode_ = true;
  status_.show_pizza(PizzaCtx{
      .state = &state_,
      .wip_pizza = &bake_->working,
      .recipe = nullptr,
  });
  change_mode(PizzaDesignMode::CompetitionDesign);

  auto* player = competing_player();
  std::string first_name = player->character().first_name();
  context_.popup->create<AlertPopup>(
      compat::format("Right, {}! Let's see what you can do!", first_name),
      [this] { competition_design_ticks_ = 0; });
}

void PizzaDesign::on_competition_design_finished() {
  if (!competition_ || !bake_) {
    return;
  }
  state_.advance_time_blocks(5);
  bake_->working.set_scores_from_canonical(*competition_->canonical_pizza);
  auto imitation_scores = bake_->working.scores();
  bake_->working.finalize(state_.customer_taste_profiles(),
                          "competition pizza");

  auto bake_ptr = std::make_shared<core::food::BakeSession>(std::move(*bake_));
  bake_.reset();
  competition_->human_design_ticks = competition_design_ticks_;
  auto comp_ptr =
      std::make_shared<CompetitionSession>(std::move(*competition_));
  competition_.reset();
  auto done = std::move(on_done_);

  state_.ui().push<PizzaJudgesState>(
      imitation_scores,
      [&state = state_, bake_ptr, comp_ptr, done = std::move(done)]() mutable {
        state.ui().pop();
        state.ui().replace_top<PizzaDesignState>(PizzaDesignState{
            .initial_mode = PizzaDesignMode::CompetitionJudging,
            .bake = bake_ptr,
            .competition_session = comp_ptr,
            .on_done = std::move(done),
        });
      },
      JudgingMode::Competition);
}

void PizzaDesign::finalize_competition_results() {
  if (!competition_) {
    return;
  }
  auto& c = *competition_;

  // Simulate AI participants
  for (auto& p : c.participants) {
    if (p.is_human) {
      continue;
    }
    const auto* ai = state_.player(p.player_id);
    if (!ai) {
      continue;
    }
    const auto& ch = ai->character();
    auto dex = ch.dexterity();
    auto cook = ch.cooking();
    auto comp_count = state_.competition_count();
    auto raw_time = 50 - engine::random::rand_max_n((dex / 2) + 1);
    auto time_ticks = raw_time + (raw_time * comp_count * 5 / 100);
    auto difficulty = c.pizza_index - 10 + (c.speed_level * 5);
    int q = 100 - difficulty + engine::random::rand_max_n(14) +
            (static_cast<int>(cook) / 5);
    int match_score = std::clamp(q, 0, 99);
    int speed_bonus = match_score * (c.speed_level * 20) / 100;
    int final_score =
        std::clamp(match_score + speed_bonus - (time_ticks / 3), 0, 200);
    p.match_score = match_score;
    p.time_ticks = time_ticks;
    p.final_score = final_score;
  }

  // Determine winner
  int best = -1;
  c.winner_slot = -1;
  c.all_tied = true;
  for (int i = 0; i < static_cast<int>(c.participants.size()); ++i) {
    int score = c.participants[i].final_score;
    if (score > best) {
      best = score;
      c.winner_slot = i;
    }
    if (i > 0 &&
        c.participants[i].final_score != c.participants[i - 1].final_score) {
      c.all_tied = false;
    }
  }
  if (c.participants.size() < 2) {
    c.all_tied = false;
  }

  // Distribute prize money and update counters
  // All bets were already deducted upfront in begin_competition().
  GameDate today = state_.time().current_date();
  if (c.winner_slot >= 0 && !c.all_tied) {
    auto& winner = c.participants[c.winner_slot];
    auto* winner_player = state_.player(winner.player_id);

    // Winner receives the full pot (all bets, including their own).
    Money total_bets;
    for (auto& p : c.participants) {
      total_bets += p.bet;
    }

    if (winner_player) {
      winner_player->financials().transfer(today, Account::Sales, Account::Cash,
                                           total_bets,
                                           AccountingScope::Operational);
      winner_player->add_score(40);
    }

    // Win counter
    if (winner_player) {
      winner_player->increment_competition_win_count();
      uint8_t wins = winner_player->competition_win_count();

      // Events for human winner: deferred until the results screen is
      // dismissed, don't want to spoil the results :)
      if (winner.is_human) {
        if (wins == 1) {
          pending_competition_message_ =
              core::events::LeventId::FirstPriceInPizzaCompetition;
        }
        else if (wins == 10) {
          pending_competition_message_ =
              core::events::LeventId::Won10PizzaCompetitions;
        }
      }
    }

    // Challenge mode: an AI that loses gets a cool down before wanting to be
    // challenged again
    if (c.is_challenge && c.human_slot >= 0 && c.winner_slot == c.human_slot) {
      for (auto& p : c.participants) {
        auto* ai = state_.player(p.player_id);
        if (ai && !ai->is_human_controlled()) {
          uint8_t courage = ai->character().courage();
          uint8_t penalty =
              static_cast<uint8_t>(engine::random::rand_max_n(100 - courage));
          ai->set_competition_willingness(penalty);
        }
      }
    }
  }
  else {
    // Draw: refund all bets
    for (auto& p : c.participants) {
      auto* participant = state_.player(p.player_id);
      if (participant) {
        participant->financials().transfer(today, Account::Others,
                                           Account::Cash, p.bet,
                                           AccountingScope::Operational);
      }
    }
  }

  state_.increment_competition_count();

  c.results_rows_shown = 0;
  c.results_last_row_ms = context_.clock.now_ms;
  if (auto* rest = state_.active_restaurant()) {
    status_.show_restaurant(
        {.state = &state_, .rest = rest, .label = "COMPETITION"});
  }
  change_mode(PizzaDesignMode::CompetitionResults);
}

void PizzaDesign::draw_competition_reveal(const RenderContext& ctx) const {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back1"));
  ctx.surface->blit_masked(chop_horizontal_ ? blade_horizontal_
                                            : blade_vertical_,
                           ViewPoint{CHOPPER_OFFSET_X, CHOPPER_OFFSET_Y});

  if (!competition_) {
    return;
  }
  const auto& toppings = competition_->canonical_pizza->toppings();
  int to_draw =
      std::min(competition_->reveal_idx, static_cast<int>(toppings.size()));
  for (int i = 0; i < to_draw; ++i) {
    renderer_.render(*toppings[i], *ctx.surface, camera_);
  }
}

void PizzaDesign::draw_competition_results(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back4"));
  if (!competition_) {
    return;
  }
  auto& c = *competition_;

  constexpr int text_color = 6;
  context_.font->render_text_onto(
      *ctx.surface, "PIZZA COMPETITION", ViewPoint{-20, 42}, FontFace::ASTANDAR,
      TextStyle{.color = text_color, .align = AlignX::Center});

  constexpr int col_name = 46;
  constexpr int col_time = 234;
  constexpr int col_match = 286;
  constexpr int col_final = 322;
  constexpr int row_height = 18;
  int y = 80;

  int rows_to_draw =
      std::min(c.results_rows_shown, static_cast<int>(c.participants.size()));

  for (int i = 0; i < rows_to_draw; ++i) {
    const auto& p = c.participants[i];
    auto* player_obj = state_.player(p.player_id);
    std::string name = player_obj ? player_obj->name() : "?";
    int m = p.time_ticks / 60;
    int s = p.time_ticks % 60;
    std::string time_str = compat::format("{}:{:02}", m, s);
    std::string match_str = compat::format("{} %", p.match_score);
    std::string final_str = std::to_string(p.final_score);

    context_.font->render_text_onto(
        *ctx.surface, name, ViewPoint{col_name, y}, FontFace::PIZ_NUMB,
        TextStyle{.color = text_color, .character_spacing = 1});
    context_.font->render_text_onto(*ctx.surface, time_str,
                                    ViewPoint{col_time, y}, FontFace::PIZ_NUMB,
                                    text_color, AlignX::RightEdge);
    context_.font->render_text_onto(*ctx.surface, match_str,
                                    ViewPoint{col_match, y}, FontFace::PIZ_NUMB,
                                    text_color, AlignX::RightEdge);
    context_.font->render_text_onto(*ctx.surface, final_str,
                                    ViewPoint{col_final, y}, FontFace::PIZ_NUMB,
                                    text_color, AlignX::RightEdge);
    y += row_height;
  }

  // Once all participant rows are visible we show the winner/draw:
  if (c.results_rows_shown > static_cast<int>(c.participants.size())) {
    std::string winner_text;
    if (c.winner_slot < 0 || c.all_tied) {
      winner_text = "-- draw --";
    }
    else {
      auto* winner_player =
          state_.player(c.participants[c.winner_slot].player_id);
      std::string wname =
          winner_player ? winner_player->name() : "unknown player";
      winner_text = wname + " wins";
    }
    y = 285;
    context_.font->render_text_onto(*ctx.surface, winner_text,
                                    ViewPoint{col_name, y}, FontFace::ASTANDAR,
                                    text_color);
  }
}
