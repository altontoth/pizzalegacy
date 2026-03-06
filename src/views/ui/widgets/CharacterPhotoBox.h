/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/character/Character.h"
#include "engine/geom/Spaces.h"
#include "views/ui/widgets/MenuItem.h"

class CharacterPhotoBox : public MenuItem {
public:
  CharacterPhotoBox(GameContext& context,
                    core::character::CharacterData** character)
      : MenuItem(context), character_(character) {};
  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  Size measure() const override;

private:
  core::character::CharacterData** character_;
};
