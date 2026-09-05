/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/viewers/BobViewer.h"

#include <cstddef>
#include <cstdlib>
#include "core/util/Tools.h"
#include "engine/gfx/PaletteBank.h"
#include "engine/input/InputManager.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/TextInputPopup.h"

BobViewer::BobViewer(GameContext& context, GameState& state,
                     std::string_view filename)
    : View(context, state), palette_(0), filename_(filename), state_(0),
      bob_(context_.bob->get(filename_)),
      listener_(context.input->register_listener(*this)) {}

bool BobViewer::on_action(const input::ActionEvent& ev) {
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
  case input::Action::CycleDown:
    if (++state_ >= static_cast<int>(bob_.frame_count())) {
      state_ = 0;
    }
    Logger::debug("state_ = {}", state_);
    mark_dirty();
    break;
  case input::Action::CycleUp:
    if (--state_ < 0) {
      state_ = static_cast<int>(bob_.frame_count() - 1);
    }
    Logger::debug("state_ = {}", state_);
    mark_dirty();
    break;
  case input::Action::ToggleExtraInfo:
    context_.popup->create<TextInputPopup>(
        "Which bob frame nr?", 4, [this](const std::string& input) {
          auto frame_nr = Tools::string_to_int(input);
          auto frame_count = static_cast<int>(bob_.frame_count());
          if (frame_nr > frame_count) {
            frame_nr = frame_count - 1;
          }
          state_ = frame_nr;
          mark_dirty();
        });
    break;
  default:
    return false;
  }
  return true;
}

void BobViewer::tick() {}

void BobViewer::render(const RenderContext& ctx) {
  ctx.surface->fill(240);
  auto bob_surface = bob_.frame(state_, PaletteHandle{palette_});
  ctx.surface->palette(PaletteHandle{palette_});
  ctx.surface->blit_masked(bob_surface);
}
