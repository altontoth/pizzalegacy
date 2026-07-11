/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <vector>

#include "core/init/NewGameConfig.h"
#include "engine/DebugLabel.h"
#include "engine/geom/Spaces.h"
#include "engine/input/InputListener.h"
#include "views/ui/widgets/MenuItem.h"

class CompanySelectionBox : public MenuItem,
                            public input::InputListener,
                            public debug::AutoLabel<CompanySelectionBox> {
public:
  CompanySelectionBox(GameContext& context,
                      std::vector<init::PlayerSlot>& players)
      : MenuItem(context), players_(players) {};
  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  bool handles_keyboard_events() override { return keyboard_enabled_; };
  bool handles_mouse_events() override { return true; };
  void on_company_clicked(std::function<void(int playerIndex)>);
  Size measure() const override;
  bool on_adjust(AdjustAxis axis, int delta) override;
  bool on_interaction(views::ui::InteractionEvent ie) override;
  void begin_keyboard_session(input::InputManager* input,
                              std::function<void()> inv) override;

  bool on_text(const input::TextEvent& ev) override;
  bool on_action(const input::ActionEvent& ev) override;

  void on_scroll(std::function<void(int delta)> callback) {
    on_scroll_ = std::move(callback);
  }

private:
  void rename_company(int company_id);
  void end_rename();
  void mark_dirty();

  std::vector<init::PlayerSlot>& players_;
  bool keyboard_enabled_ = false;
  std::optional<int> company_being_renamed_;
  int selected_company_ = 0;
  // std::string name_backup_;
  std::function<void(int playerIndex)> on_company_clicked_;
  std::optional<input::TextSession> text_session_; // RAII
  std::string new_name_;
  std::function<void()> invalidate_;
  // Wheel-direction scroll handler: positive delta is up.
  std::function<void(int delta)> on_scroll_;
};
