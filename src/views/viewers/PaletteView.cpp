/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/viewers/PaletteView.h"

#include <set>

#include "engine/input/InputManager.h"
#include "resources/FontManager.h"

PaletteView::PaletteView(GameContext& context, GameState& state)
    : View(context, state),
      listener_{context_.input->register_listener(*this)} {}

void PaletteView::render(const RenderContext& ctx) {
  ViewRect dot;
  dot.w = 25;
  dot.h = 25;
  ctx.surface->palette(PaletteHandle{palette_});

  // Define indices to highlight - e.g., water animation entries
  std::set<int> highlight_indices = {};

  constexpr int palette_square_count = 16;
  for (int row = 0; row < palette_square_count; ++row) {
    for (int col = 0; col < palette_square_count; ++col) {
      int index = (row * palette_square_count) + col;

      dot.x = col * dot.w;
      dot.y = row * dot.h;

      ctx.surface->fill_rect(dot, index);
      if (show_numbers_) {
        uint8_t color = 1;
        if (color == index) {
          color = 2;
        }
        context_.font->render_text_onto(*ctx.surface, std::to_string(index),
                                        {dot.x + 4, dot.y + 4}, FontFace::MINI,
                                        color);
      }
    }
  }
}

bool PaletteView::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }
  switch (ev.action) {
  case input::Action::CycleForward:
    if (++palette_ >= gfx::VGA_PALETTE_COUNT) {
      palette_ = 0;
    }
    Logger::info("Palette is {}", palette_);
    mark_dirty();
    break;
  case input::Action::CycleBackward:
    if (palette_ == 0) {
      palette_ = gfx::VGA_PALETTE_COUNT - 1;
    }
    else {
      --palette_;
    }
    Logger::info("Palette is {}", palette_);
    mark_dirty();
    break;
  case input::Action::ToggleExtraInfo:
    show_numbers_ = !show_numbers_;
    mark_dirty();
    Logger::info("Toggle numbers");
    break;
  default:
    return false;
  }
  return true;
}

void PaletteView::tick() {}
