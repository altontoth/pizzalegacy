/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "engine/ui/CursorManager.h"

#include <algorithm>
#include <cmath>

#include "engine/DisplayConstants.h"
#include "engine/Renderer.h"
#include "gfx/Surface.h"

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

CursorManager::CursorManager(VgaManager* /*vga*/, int display_height)
    : cursor_surface_(create_cursor_surface()),
      display_height_(display_height) {}

void CursorManager::base_cursor_mode(UiMode mode) {
  base_mode_ = mode;

  // Update cursor palette based on mode
  // Menu mode uses palette 38 (red), Game mode uses palette 0 (yellow)
  auto palette =
      (mode == UiMode::Menu) ? gfx::PaletteHandle{38} : gfx::PaletteHandle{0};
  cursor_surface_.palette(palette);

  if (!custom_cursor_showing()) {
    sync_hardware_cursor();
  }
}

UiMode CursorManager::base_cursor_mode() const { return base_mode_; }

void CursorManager::custom_cursor(const Surface& surface, bool centered) {
  custom_cursor_surface_.emplace(surface.clone());
  custom_cursor_active_ = true;
  custom_cursor_centered_ = centered;
  custom_cursor_suppressed_ = false;
  // Installing a custom cursor makes it visible.
  cursor_hidden_ = false;
  sync_hardware_cursor();
}

void CursorManager::clear_custom_cursor() {
  if (!custom_cursor_active_) {
    return;
  }

  custom_cursor_active_ = false;
  custom_cursor_suppressed_ = false;
  custom_cursor_surface_.reset();
  scaled_custom_surface_.reset();

  sync_hardware_cursor();
}

void CursorManager::suppress_custom_cursor(bool suppress) {
  if (custom_cursor_suppressed_ == suppress) {
    return;
  }
  custom_cursor_suppressed_ = suppress;
  sync_hardware_cursor();
}

bool CursorManager::custom_cursor_showing() const {
  return custom_cursor_active_ && !custom_cursor_suppressed_;
}

void CursorManager::hide_cursor() {
  cursor_hidden_ = true;
  sync_hardware_cursor();
}

void CursorManager::show_cursor() {
  cursor_hidden_ = false;
  sync_hardware_cursor();
}

bool CursorManager::is_custom_cursor_active() const {
  return custom_cursor_active_;
}

bool CursorManager::is_cursor_hidden() const { return cursor_hidden_; }

void CursorManager::display_height(int display_height) {
  display_height_ = display_height;
}

void CursorManager::update_scaled_cursor(int new_window_width,
                                         int new_window_height) {
  // SDL gives us the window width and height, including any black bars SDL
  // might have added to preserve the aspect ratio. The cursor must match the
  // scale of the actual game inside the black bars, so we need to compensate
  // for this.
  //
  // CRT mode applies the additional vertical stretch (640x400 -> 640x480) so we
  // apply that factor on top at the end.
  float content_scale =
      std::min(static_cast<float>(new_window_width) / SCREEN_WIDTH,
               static_cast<float>(new_window_height) /
                   static_cast<float>(display_height_));
  float scale_x = content_scale;
  float scale_y = content_scale * (static_cast<float>(display_height_) /
                                   DisplayConstants::LOGICAL_HEIGHT);

  // If scale hasn't changed meaningfully, do nothing.
  if (std::abs(scale_x - last_scale_x_) < 0.01f &&
      std::abs(scale_y - last_scale_y_) < 0.01f) {
    return;
  }

  last_scale_x_ = scale_x;
  last_scale_y_ = scale_y;

  sync_hardware_cursor();
}

void CursorManager::hw_free_cursor() {
  if (active_cursor_) {
    // SDL keeps using the currently set cursor internally, so detach it
    // before destroying the object to avoid SDL referencing freed memory.
    SDL_SetCursor(nullptr);
    // unique_ptr deleter calls SDL_DestroyCursor().
    active_cursor_.reset();
  }
}

void CursorManager::sync_hardware_cursor() {
  if (software_mode_) {
    // the renderer draws the cursor
    return;
  }
  if (cursor_hidden_) {
    SDL_HideCursor();
    return;
  }
  if (custom_cursor_showing()) {
    hw_apply_cursor(*custom_cursor_surface_, custom_cursor_centered_);
  }
  else {
    hw_rebuild_base_cursor();
  }
}

void CursorManager::hw_rebuild_base_cursor() {
  hw_free_cursor();
  // only custom cursors can be centered:
  hw_apply_cursor(cursor_surface_, false);
}

void CursorManager::update_palette(gfx::PaletteHandle palette) {
  cursor_surface_.palette(palette);
  if (!custom_cursor_showing()) {
    sync_hardware_cursor();
  }
}

void CursorManager::hw_apply_cursor(const Surface& source_surface,
                                    bool centered) {
  hw_free_cursor();

  float scale_x = last_scale_x_;
  float scale_y = last_scale_y_;

  SDL_Surface_unique_ptr sdl_src = Renderer::to_sdl_surface(source_surface);
  // Check if we need to scale
  if (std::abs(scale_x - 1.f) > 0.01f || std::abs(scale_y - 1.f) > 0.01f) {
    int scaled_w = int(sdl_src->w * scale_x);
    int scaled_h = int(sdl_src->h * scale_y);
    SDL_Surface_unique_ptr sdl_scaled{
        SDL_CreateSurface(scaled_w, scaled_h, sdl_src->format)};
    SDL_BlitSurfaceScaled(sdl_src.get(), nullptr, sdl_scaled.get(), nullptr,
                          SDL_SCALEMODE_NEAREST);

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
    SDL_ShowCursor();
  }
}

void CursorManager::software_mode(bool enable) {
  if (software_mode_ == enable) {
    return;
  }
  software_mode_ = enable;

  if (enable) {
    // Our own renderer takes over, drop any installed SDL cursor
    // and hide the OS pointer.
    hw_free_cursor();
    SDL_HideCursor();
  }
  else {
    // Go back to the hardware cursor and rebuild whatever cursor should
    // currently be showing.
    sync_hardware_cursor();
  }
}

const gfx::Surface* CursorManager::software_cursor_sprite() const {
  if (!software_mode_ || cursor_hidden_) {
    return nullptr;
  }
  if (relative_mouse_mode_) {
    return nullptr;
  }
  if (custom_cursor_showing()) {
    return &*custom_cursor_surface_;
  }
  return &cursor_surface_;
}

bool CursorManager::software_cursor_centered() const {
  return custom_cursor_showing() ? custom_cursor_centered_ : false;
}

void CursorManager::set_relative_mouse_mode(bool enable) {
  relative_mouse_mode_ = enable;
  if (enable) {
    // Store cursor position before locking.
    SDL_GetGlobalMouseState(&relative_mode_origin_x_, &relative_mode_origin_y_);
    SDL_SetWindowRelativeMouseMode(window_, true);
  }
  else {
    // Restore the cursor to its pre-drag position when leaving relative mode.
    SDL_SetWindowRelativeMouseMode(window_, false);
    SDL_WarpMouseGlobal(relative_mode_origin_x_, relative_mode_origin_y_);
  }
}
