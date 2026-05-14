/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/character/Character.h"
#include "core/init/NewGameConfig.h"
#include "views/base/Menu.h"

#include <vector>

class CharacterSelection : public Menu {
public:
  CharacterSelection(GameContext& context, GameState& state);

protected:
  Surface background() const override;

private:
  Surface& character_stats() const;
  void reload_character();
  void generate_player_items();
  void active_player(int index);
  void character_for_player(int character_id);
  void change_character(int delta);

  int active_player_{0};
  int human_players_{1};
  int selected_character_{0};
  core::character::CharacterData* character_;
  std::vector<init::PlayerSlot> player_items_;
};
