/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <vector>
#include "engine/DebugConsole.h"
#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "engine/input/TextSession.h"
#include "views/base/OverlayBehavior.h"
#include "views/base/View.h"

class ConsoleLayer : public View,
                     public OverlayBehavior<ConsoleLayer>,
                     public input::InputListener,
                     public debug::AutoLabel<ConsoleLayer> {
public:
  ConsoleLayer(GameContext&, GameState&, DebugConsole&);

  void render(const RenderContext& ctx) override;
  void tick() override;
  bool needs_force_recompose() const override { return pull_recompose_flag(); }
  bool freezes_view() const override;

private:
  // input helpers
  void commit_input();
  void navigate_history(int delta);
  void scroll_page(int delta);
  void scroll_line(int delta);
  bool on_text(const input::TextEvent& ev) override;
  bool on_action(const input::ActionEvent& ev) override;
  bool has_older_content() const;

  DebugConsole& console_;
  std::string input_;
  std::vector<std::string> cmd_history_;
  // If size == 0 => blank line
  std::size_t cmd_hist_index_ = 0;
  // First visible line index
  std::size_t scroll_ = 0;
  std::optional<input::TextSession> text_session_;
  input::ListenerToken listener_;

  static constexpr std::size_t MAX_INPUT_BYTES = 1024;
  static constexpr int console_height_ = 200;
  static constexpr int font_line_height_ = 6;
};
