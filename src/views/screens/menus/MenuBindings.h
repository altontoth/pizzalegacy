/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <vector>

#include "engine/input/InputManager.h"
#include "views/base/Menu.h"

class BindingMenuItem;

class MenuBindings : public Menu {
public:
  MenuBindings(GameContext& context, GameState& state);
  ~MenuBindings() override;

  bool on_action(const input::ActionEvent& ev) override;

protected:
  Surface background() const override;

private:
  void choose(BindingMenuItem& row);
  void on_captured(std::optional<input::CapturedInput> captured);
  void clear(BindingMenuItem& row);
  void restore_defaults();
  void stop_waiting();
  void save();

  std::vector<BindingMenuItem*> rows_;
  // The row waiting for a key, if any.
  BindingMenuItem* waiting_ = nullptr;
};
