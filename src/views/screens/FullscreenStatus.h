/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>

#include "core/events/MilestoneId.h"
#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/View.h"
#include "views/helpers/TypewriterTextRenderer.h"

class FullscreenStatus : public View,
                         public input::InputListener,
                         public debug::AutoLabel<FullscreenStatus> {
public:
  FullscreenStatus(GameContext& context, GameState& state,
                   core::events::MilestoneId msg_id,
                   std::function<void()> on_dismiss = nullptr);

  ~FullscreenStatus() override = default;

  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  void init_from_message_id(core::events::MilestoneId msg_id);
  void dismiss();

  TypewriterTextRenderer typewriter_;
  std::string text_;
  std::optional<core::events::MilestoneId> message_id_;
  input::ListenerToken listener_;
  std::function<void()> on_dismiss_;
  const Surface& background_;
  uint8_t fg_color_{3};
  uint8_t shadow_color_{20};
  bool override_palette_{false};
};
