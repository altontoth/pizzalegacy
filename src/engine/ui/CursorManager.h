/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/DisplayConstants.h"
#include "engine/SDL_include.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/ICursorManager.h"
#include "resources/VgaManager.h"

class CursorManager : public ICursorManager {
public:
  CursorManager(VgaManager* vga,
                int display_height = DisplayConstants::LOGICAL_HEIGHT);

  void base_cursor_mode(UiMode mode) override;
  UiMode base_cursor_mode() const override;
  void custom_cursor(const Surface& surface, bool centered = true) override;
  void clear_custom_cursor() override;
  void suppress_custom_cursor(bool suppress) override;
  void hide_cursor() override;
  void show_cursor() override;
  bool is_custom_cursor_active() const override;
  bool is_cursor_hidden() const override;
  void display_height(int display_height);
  void update_scaled_cursor(int new_window_width,
                            int new_window_height) override;
  void update_palette(gfx::PaletteHandle palette) override;

  void software_mode(bool enable) override;
  bool software_mode() const { return software_mode_; }

  void set_relative_mouse_mode(bool enable) override;
  bool relative_mouse_mode() const { return relative_mouse_mode_; }

  void window(SDL_Window* window) { window_ = window; }

  void cursor_position(ScreenPoint p) { cursor_pos_ = p; }
  ScreenPoint cursor_position() const { return cursor_pos_; }

  // Returns nullptr if nothing to write (e.g. hardware mode or cursor hidden)
  const gfx::Surface* software_cursor_sprite() const;
  bool software_cursor_centered() const;

private:
  // sync_hardware_cursor() is what turns cursor state into SDL cursor state. In
  // software mode this is a noop (because our renderer draws the cursor).
  void sync_hardware_cursor();
  void hw_free_cursor();
  void hw_rebuild_base_cursor();
  void hw_apply_cursor(const Surface& source_surface, bool centered);
  bool custom_cursor_showing() const;

  gfx::Surface cursor_surface_;
  std::optional<gfx::Surface> custom_cursor_surface_;
  std::optional<gfx::Surface> scaled_custom_surface_;
  SDL_Cursor_unique_ptr active_cursor_;
  UiMode base_mode_{UiMode::Game};
  bool cursor_hidden_{false};
  bool custom_cursor_active_{false};
  bool custom_cursor_suppressed_{false};
  float last_scale_x_{1.0f};
  float last_scale_y_{1.0f};
  bool custom_cursor_centered_{false};
  int display_height_{DisplayConstants::LOGICAL_HEIGHT};
  bool software_mode_{false};
  ScreenPoint cursor_pos_;
  bool relative_mouse_mode_{false};
  // Saved cursor position for restoring after relative mouse mode.
  float relative_mode_origin_x_{0.f};
  float relative_mode_origin_y_{0.f};
  // Window that relative mouse mode is scoped to
  SDL_Window* window_{nullptr};
};
