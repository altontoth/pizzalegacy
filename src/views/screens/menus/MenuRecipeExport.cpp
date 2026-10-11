/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuRecipeExport.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include "core/character/Player.h"
#include "core/food/Recipe.h"
#include "core/food/RecipeCodec.h"
#include "core/food/Topping.h"
#include "core/food/ToppingSize.h"
#include "core/state/GameState.h"
#include "engine/Logger.h"
#include "resources/BobManager.h"
#include "resources/FontTypes.h"
#include "views/screens/menus/MenuTheme.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/widgets/ArrowMenuItem.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"
#include "views/ui/widgets/SaveFileInputBox.h"
#include "views/ui/widgets/TextMenuItem.h"

using namespace views::ui::menu;

namespace {
constexpr int box_x = 35;
constexpr int box_y = 125;
constexpr int box_w = 224;
constexpr int box_h = 216;
constexpr int arrow_h = 24;
} // namespace

MenuRecipeExport::MenuRecipeExport(GameContext& context, GameState& state,
                                   RecipeExportState s)
    : Menu(context, state, ViewPoint{box_x, box_y}, 0), player_(s.player),
      recipe_(s.recipe) {
  files_ = io::recipe::list_recipe_files();

  add_menu_item(std::make_unique<ArrowMenuItem>(context, ArrowType::Up, [this] {
    if (scroll_offset_ > 0) {
      scroll_offset_--;
      mark_dirty();
    }
  }));

  auto box = std::make_unique<SaveFileInputBox>(
      context, files_, &scroll_offset_,
      [this](const std::string& filename) { save_to_file(filename); },
      io::recipe::recipe_extension);
  if (const auto* r = recipe()) {
    box->start_new_name_input(
        io::recipe::unused_recipe_stem(r->display_name(), files_));
  }
  const auto* input_box = box.get();
  add_menu_item(std::move(box));

  add_menu_item(std::make_unique<ArrowMenuItem>(
      context, ArrowType::Down, [this, input_box] {
        if (scroll_offset_ < input_box->max_scroll()) {
          scroll_offset_++;
          mark_dirty();
        }
      }));

  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 234, 248));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Back", [this] { state_.ui().pop(); }, FontFace::RUBY, style,
      sel_style, item_align));
}

const core::food::Recipe* MenuRecipeExport::recipe() const {
  const auto* player = state_.player(player_);
  return player ? player->recipes().find(recipe_) : nullptr;
}

void MenuRecipeExport::save_to_file(const std::string& filename) {
  const bool exists =
      std::ranges::any_of(files_, [&](const fileutils::FileEntry& f) {
        return f.filename == filename;
      });
  if (exists) {
    context_.popup->origin(ViewPoint{26, 40});
    context_.popup->create<ConfirmPopup>("Overwrite old file?",
                                         [this, filename](bool confirmed) {
                                           if (confirmed) {
                                             write_recipe(filename);
                                           }
                                         });
  }
  else {
    write_recipe(filename);
  }
}

void MenuRecipeExport::write_recipe(const std::string& filename) {
  const auto* r = recipe();
  if (!r) {
    state_.ui().pop();
    return;
  }
  const auto path = io::recipe::recipe_path(filename);
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);

  const auto bob_size = core::food::topping_size_fn(*context_.bob);
  if (io::recipe::save_prez(r->pizza(), path, bob_size, r->display_name())) {
    Logger::info("Exported recipe {} to {}", r->display_name(), path.string());
    state_.ui().pop();
  }
  else {
    context_.popup->origin(ViewPoint{26, 40});
    context_.popup->create<AlertPopup>("Failed to write the recipe file.");
  }
}

Surface MenuRecipeExport::background() const {
  auto background = default_background("Export recipe");

  ViewRect shadow_rect(box_x - 2, box_y + 4, box_w, box_h + (arrow_h * 2) + 2);
  background.fill_rect(shadow_rect, 14);
  ViewRect box_rect(box_x, box_y, box_w + 2, box_h + (arrow_h * 2));
  background.fill_rect(box_rect, 13);

  return background;
}
