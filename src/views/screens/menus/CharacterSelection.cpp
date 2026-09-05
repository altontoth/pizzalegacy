/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/CharacterSelection.h"

#include <algorithm>
#include <cassert>
#include <numeric>

#include "core/character/Character.h"
#include "core/factories/CharacterFactory.h"
#include "core/state/GameState.h"
#include "engine/Random.h"
#include "views/screens/menus/MenuTheme.h"
#include "views/ui/widgets/ArrowMenuItem.h"
#include "views/ui/widgets/CharacterPhotoBox.h"
#include "views/ui/widgets/CharacterSelectionBox.h"
#include "views/ui/widgets/CompanySelectionBox.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"
#include "views/ui/widgets/TextMenuItem.h"

using namespace views::ui::menu;

#define PHOTO_BOX_OFFSET_X 273
#define PHOTO_BOX_OFFSET_Y 125

#define CHAR_BOX_OFFSET_X 35
#define CHAR_BOX_OFFSET_Y 125
#define CHAR_BOX_OFFSET_W 224
#define CHAR_BOX_OFFSET_H 216
#define CHAR_BOX_ARROW_H 24

constexpr int MAX_HUMAN_PLAYERS = 8;

CharacterSelection::CharacterSelection(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{CHAR_BOX_OFFSET_X, CHAR_BOX_OFFSET_Y}, 0) {

  generate_player_items();
  reload_character();

  // Up arrow to pick next character
  add_menu_item(std::make_unique<ArrowMenuItem>(
      context, ArrowType::Up, [this] { change_character(+1); }));
  // Character stats
  auto char_box = std::make_unique<CharacterSelectionBox>(
      context, &character_, &active_player_, player_items_);
  char_box->on_scroll([this](int delta) {
    // Scroll up (positive) = next character, scroll down (negative) = previous
    // character
    change_character(delta > 0 ? +1 : -1);
  });
  add_menu_item(std::move(char_box));
  // Down arrow
  add_menu_item(std::make_unique<ArrowMenuItem>(
      context, ArrowType::Down, [this] { change_character(-1); }));
  // New column
  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 238, 0));
  // Photo box
  add_menu_item(std::make_unique<CharacterPhotoBox>(context, &character_));
  // Reposition for the start button
  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, -30, 142));
  // Start button
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Start",
      [this] {
        // Copy: the view stays alive (and may render) until the deferred
        // view swap happens.
        state_.initializer().players = player_items_;
        state_.ui().push<EuroMapState>(MapMode::GAME_INIT);
      },
      face, style, sel_style, item_align));
  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, -28, 240));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Back", [this] { state_.ui().pop(); }, FontFace::RUBY, style,
      sel_style, item_align));
  // Reposition for third column
  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 365, 0));
  // Up arrow for human selection:
  add_menu_item(std::make_unique<ArrowMenuItem>(
      context, ArrowType::Up,
      [this] {
        human_players_ = std::clamp(human_players_ - 1, 1, MAX_HUMAN_PLAYERS);
        player_items_.at(human_players_).is_human = false;
        mark_dirty();
      },
      "Decrease number of human players"));
  // Company selection box
  auto company_box =
      std::make_unique<CompanySelectionBox>(context, player_items_);
  company_box->on_company_clicked([this](int index) { active_player(index); });
  company_box->on_scroll([this](int delta) {
    // Up (positive delta) removes a human player, down adds one, matching
    // the arrow buttons above/below the box.
    if (delta > 0) {
      human_players_ = std::clamp(human_players_ - 1, 1, MAX_HUMAN_PLAYERS);
      player_items_.at(human_players_).is_human = false;
    }
    else {
      human_players_ = std::clamp(human_players_ + 1, 1, MAX_HUMAN_PLAYERS);
      player_items_.at(human_players_ - 1).is_human = true;
    }
    mark_dirty();
  });
  add_menu_item(std::move(company_box));
  // Down arrow
  add_menu_item(std::make_unique<ArrowMenuItem>(
      context, ArrowType::Down,
      [this] {
        human_players_ = std::clamp(human_players_ + 1, 1, MAX_HUMAN_PLAYERS);
        player_items_.at(human_players_ - 1).is_human = true;
        mark_dirty();
      },
      "Increase number of human players"));
}

