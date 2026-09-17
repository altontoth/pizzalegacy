/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/viewers/CardViewer.h"

#include <vector>
#include "engine/DisplayConstants.h"
#include "engine/Logger.h"
#include "engine/gfx/Surface.h"
#include "engine/input/InputManager.h"
#include "engine/ui/TextAnchor.h"
#include "resources/FontManager.h"
#include "views/ui/PageDecoration.h"

CardViewer::CardViewer(GameContext& context, GameState& state, std::string text)
    : View(context, state), text_(std::move(text)),
      stanbobs_(context_.bob->get("bob/stanbobs.bob")),
      listener_(context.input->register_listener(*this)) {}

bool CardViewer::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }
  const std::vector<FontFace> faces = FontManager::font_list();
  int face_index = 0;
  for (int i = 0; i < static_cast<int>(faces.size()); ++i) {
    if (faces[i] == face_) {
      face_index = i;
    }
  }
  switch (ev.action) {
  case input::Action::CycleForward:
    face_index++;
    if (face_index >= static_cast<int>(faces.size())) {
      face_index = 0;
    }
    face_ = faces[face_index];
    Logger::info("Font: {}", to_string(face_));
    break;
  case input::Action::CycleBackward:
    face_index--;
    if (face_index < 0) {
      face_index = static_cast<int>(faces.size()) - 1;
    }
    face_ = faces[face_index];
    Logger::info("Font: {}", to_string(face_));
    break;
  case input::Action::CycleUp:
    if (background_ < 255) {
      background_++;
    }
    Logger::info("Background: {}", background_);
    break;
  case input::Action::CycleDown:
    if (background_ > 0) {
      background_--;
    }
    Logger::info("Background: {}", background_);
    break;
  default:
    return false;
  }
  mark_dirty();
  return true;
}

void CardViewer::tick() {}

void CardViewer::render(const RenderContext& ctx) {
  constexpr int screen_w = DisplayConstants::SCREEN_WIDTH;
  constexpr int screen_h = DisplayConstants::SCREEN_HEIGHT;
  constexpr int edge_offset = 12;
  constexpr int corner_offset = 8;
  constexpr gfx::ColorIndex line_color = 11;

  ctx.surface->palette(PaletteHandle{68});
  ctx.surface->fill(background_);
  views::ui::render_decorated_corners(
      *ctx.surface, stanbobs_,
      ViewRect{{edge_offset, edge_offset},
               {screen_w - (edge_offset * 2), screen_h - (edge_offset * 2)}},
      corner_offset, line_color);

  const TextStyle ts{.color = 240,
                     .align = TextAnchor::Middle,
                     .shadow = ShadowStyle{.offset = {-2, 2}, .color = 13},
                     .line_spacing = 8};
  const int text_h = context_.font->size_text(text_, face_, ts).height;
  context_.font->render_text_onto(
      *ctx.surface, text_, ViewPoint{screen_w / 2, (screen_h - text_h) / 2},
      face_, ts);
}
