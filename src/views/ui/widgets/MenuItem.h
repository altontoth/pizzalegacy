/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <utility>

#include "core/util/GameContext.h"
#include "engine/Size.h"
#include "engine/geom/Spaces.h"
#include "engine/input/InputManager.h"
#include "engine/ui/Alignment.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/FontTypes.h"

class MenuItem {
public:
  MenuItem(GameContext& gc) : context_(gc), font_face_(FontFace::MENU) {};
  MenuItem(GameContext& gc, FontFace font_face)
      : context_(gc), font_face_(font_face) {};
  MenuItem(GameContext& gc, FontFace font_face, std::string text,
           AlignX align = AlignX::Left)
      : context_(gc), font_face_(font_face), text_(std::move(text)),
        alignment_(align) {};
  virtual ~MenuItem() = default;

  virtual void draw(Surface& screen, ViewPoint p, bool is_selected) = 0;
  virtual void handle_action() = 0;
  virtual Size measure() const = 0;
  int x() const { return x_pos_; }
  FontFace font_face() const { return font_face_; }
  AlignX alignment() const { return alignment_; }

  // For specific cases we have some input handling
  virtual bool handles_keyboard_events() { return false; };
  virtual bool handles_mouse_events() { return false; };
  virtual bool on_interaction(views::ui::InteractionEvent ie) { return false; };
  virtual std::string tooltip_text() const { return ""; }
  virtual void begin_keyboard_session(input::InputManager*,
                                      std::function<void()>) {};

protected:
  GameContext& context_;
  int width_ = 0;
  int height_ = 0;
  int x_pos_ = -1;
  FontFace font_face_;
  std::string text_;
  AlignX alignment_{AlignX::Left};
};