void CharacterSelection::generate_player_items() {
  static std::vector<std::string> active_player_names;

  if (active_player_names.empty()) {
    // First-time initialization: Pick 8 random names
    std::vector<std::string> company_names = {
        "Pizza King",   "Pizza World", "Pizza Land",   "Fast Slime",
        "Mac Fat",      "Paradiso",    "KF Pizza",     "Pizza-U-Eat",
        "Pizza 2000",   "Kwik Pizza",  "Salmo Pizza",  "Geno-Pizza",
        "Panda Pizza",  "Piz Perfect", "Dino-Pizza",   "Thai Ching",
        "Pizza Parlor", "Ripley's",    "Rick's Diner", "SoftRock Pizza"};

    // Shuffle the names
    engine::random::shuffle(company_names.begin(), company_names.end());

    // Select the first 8 names
    active_player_names.assign(company_names.begin(),
                               company_names.begin() + MAX_HUMAN_PLAYERS);
  }

  std::vector<int> character_ids(NUMBER_OF_CHARACTERS);
  std::iota(character_ids.begin(), character_ids.end(), 0);
  if (state_.options().qol_improvements) {
    engine::random::shuffle(character_ids.begin(), character_ids.end());
  }

  // Generate the player items using the selected names
  player_items_.reserve(MAX_HUMAN_PLAYERS);
  for (int i = 0; i < MAX_HUMAN_PLAYERS; ++i) {
    auto* char_data = CharacterFactory::create_character(character_ids[i]);
    player_items_.push_back(init::PlayerSlot{
        .name = active_player_names[i],
        .is_selected = i == 0,
        .is_human = i < human_players_,
        .character_id = character_ids[i],
        .character_name = char_data->first_name + " " + char_data->last_name,
    });
  }
}

void CharacterSelection::reload_character() {
  auto character_id = player_items_[active_player_].character_id;
  character_ = CharacterFactory::create_character(character_id);
  player_items_[active_player_].character_name =
      character_->first_name + " " + character_->last_name;
  mark_dirty();
}

Surface CharacterSelection::background() const {
  auto background = default_background("Select Character");

  // TODO: Move the background into their respective boxes,
  // maybe with some composer boxes to combine e.g. the
  // character selection box and its arrows into one MenuItem

  // Black box for picture border:
  ViewRect photo_box(PHOTO_BOX_OFFSET_X - 2, PHOTO_BOX_OFFSET_Y - 2, 108, 122);
  background.fill_rect(photo_box, 8);

  // Gray shadow box:
  ViewRect char_box(CHAR_BOX_OFFSET_X - 2, CHAR_BOX_OFFSET_Y + 4,
                    CHAR_BOX_OFFSET_W,
                    CHAR_BOX_OFFSET_H + (CHAR_BOX_ARROW_H * 2) + 2);
  background.fill_rect(char_box, 14);

  ViewRect char_box2(CHAR_BOX_OFFSET_X, CHAR_BOX_OFFSET_Y,
                     CHAR_BOX_OFFSET_W + 2,
                     CHAR_BOX_OFFSET_H + (CHAR_BOX_ARROW_H * 2));
  background.fill_rect(char_box2, 13);

  return background;
}

void CharacterSelection::active_player(int index) {
  if (index < 0 || index >= (int)player_items_.size()) {
    return;
  }

  // Mark old one not selected
  player_items_[active_player_].is_selected = false;

  // Mark new one as selected
  active_player_ = index;
  player_items_[active_player_].is_selected = true;

  // Reload the character
  // Since each player has their own character_id, we can do:
  character_ = CharacterFactory::create_character(
      player_items_[active_player_].character_id);

  mark_dirty();
}

void CharacterSelection::change_character(int delta) {
  if (state_.options().qol_improvements) {
    selected_character_ = (selected_character_ + NUMBER_OF_CHARACTERS + delta) %
                          NUMBER_OF_CHARACTERS;
  }
  else {
    selected_character_ =
        std::clamp(selected_character_ + delta, 0, NUMBER_OF_CHARACTERS - 1);
  }
  character_for_player(selected_character_);
}

void CharacterSelection::character_for_player(int character_id) {
  player_items_[active_player_].character_id = character_id;
  reload_character();
}
