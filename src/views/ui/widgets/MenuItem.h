/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "core/util/GameContext.h"
#include "engine/Geometry.h"
#include "engine/Size.h"
#include "engine/geom/Spaces.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/FontTypes.h"

// Direction of an arrow-key adjustment on a focused item.
enum class AdjustAxis { Horizontal, Vertical };

class MenuItem {
public:
  MenuItem(GameContext& gc) : context_(gc), font_face_(FontFace::MENU) {};
  MenuItem(GameContext& gc, FontFace font_face)
      : context_(gc), font_face_(font_face) {};
  MenuItem(GameContext& gc, FontFace font_face, std::string text,
           HAlign align = HAlign::Left)
      : context_(gc), font_face_(font_face), text_(std::move(text)),
        alignment_(align) {};
  virtual ~MenuItem() = default;

  virtual void draw(Surface& screen, ViewPoint p, bool is_selected) = 0;
  virtual void handle_action() = 0;
  virtual Size measure() const = 0;
  int x() const { return x_pos_; }
  FontFace font_face() const { return font_face_; }
  HAlign alignment() const { return alignment_; }

  // Whether keyboard navigation can land on this item. Decorative and
  // layout-only items return false and are skipped when stepping focus.
  virtual bool focusable() const { return true; }
  // Whether the item reacts to the mouse at all. Plain informative items
  // return false and get no hover or click handling.
  virtual bool interactive() const { return true; }
  // Arrow-key adjustment while focused; delta is -1 (left/up) or +1
  // (right/down). Return true if consumed; an unconsumed vertical step
  // falls back to menu focus movement.
  virtual bool on_adjust(AdjustAxis /*axis*/, int /*delta*/) { return false; }
  // Whether holding Left/Right keeps calling on_adjust after a short delay.
  virtual bool adjust_repeats() const { return false; }

  // For specific cases we have some input handling
  virtual bool handles_keyboard_events() { return false; };
  virtual bool handles_mouse_events() { return false; };
  virtual bool on_interaction(views::ui::InteractionEvent ie) { return false; };
  // Item opts into pointer dragging by naming the button that starts a drag.
  virtual std::optional<views::ui::MouseButton> drag_button() const {
    return std::nullopt;
  }
  virtual std::string tooltip_text() const { return ""; }
  const std::string& help_text() const { return help_text_; }
  void help_text(std::string text) { help_text_ = std::move(text); }
  // Advanced items are drawn in gray while not selected.
  bool advanced() const { return advanced_; }
  MenuItem& advanced(bool advanced) {
    advanced_ = advanced;
    return *this;
  }
  virtual void begin_keyboard_session(input::InputManager*,
                                      std::function<void()>) {};

protected:
  // Text color while not selected.
  uint8_t idle_color() const { return advanced_ ? 246 : 240; }

  GameContext& context_;
  int width_ = 0;
  int height_ = 0;
  int x_pos_ = -1;
  FontFace font_face_;
  std::string text_;
  std::string help_text_;
  HAlign alignment_{HAlign::Left};
  bool advanced_ = false;
};
