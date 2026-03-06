/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/viewers/FontViewer.h"
#include "resources/FontManager.h"

FontViewer::FontViewer(GameContext& context, GameState& state)
    : View(context, state), listener_(context.input->register_listener(*this)) {
  fonts_ = {
      FontFace::MENU,     FontFace::ASTANDAR, FontFace::CSTANDAR,
      FontFace::PIZ_NUMB, FontFace::RUBY,     FontFace::SUPERSMA,
      FontFace::MINI,
  };
  current_position_ = 0;
}

bool FontViewer::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }
  switch (ev.action) {
  case input::Action::CycleForward:
    if (++current_position_ >= static_cast<int>(fonts_.size())) {
      current_position_ = 0;
    }
    mark_dirty();
    break;
  case input::Action::CycleBackward:
    if (--current_position_ < 0) {
      current_position_ = static_cast<int>(fonts_.size()) - 1;
    }
    mark_dirty();
    break;

  default:
    return false;
  }
  return true;
}

void FontViewer::tick() {}

void FontViewer::render(const RenderContext& ctx) {
  ctx.surface->fill(254);
  constexpr int text_color = 1;

  auto* f = context_.font;

  FontFace current = FontManager::font_list().at(current_position_);
  std::string font_line = "Font: " + std::string(to_string(current));
  f->render_text_onto(*ctx.surface, font_line, ViewPoint{4, 4},
                      FontFace::ASTANDAR, text_color);
  int y = 44;
  auto metrics = f->render_text_onto(
      *ctx.surface,
      "!@#$%^&*()-_[]{}|0123456789|abcdefghijklmnopqrstuvwxyz|"
      "ABCDEFGHIJKLMNOPQRSTUVWX|YZ and backslash is \\ and u is ü",
      ViewPoint{4, y}, current, BLACK);
  y += metrics.height;
  metrics = f->render_text_onto(*ctx.surface, "The quick brown dog \\",
                                ViewPoint{4, y}, current, text_color);
  y += metrics.height;
  metrics = f->render_text_onto(*ctx.surface, "Jumped over something somewhere",
                                ViewPoint{4, y}, current, text_color);

  y += metrics.height + 20;
  f->render_text_onto(*ctx.surface, "Test with spaces 123 :)", ViewPoint{4, y},
                      current, text_color);
}
