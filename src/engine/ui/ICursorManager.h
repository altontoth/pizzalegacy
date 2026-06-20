/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include "gfx/PaletteHandle.h"
#include "resources/VgaManager.h"

namespace gfx {
class Surface;
}

// The original just used a couple of spot in the palette for the cursor,
// so the palette of the screen decided the color of the cursor. Because we are
// capable of mixing palettes in different layers, we don't have one true
// palette, so we can't use that. Not without some other sacrifices. So for now,
// just set the game mode to game or menu and we'll pick the right palette for
// the cursor based on that.
enum class CursorMode { GAME, MENU };

struct ICursorManager {
  virtual ~ICursorManager() = default;

  virtual void base_cursor_mode(CursorMode) = 0;
  virtual CursorMode base_cursor_mode() const = 0;

  virtual void custom_cursor(const gfx::Surface& surface,
                             bool centered = true) = 0;
  virtual void clear_custom_cursor() = 0;

  virtual void hide_cursor() = 0;
  virtual void show_cursor() = 0;

  virtual bool is_custom_cursor_active() const = 0;
  virtual bool is_cursor_hidden() const = 0;

  virtual void update_scaled_cursor(int new_window_width,
                                    int new_window_height) = 0;
  virtual void update_palette(gfx::PaletteHandle palette) = 0;

  virtual void set_relative_mouse_mode(bool enable) = 0;
};

// Factory is declared here but implemented in the SDL-enabled engine.
std::unique_ptr<ICursorManager> make_cursor_manager(VgaManager& vga);
