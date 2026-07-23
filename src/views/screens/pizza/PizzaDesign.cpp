/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Lifecycle, mode switching and the input panel. The other concerns of this
// screen live in the sibling PizzaDesign*.cpp translation units.

#include "views/screens/pizza/PizzaDesign.h"

#include "core/GameTypes.h"
#include "core/finance/MoneyParse.h"
#include "core/food/RecipeCodec.h"
#include "core/state/GameState.h"
#include "engine/Logger.h"
#include "engine/PlatformPaths.h"
#include "engine/Random.h"
#include "engine/audio/AudioManager.h"
#include "engine/geom/Spaces.h"
#include "engine/input/InputManager.h"
#include "engine/ui/ICursorManager.h"
#include "gfx/PaletteBank.h"
#include "resources/BobSizeFn.h"
#include "resources/util/FileUtils.h"
#include "views/base/ViewDefs.h"
#include "views/screens/pizza/PizzaDesignHelpers.h"
#include "views/screens/restaurant/RecipeListRow.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/popups/NumberInputPopup.h"
#include "views/ui/popups/TextInputPopup.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>

using namespace core::food;
using namespace pizza_design;
using core::finance::parse::from_input;
using io::recipe::save_prez;

namespace {
// build an override that copies 0xC0..0xDF from src palette (e.g. 13)
gfx::PaletteOverride make_bar_ui_ramp_override(uint8_t src_pal_id) {
  std::vector<uint8_t> r = {101, 103, 104, 108, 109, 111};
  return gfx::PaletteOverride{
      .dst_indices = r, .src_indices = r, .src_palette_id = src_pal_id};
}
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

  if (tick_competition()) {
    return;
  }

  tick_chop();
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

  // Tab: cycle active restaurant (only in browsing modes, not during baking or
  // competition)
  if (ev.action == input::Action::NextRestaurant ||
      ev.action == input::Action::PreviousRestaurant) {
    const bool in_competition =
        mode_ == PizzaDesignMode::CompetitionDesign ||
        mode_ == PizzaDesignMode::CompetitionReveal ||
        mode_ == PizzaDesignMode::CompetitionResults ||
        mode_ == PizzaDesignMode::CompetitionJudging ||
        mode_ == PizzaDesignMode::OwnPizzaJudging ||
        mode_ == PizzaDesignMode::CanonicalPizzaJudging ||
        mode_ == PizzaDesignMode::MonthlyChallenge ||
        mode_ == PizzaDesignMode::CompetitorChallenge ||
        mode_ == PizzaDesignMode::AnnualCompetitionDesign;
    if (!in_competition && !bake_) {
      auto* player = state_.active_player();
      auto* rest = state_.active_restaurant();
      if (player && rest) {
        const auto cur = rest->id();
        std::optional<core::RestaurantId> next =
            ev.action == input::Action::NextRestaurant
                ? state_.next_restaurant(player->id(), cur)
                : state_.prev_restaurant(player->id(), cur);
        if (next && *next != cur) {
          state_.active_restaurant(*next);
          if (!bake_) {
            status_.show_restaurant({.state = &state_,
                                     .rest = state_.active_restaurant(),
                                     .label = "RECIPES"});
          }
          mark_dirty();
        }
      }
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
                                     std::to_string(number) + ".pc");
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
    auto rows = build_recipe_list_rows(
        recipes,
        state_.restaurants_in_city(player->id(), state_.active_city_id()));
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
                      auto* restaurant = state_.active_restaurant();
                      restaurant->menu().add_or_set(id, *price);
                      if (const auto* recipe =
                              state_.active_player()->recipes().find(id)) {
                        restaurant->seed_menu_ingredient_demand(*recipe);
                      }
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
