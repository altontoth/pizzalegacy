/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string_view>

#include "engine/DebugLabel.h"
#include "engine/GameLocationPrompt.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/Surface.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "resources/FontTypes.h"
#include "views/base/View.h"

// Asks the player where the original game is. Since we can't render text
// before we know where the original game is, we use a VGA file prerendered by
// tools/gen_game_location_screen.
class GameLocationScreen final : public View,
                                 public input::InputListener,
                                 public debug::AutoLabel<GameLocationScreen> {
public:
  GameLocationScreen(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  enum class Choice { Browse, Exit };

  void create_button(ViewRect rect, std::string_view id, Choice choice);
  void choose(Choice choice);
  void draw_hover(const RenderContext& ctx);
  void draw_button(const RenderContext& ctx, ViewRect rect,
                   std::string_view label);
  void draw_centered(const RenderContext& ctx, std::string_view text,
                     FontFace face, int center_x, int y);

  engine::FolderDialog dialog_;
  const gfx::Surface* picture_ = nullptr;
  std::optional<ViewRect> hovered_;
  input::ListenerToken listener_;
};
