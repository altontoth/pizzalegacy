/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include "engine/DisplayConstants.h"
#include "engine/SDL_include.h"
#include "engine/ui/ICursorManager.h"
#include "resources/VgaManager.h"

class CursorManager : public ICursorManager {
public:
  CursorManager(VgaManager* vga,
                int display_height = DisplayConstants::LOGICAL_HEIGHT);

  void base_cursor_mode(CursorMode mode);
  CursorMode base_cursor_mode() const;
  void custom_cursor(const Surface& surface, bool centered = true);
  void clear_custom_cursor();
  void hide_cursor();
  void show_cursor();
  bool is_custom_cursor_active() const;
  bool is_cursor_hidden() const;
  void update_scaled_cursor(int new_window_width, int new_window_height);
  void update_palette(gfx::PaletteHandle palette);

private:
  void free_active_cursor();
  void rebuild_base_cursor();
  void apply_cursor(const Surface& source_surface, bool centered);

  gfx::Surface cursor_surface_;
  std::optional<gfx::Surface> custom_cursor_surface_;
  std::optional<gfx::Surface> scaled_custom_surface_;
  SDL_Cursor_unique_ptr active_cursor_;
  CursorMode base_mode_{CursorMode::GAME};
  bool cursor_hidden_{false};
  bool custom_cursor_active_{false};
  float last_scale_x_{1.0f};
  float last_scale_y_{1.0f};
  bool custom_cursor_centered_{false};
};
