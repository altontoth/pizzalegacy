/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <string>

#include "engine/input/Action.h"
#include "views/ui/widgets/MenuItem.h"

// An action's name and the keys bound to it.
class BindingMenuItem : public MenuItem {
public:
  using RowCallback = std::function<void(BindingMenuItem&)>;

  BindingMenuItem(GameContext& context, std::string label, input::Action action,
                  RowCallback on_choose);

  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override { on_choose_(*this); }
  Size measure() const override;

  input::Action action() const { return action_; }
  void waiting(bool waiting) { waiting_ = waiting; }

private:
  std::string value_text() const;

  input::Action action_;
  RowCallback on_choose_;
  bool waiting_ = false;
};
