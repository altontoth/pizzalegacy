/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/View.h"
#include "views/helpers/camera/CameraController.h"

class RawViewer : public View,
                  public input::InputListener,
                  public debug::AutoLabel<RawViewer> {
public:
  RawViewer(GameContext& context, GameState& state, std::string filename,
            std::optional<int> width = std::nullopt,
            std::optional<int> height = std::nullopt);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  void handle_left_mouse_click(ViewPoint p);
  uint8_t palette_{0};
  std::string filename_;
  Surface image_;
  bool show_blitmap_overlay_{true};
  Camera camera_;
  CameraController camctrl_;
  input::ListenerToken listener_;
};
