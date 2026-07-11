/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/character/Character.h"
#include "core/init/NewGameConfig.h"
#include "engine/DebugLabel.h"
#include "engine/geom/Spaces.h"
#include "views/ui/widgets/MenuItem.h"

class CharacterSelectionBox : public MenuItem,
                              public input::InputListener,
                              public debug::AutoLabel<CharacterSelectionBox> {
public:
  CharacterSelectionBox(GameContext& gc,
                        core::character::CharacterData** character,
                        int* active_player,
                        std::vector<init::PlayerSlot>& players)
      : MenuItem(gc), character_(character), active_player_(active_player),
        players_(players) {};
  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  Size measure() const override;
  bool handles_keyboard_events() override { return keyboard_enabled_; };
  bool handles_mouse_events() override { return true; };
  bool on_adjust(AdjustAxis axis, int delta) override;
  bool adjust_repeats() const override { return true; }
  bool on_interaction(views::ui::InteractionEvent ie) override;
  void begin_keyboard_session(input::InputManager* input,
                              std::function<void()> inv) override;

  bool on_text(const input::TextEvent& ev) override;
  bool on_action(const input::ActionEvent& ev) override;

  void on_scroll(std::function<void(int delta)> callback) {
    on_scroll_ = std::move(callback);
  }

private:
  void mark_dirty();
  void start_rename();
  void end_rename();

  core::character::CharacterData** character_;
  int* active_player_;
  std::vector<init::PlayerSlot>& players_;
  bool keyboard_enabled_ = false;
  std::string new_name_;
  std::function<void()> invalidate_;
  std::optional<input::TextSession> text_session_;
  bool rename_active_{false};
  // Wheel-direction scroll handler: positive delta is up.
  std::function<void(int delta)> on_scroll_;
};
