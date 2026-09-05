/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/viewers/PaletteView.h"

#include <set>
#include <string>

#include "core/state/GameState.h"
#include "engine/DisplayConstants.h"
#include "engine/gfx/PaletteBank.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionEvent.h"
#include "engine/ui/InteractionLayers.h"
#include "resources/FontManager.h"

namespace {
// Geometry of the 16x16 swatch grid.
constexpr int swatch_size = 25;
constexpr int grid_squares = 16;
constexpr int grid_pixels = swatch_size * grid_squares;
} // namespace

PaletteView::PaletteView(GameContext& context, GameState& state)
    : View(context, state),
      listener_{context_.input->register_listener(*this)} {

  for (int index = 0; index < grid_squares * grid_squares; ++index) {
    int col = index % grid_squares;
    int row = index / grid_squares;
    ViewRect rect{col * swatch_size, row * swatch_size, swatch_size,
                  swatch_size};
    interactions_.create_persistent_item(
        rect, "s_" + std::to_string(index),
        [this, index](const views::ui::InteractionEvent& ev) {
          switch (ev.type) {
          case views::ui::InteractionEvent::Type::Enter:
          case views::ui::InteractionEvent::Type::Hover:
            if (hover_index_ != index) {
              hover_index_ = index;
              mark_dirty();
            }
            break;
          case views::ui::InteractionEvent::Type::Leave:
            // Only clear if we still own the hover; the next swatch's Enter may
            // have already claimed it.
            if (hover_index_ == index) {
              hover_index_.reset();
              mark_dirty();
            }
            break;
          case views::ui::InteractionEvent::Type::Click:
            if (ev.button == views::ui::MouseButton::Left) {
              click_index_ = index;
              mark_dirty();
            }
            break;
          default:
            break;
          }
        });
  }
}

void PaletteView::render(const RenderContext& ctx) {
  ViewRect dot;
  dot.w = swatch_size;
  dot.h = swatch_size;
  ctx.surface->palette(PaletteHandle{palette_});

  // Define indices to highlight - e.g., water animation entries
  std::set<int> highlight_indices = {};

  for (int row = 0; row < grid_squares; ++row) {
    for (int col = 0; col < grid_squares; ++col) {
      int index = (row * grid_squares) + col;

      dot.x = col * dot.w;
      dot.y = row * dot.h;

      ctx.surface->fill_rect(dot, static_cast<uint8_t>(index));
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

  render_rgb(ctx);
}

void PaletteView::render_rgb(const RenderContext& ctx) {
  const auto& colors =
      gfx::PaletteBank::instance().colors(PaletteHandle{palette_});

  constexpr uint8_t panel_bg = 240; // usually black

  // Text is drawn through the palette being viewed, so a fixed index is
  // illegible in some palettes. Pick the brightest entry for the text against
  // the dark panel (based on perceived brightness).
  auto luminance = [](const gfx::Color& c) {
    return (2 * c.r) + (3 * c.g) + c.b;
  };
  uint8_t fg = 240;
  int max_lum = -1;
  for (int i = 1; i < static_cast<int>(colors.size()); ++i) {
    if (i == panel_bg) {
      continue;
    }
    int lum = luminance(colors[i]);
    if (lum > max_lum) {
      max_lum = lum;
      fg = static_cast<uint8_t>(i);
    }
  }

  constexpr int panel_x = grid_pixels + 16;
  constexpr int panel_w = DisplayConstants::SCREEN_WIDTH - panel_x - 8;
  constexpr int line_h = 18;
  ctx.surface->fill_rect(ViewRect{panel_x - 8, 0, panel_w + 16, grid_pixels},
                         panel_bg);

  int y = 12;
  auto line = [&](const std::string& text) {
    context_.font->render_line_onto(*ctx.surface, text, {panel_x, y},
                                    FontFace::CSTANDAR, fg);
    y += line_h;
  };

  auto rgb_block = [&](const std::string& heading, std::optional<int> index) {
    if (!index.has_value()) {
      return;
    }
    const gfx::Color& c = colors[*index];
    line(heading + " #" + std::to_string(*index));
    line("R: " + std::to_string(c.r));
    line("G: " + std::to_string(c.g));
    line("B: " + std::to_string(c.b));
  };

  line("Palette " + std::to_string(palette_));
  y += line_h / 2;
  rgb_block("Hover", hover_index_);
  y += line_h / 2;
  rgb_block("Clicked", click_index_);
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
