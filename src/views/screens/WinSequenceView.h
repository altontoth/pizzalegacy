/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/View.h"
#include "views/helpers/TypewriterTextRenderer.h"

class WinSequenceView : public View,
                        public input::InputListener,
                        public debug::AutoLabel<WinSequenceView> {
public:
  WinSequenceView(GameContext& context, GameState& state, int line_index,
                  core::PlayerId player_id);

  ~WinSequenceView() override = default;

  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  void dismiss();

  TypewriterTextRenderer typewriter_;
  int line_index_;
  core::PlayerId player_id_;
  int total_lines_;
  input::ListenerToken listener_;
  const Surface& background_;
};
