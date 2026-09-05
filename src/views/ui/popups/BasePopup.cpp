/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/BasePopup.h"
#include "engine/gfx/Draw.h"
#include "engine/input/InputManager.h"
#include "views/ui/popups/PopupButton.h"

BasePopup::BasePopup(const GameContext& context)
    : font_(context.font), vga_(context.vga), bob_(context.bob) {
  listener_token_ = context.input->register_listener(
      *this, input::InputManager::ListenerPriority::High);
}

void BasePopup::add_shortcut(input::Action action, PopupButton* button) {
  shortcuts_.emplace_back(action, button);
}

bool BasePopup::on_action(const input::ActionEvent& ev) {
  for (const auto& [action, button] : shortcuts_) {
    if (ev.action != action) {
      continue;
    }
    // Fire on the press edge and swallow both edges.
    if (ev.state.pressed && !is_finished()) {
      button->trigger();
    }
    return true;
  }
  return false;
}

Surface BasePopup::text_button(const std::string& text, int width, int height,
                               bool mouse_down) {
  auto surface = button_surface(width, height, mouse_down);
  if (text.empty()) {
    return surface;
  }

  auto metrics = font_->size_text(text, FontFace::ASTANDAR);

  ViewPoint origin{(width - metrics.width) / 2, (height - metrics.height) / 2};

  constexpr int button_color_idx = 9;
  font_->render_text_onto(surface, text, origin, FontFace::ASTANDAR,
                          button_color_idx);
  return surface;
}

gfx::Surface BasePopup::button_surface(int width, int height, bool mouse_down) {
  gfx::Surface s{width, height, gfx::PaletteHandle{0}};
  const uint8_t fill_idx = 14;

  uint8_t light = 15;
  uint8_t dark = 10;
  if (mouse_down) {
    std::swap(light, dark);
  }

  ViewRect outer{0, 0, width, height};
  s.fill_rect(outer, fill_idx);

  // Outer bevel
  gfx::draw::beveled_box(s, outer, light, dark, 4);

  return s;
}

gfx::Surface BasePopup::popup_surface(int width, int height) {
  gfx::Surface s{width, height, gfx::PaletteHandle{0}};

  const uint8_t fillIdx = 14;
  const uint8_t b1 = 11;
  const uint8_t b2 = 13;
  const uint8_t b3 = 15;
  const uint8_t b4 = 14;
  const uint8_t b5 = 13;

  // We subtract two because we need some extra room for the border to go around
  // it
  ViewRect outer{0, 0, width - 1, height - 1};
  // Background color first:
  s.fill_rect(outer, fillIdx);

  // Then all the insets that give a beveled look:
  gfx::draw::box(s, outer.inset(0, 0), b1, 2);
  gfx::draw::box(s, outer.inset(2, 2), b2, 2);
  gfx::draw::box(s, outer.inset(4, 4), b3, 2);
  gfx::draw::box(s, outer.inset(6, 6), b4, 2);
  gfx::draw::box(s, outer.inset(8, 8), b5, 2);

  return s;
}
