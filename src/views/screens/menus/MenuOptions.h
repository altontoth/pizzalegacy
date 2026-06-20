/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include "core/state/UIState.h"
#include "views/base/Menu.h"

class MenuOptions : public Menu {
public:
  // Main choice screen
  MenuOptions(GameContext& context, GameState& state);
  // Settings sub-screen
  MenuOptions(GameContext& context, GameState& state, OptionsScreen screen);

protected:
  Surface background() const override;

private:
  void build_chooser(GameState& state);
  void build_audio(GameState& state);
  void build_graphics(GameState& state);
  void build_controls(GameState& state);
  void build_gameplay(GameState& state);
  void add_save_and_back(GameState& state);

  std::string title_;
};
