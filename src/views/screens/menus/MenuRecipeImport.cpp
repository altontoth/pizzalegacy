/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuRecipeImport.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include "core/character/Player.h"
#include "core/factories/PizzaFactory.h"
#include "core/food/PlayerRecipes.h"
#include "core/food/RecipeCodec.h"
#include "core/food/Topping.h"
#include "core/food/ToppingSize.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"
#include "resources/BobManager.h"
#include "resources/FontTypes.h"
#include "views/screens/menus/MenuTheme.h"
#include "views/screens/menus/RecipePreview.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"
#include "views/ui/widgets/ArrowMenuItem.h"
#include "views/ui/widgets/CheckboxMenuItem.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"
#include "views/ui/widgets/RecipeFileInfoBox.h"
#include "views/ui/widgets/SaveFileListBox.h"
#include "views/ui/widgets/TextMenuItem.h"

using namespace views::ui::menu;

namespace {
constexpr int box_x = 35;
constexpr int box_y = 125;
constexpr int box_w = 224;
constexpr int box_h = 216;
constexpr int arrow_h = 24;
constexpr int info_column_x = 230;
constexpr int preview_margin = 14;
constexpr int info_text_left = (preview_margin * 2) + RecipePreview::size;
} // namespace

MenuRecipeImport::MenuRecipeImport(GameContext& context, GameState& state,
                                   RecipeImportState s)
    : Menu(context, state, ViewPoint{box_x, box_y}, 0), player_(s.player),
      preview_(std::make_unique<RecipePreview>(context, state)) {
  files_ = io::recipe::list_recipe_files();
  for (const auto& entry : files_) {
    if (!PizzaFactory::read_summary(
            io::recipe::recipe_path(entry.filename).string())) {
      unreadable_.insert(entry.filename);
    }
  }

  add_menu_item(std::make_unique<ArrowMenuItem>(context, ArrowType::Up, [this] {
    if (scroll_offset_ > 0) {
      scroll_offset_--;
      mark_dirty();
    }
  }));

  auto list = std::make_unique<SaveFileListBox>(
      context, files_, &scroll_offset_,
      [this](const std::string& filename) { click_file(filename); },
      [this](const std::string& /*filename*/) {
        if (!select_several_) {
          import_marked();
        }
      },
      io::recipe::recipe_extension);
  list->mark_rows([this](const std::string& filename) {
    return marked_.contains(filename);
  });
  list->disable_rows([this](const std::string& filename) {
    return unreadable_.contains(filename);
  });
  const auto* list_box = list.get();
  add_menu_item(std::move(list));

  add_menu_item(std::make_unique<ArrowMenuItem>(
      context, ArrowType::Down, [this, list_box] {
        if (scroll_offset_ < list_box->max_scroll()) {
          scroll_offset_++;
          mark_dirty();
        }
      }));

  add_menu_item(
      std::make_unique<PositionMarkerMenuItem>(context, info_column_x, 0));
  auto info = std::make_unique<RecipeFileInfoBox>(context, info_text_left);
  info_box_ = info.get();
  add_menu_item(std::move(info));

  const auto first = std::ranges::find_if(files_, [this](const auto& entry) {
    return !unreadable_.contains(entry.filename);
  });
  if (first != files_.end()) {
    click_file(first->filename);
  }

  constexpr TextStyle button_style{.color = 1, .align = TextAnchor::Start};
  constexpr TextStyle sel_button_style{.color = 29, .align = TextAnchor::Start};

  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 260, 160));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Import", [this] { import_marked(); }, face, button_style,
      sel_button_style));

  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 244, 230));
  add_menu_item(std::make_unique<CheckboxMenuItem>(
      context, "Select several", select_several_, [this] {
        select_several_ = !select_several_;
        if (!select_several_) {
          marked_.clear();
          marked_.insert(selected_);
        }
        mark_dirty();
      }));

  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 510, 244));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Back", [this] { state_.ui().pop(); }, FontFace::RUBY, style,
      sel_style));
}

MenuRecipeImport::~MenuRecipeImport() = default;

std::vector<LayerInfo> MenuRecipeImport::get_layers() {
  auto layers = Menu::get_layers();
  layers.push_back(LayerInfo{
      .view = preview_.get(),
      .area = {box_x + info_column_x + preview_margin, box_y + preview_margin,
               RecipePreview::size, RecipePreview::size},
      .label = "Recipe preview",
  });
  return layers;
}

void MenuRecipeImport::click_file(const std::string& filename) {
  if (!select_several_) {
    marked_.clear();
    marked_.insert(filename);
  }
  else if (!marked_.erase(filename)) {
    marked_.insert(filename);
  }
  selected_ = filename;
  info_box_->set_file(filename);
  preview_->show_file(io::recipe::recipe_path(filename));
  mark_dirty();
}

void MenuRecipeImport::import_marked() {
  if (marked_.empty()) {
    return;
  }
  auto* player = state_.player(player_);
  if (!player) {
    state_.ui().pop();
    return;
  }
  auto& recipes = player->recipes();

  const auto bob_size = core::food::topping_size_fn(*context_.bob);

  int imported = 0;
  int listed = 0;
  int unreadable = 0;
  for (const auto& entry : files_) {
    const std::string& filename = entry.filename;
    if (!marked_.contains(filename)) {
      continue;
    }
    const auto path = io::recipe::recipe_path(filename);
    switch (PizzaFactory::import_recipe(path.string(), recipes, bob_size,
                                        state_.customer_taste_profiles())) {
    case PizzaFactory::ImportResult::Imported:
      ++imported;
      break;
    case PizzaFactory::ImportResult::AlreadyListed:
      ++listed;
      break;
    case PizzaFactory::ImportResult::Unreadable:
      ++unreadable;
      break;
    }
  }

  if (listed == 0 && unreadable == 0) {
    state_.ui().pop();
    return;
  }
  std::string message =
      imported == 0 ? "Nothing imported."
                    : compat::format("Imported {} {}.", imported,
                                     imported == 1 ? "recipe" : "recipes");
  if (listed > 0) {
    message += compat::format("|{} {} a name already in your list.", listed,
                              listed == 1 ? "has" : "have");
  }
  if (unreadable > 0) {
    message += compat::format("|{} could not be read.", unreadable);
  }
  if (imported == 0) {
    context_.popup->origin(ViewPoint{26, 40});
    context_.popup->create<AlertPopup>(message);
    return;
  }
  context_.popup->create<AlertPopup>(message);
  state_.ui().pop();
}

Surface MenuRecipeImport::background() const {
  auto background = default_background("Import recipes");

  ViewRect shadow_rect(box_x - 2, box_y + 4, box_w, box_h + (arrow_h * 2) + 2);
  background.fill_rect(shadow_rect, 14);
  ViewRect box_rect(box_x, box_y, box_w + 2, box_h + (arrow_h * 2));
  background.fill_rect(box_rect, 13);

  return background;
}
