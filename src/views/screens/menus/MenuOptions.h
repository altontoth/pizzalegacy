/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <string>
#include "core/state/UIState.h"
#include "views/base/Menu.h"

class MenuOptions : public Menu {
public:
  // Main choice screen
  MenuOptions(GameContext& context, GameState& state);
  // Settings sub-screen
  MenuOptions(GameContext& context, GameState& state, OptionsScreen screen,
              bool focus_advanced_toggle = false);

protected:
  Surface background() const override;

private:
  void build_chooser(GameState& state);
  void build_audio(GameState& state);
  void build_graphics(GameState& state);
  void build_controls(GameState& state);
  void build_gameplay(GameState& state);
  // Returns the advanced options checkbox, if the screen has one.
  MenuItem* add_save_and_back(GameState& state, OptionsScreen screen);
  void add_back(std::function<void()> action);

  std::string title_;
};
