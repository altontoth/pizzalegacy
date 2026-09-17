/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <string>
#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "resources/Bob.h"
#include "resources/FontTypes.h"
#include "views/base/View.h"

// Text card for trailers
class CardViewer final : public View,
                         public input::InputListener,
                         public debug::AutoLabel<CardViewer> {
public:
  CardViewer(GameContext& context, GameState& state, std::string text);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  std::string text_;
  const Bob& stanbobs_;
  FontFace face_{FontFace::MENU};
  uint8_t background_{160};
  input::ListenerToken listener_;
};
