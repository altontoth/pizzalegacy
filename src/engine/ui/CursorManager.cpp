/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "engine/ui/CursorManager.h"
#include "engine/DisplayConstants.h"
#include "engine/Renderer.h"
#include "gfx/Surface.h"

using DisplayConstants::SCREEN_HEIGHT;
using DisplayConstants::SCREEN_WIDTH;
using gfx::Surface;

namespace {
// The cursor was just kept in the executable as an array, so here it is:
constexpr uint8_t mouse_cursor_data[] = {
    0x11, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x12, 0x11, 0x13, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x12, 0x11, 0x11, 0x13, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x12, 0x11, 0x11, 0x11, 0x11, 0x13, 0x13, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x12, 0x11, 0x11, 0x11, 0x11, 0x11, 0x13, 0x13, 0x00,
    0x00, 0x00, 0x12, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x00, 0x00, 0x00, 0x12, 0x11, 0x11, 0x11, 0x13, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x12, 0x11, 0x11, 0x12, 0x11, 0x13, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x12, 0x11, 0x00, 0x12, 0x11, 0x13, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x12, 0x11, 0x00, 0x00, 0x12, 0x11, 0x13,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x11,
};

constexpr int CURSOR_WIDTH = 11;
constexpr int CURSOR_HEIGHT = 11;

gfx::Surface create_cursor_surface() {
  return Surface::from_indexed(mouse_cursor_data, CURSOR_WIDTH, CURSOR_HEIGHT,
                               gfx::PaletteHandle{0});
}
} // namespace

CursorManager::CursorManager(VgaManager* /*vga*/, int /*display_height*/)
    : cursor_surface_(create_cursor_surface()) {}

void CursorManager::base_cursor_mode(CursorMode mode) {
  base_mode_ = mode;

  // Update cursor palette based on mode
  // Menu mode uses palette 38 (red), Game mode uses palette 0 (yellow)
  auto palette = (mode == CursorMode::MENU) ? gfx::PaletteHandle{38}
                                            : gfx::PaletteHandle{0};
  cursor_surface_.palette(palette);

  if (!custom_cursor_active_) {
    rebuild_base_cursor();
  }
}

CursorMode CursorManager::base_cursor_mode() const { return base_mode_; }

void CursorManager::custom_cursor(const Surface& surface, bool centered) {
  custom_cursor_surface_.emplace(surface.clone());
  custom_cursor_active_ = true;
  custom_cursor_centered_ = centered;
  // Rebuild with current scale factor if needed
  apply_cursor(*custom_cursor_surface_, centered);
}

void CursorManager::clear_custom_cursor() {
  if (!custom_cursor_active_) {
    return;
  }

  custom_cursor_active_ = false;
  custom_cursor_surface_.reset();
  scaled_custom_surface_.reset();

  if (!cursor_hidden_) {
    rebuild_base_cursor();
  }
}

void CursorManager::hide_cursor() {
  cursor_hidden_ = true;
  SDL_ShowCursor(SDL_DISABLE);
}

void CursorManager::show_cursor() {
  cursor_hidden_ = false;
  SDL_ShowCursor(SDL_ENABLE);

  if (active_cursor_) {
    SDL_SetCursor(active_cursor_.get());
  }
}

bool CursorManager::is_custom_cursor_active() const {
  return custom_cursor_active_;
}

bool CursorManager::is_cursor_hidden() const { return cursor_hidden_; }

void CursorManager::update_scaled_cursor(int new_window_width,
                                         int new_window_height) {
  // The cursor needs to match the content's aspect ratio:
  // - Content is rendered at 640x400, displayed at window_width x window_height
  // - In CRT mode, window is 640x480, giving 1.2x vertical stretch
  // - Scaling against LOGICAL_HEIGHT (400) gives us this stretch
  float scale_x = static_cast<float>(new_window_width) / SCREEN_WIDTH;
  float scale_y =
      static_cast<float>(new_window_height) / DisplayConstants::LOGICAL_HEIGHT;

  // If scale hasn't changed meaningfully, do nothing.
  if (std::fabs(scale_x - last_scale_x_) < 0.01f &&
      std::fabs(scale_y - last_scale_y_) < 0.01f) {
    return;
  }

  last_scale_x_ = scale_x;
  last_scale_y_ = scale_y;

  if (!cursor_hidden_) {
    if (custom_cursor_active_ && custom_cursor_surface_) {
      apply_cursor(*custom_cursor_surface_, custom_cursor_centered_);
    }
    else {
      rebuild_base_cursor();
    }
  }
}

void CursorManager::free_active_cursor() {
  if (active_cursor_) {
    // SDL keeps using the currently set cursor internally, so detach it
    // before destroying the object to avoid SDL referencing freed memory.
    SDL_SetCursor(nullptr);
    // unique_ptr deleter calls SDL_FreeCursor().
    active_cursor_.reset();
  }
}

void CursorManager::rebuild_base_cursor() {
  free_active_cursor();
  apply_cursor(cursor_surface_, /*centered=*/false);
}

void CursorManager::update_palette(gfx::PaletteHandle palette) {
  cursor_surface_.palette(palette);
  if (!custom_cursor_active_ && !cursor_hidden_) {
    rebuild_base_cursor();
  }
}

void CursorManager::apply_cursor(const Surface& source_surface, bool centered) {
  free_active_cursor();

  float scale_x = last_scale_x_;
  float scale_y = last_scale_y_;

  SDL_Surface_unique_ptr sdl_src = Renderer::to_sdl_surface(source_surface);
  // Check if we need to scale
  if (std::fabs(scale_x - 1.f) > 0.01f || std::fabs(scale_y - 1.f) > 0.01f) {
    int scaled_w = int(sdl_src->w * scale_x);
    int scaled_h = int(sdl_src->h * scale_y);
    SDL_Surface_unique_ptr sdl_scaled{SDL_CreateRGBSurfaceWithFormat(
        0, scaled_w, scaled_h, sdl_src->format->BitsPerPixel,
        sdl_src->format->format)};
    SDL_BlitScaled(sdl_src.get(), nullptr, sdl_scaled.get(), nullptr);

    int hot_x = centered ? scaled_w / 2 : 0;
    int hot_y = centered ? scaled_h / 2 : 0;
    active_cursor_.reset(SDL_CreateColorCursor(sdl_scaled.get(), hot_x, hot_y));
    scaled_custom_surface_.emplace(source_surface.clone());
  }
  else {
    // no scaling
    int hot_x = centered ? sdl_src->w / 2 : 0;
    int hot_y = centered ? sdl_src->h / 2 : 0;
    active_cursor_.reset(SDL_CreateColorCursor(sdl_src.get(), hot_x, hot_y));
    scaled_custom_surface_.reset();
  }

  if (active_cursor_) {
    SDL_SetCursor(active_cursor_.get());
    SDL_ShowCursor(SDL_ENABLE);
    cursor_hidden_ = false;
  }
}
