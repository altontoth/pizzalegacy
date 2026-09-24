/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// The recipe list view and loading recipes from disk.

#include "core/factories/PizzaFactory.h"
#include "core/state/GameState.h"
#include "engine/Logger.h"
#include "engine/PlatformPaths.h"
#include "engine/StringUtil.h"
#include "engine/gfx/Blit.h"
#include "engine/ui/TextAnchor.h"
#include "resources/BobSizeFn.h"
#include "resources/util/FileUtils.h"
#include "views/screens/pizza/PizzaDesign.h"
#include "views/screens/pizza/PizzaDesignHelpers.h"
#include "views/screens/restaurant/RecipeListRow.h"
#include "views/ui/PopupManager.h"
#include "views/ui/components/BeveledButton.h"
#include "views/ui/popups/AlertPopup.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/popups/ContextMenuPopup.h"
#include "views/ui/popups/TextInputPopup.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace core::food;
using pizza_design::is_selected;

void PizzaDesign::draw_recipes(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back4"));

  const TextStyle ts{.color = 6,
                     .align = TextAnchor::Middle,
                     .shadow =
                         ShadowStyle{.offset = PxOffset{0, 1}, .color = 20}};

  const ViewPoint title_pos{(ctx.surface->width() / 2) - 20, 42};

  context_.font->render_text_onto(*ctx.surface, "recipe list", title_pos,
                                  FontFace::ASTANDAR, ts);

  const TextStyle pg_ts{.color = 6, .align = TextAnchor::Start};
  const ViewPoint page_pos{276, 42};

  context_.font->render_text_onto(*ctx.surface,
                                  "page " + std::to_string(page_ + 1), page_pos,
                                  FontFace::CSTANDAR, pg_ts);

  ViewRect name_entry_box{80, 70, 200, 18};
  ViewRect count_entry_box{290, 70, 26, 18};

  auto* player = competing_player();
  const auto& recipes = player->recipes();
  auto rows = build_recipe_list_rows(
      recipes,
      state_.restaurants_in_city(player->id(), state_.active_city_id()));

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
          if (ie.type != views::ui::InteractionEvent::Type::Click) {
            return;
          }
          if (row.recipe_id) {
            change_active_key(RecipeKey{*row.recipe_id});
          }
          else if (row.reference) {
            change_active_key(RecipeKey{*row.reference});
          }
          else {
            change_active_key(new_row_tag);
          }
          mark_dirty();
          if (ie.button == views::ui::MouseButton::Right) {
            open_recipe_menu(row, ie.pos);
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

void PizzaDesign::open_recipe_menu(const RecipeListRow& row, ViewPoint at) {
  std::vector<ContextMenuItem> items;
  if (row.recipe_id) {
    items.push_back({"Show recipe", [this] {
                       change_mode(PizzaDesignMode::View);
                       mark_dirty();
                     }});
  }
  if (row.reference && state_.options().cook_book) {
    const auto cp = *row.reference;
    items.push_back({"Show cook book", [this, cp] {
                       show_reference_pizza(cp);
                       change_mode(PizzaDesignMode::View);
                       mark_dirty();
                     }});
  }
  if (row.recipe_id && !row.reference) {
    const auto id = *row.recipe_id;
    items.push_back({"Export to disk...", [this] { export_recipe(); }});
    items.push_back({"Rename...", [this, id] { rename_recipe(id); }});
  }
  if (!row.recipe_id && !row.reference) {
    items.push_back({"Import from disk...", [this] { import_recipe(); }});
  }
  if (items.empty()) {
    return;
  }
  context_.popup->anchor(at);
  context_.popup->create<ContextMenuPopup>(std::move(items));
}

void PizzaDesign::rename_recipe(core::RecipeId id) {
  const auto* recipe = competing_player()->recipes().find(id);
  if (!recipe) {
    return;
  }
  context_.popup->create<TextInputPopup>(
      "new name of recipe:", max_recipe_name_length,
      [this, id](const std::string& input) {
        const std::string name{util::trim(input)};
        if (name.empty()) {
          return;
        }
        auto& recipes = competing_player()->recipes();
        const bool taken = std::ranges::any_of(
            recipes.all(), [&](const core::food::Recipe* r) {
              return r->id() != id && r->display_name() == name;
            });
        if (taken) {
          context_.popup->create<AlertPopup>(
              "You already have a recipe with that name!");
          return;
        }
        if (auto* r = recipes.find_mut(id)) {
          r->rename(name);
        }
        mark_dirty();
      },
      std::string(recipe->display_name()));
}

void PizzaDesign::load_standard_recipe(int number) {
  // Create the pizza from disk (from r/ directory)
  std::string filename = "r/r" + std::to_string(number) + ".pc";

  // Check if file exists first
  if (!fileutils::fexists(filename)) {
    context_.popup->create<ConfirmPopup>(
        "File '" + filename + "' not|found! Try again?",
        [this, number](bool retry) {
          if (retry) {
            load_standard_recipe(number); // Retry with same number
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

  auto* player = competing_player();
  auto& recipes = player->recipes();

  // If number is 0-13, treat as standard pizza
  if (number < 14 &&
      std::holds_alternative<core::food::StandardPizza>(*active_key_)) {
    auto cid = std::get<core::food::StandardPizza>(*active_key_);
    auto id = recipes.create_standard(cid, std::move(*pizza));
    Logger::info("Loaded standard recipe from {} into slot {}", filename,
                 id.value);
  }
  else {
    // Competition recipes (14+) are treated as user recipes
    std::string name = pizza->name();
    auto id = recipes.create_user(std::move(name), std::move(*pizza));
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
  std::string filename =
      platform_paths::recipes_dir() + "r" + std::to_string(number) + ".pc";
  load_recipe_for_debug(filename);
}

void PizzaDesign::load_standard_for_debug(int number) {
  std::string filename = "r/r" + std::to_string(number) + ".pc";
  load_recipe_for_debug(filename);
}
