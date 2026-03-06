/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <string_view>
#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/View.h"

class BobViewer : public View,
                  public input::InputListener,
                  public debug::AutoLabel<BobViewer> {
public:
  BobViewer(GameContext& context, GameState& state, std::string_view filename);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  uint8_t palette_;
  std::string filename_;
  int state_;
  Bob& bob_;
  input::ListenerToken listener_;
};
