/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>

#include "engine/Geometry.h"
#include "resources/VgaManager.h"

template <typename IconEnum> class Blitmap {
public:
  Blitmap(VgaManager* vga_manager, const std::string& vga_path,
          const std::string& offsets_path);

  ViewRect rect(IconEnum icon) const;
  void render_icon(IconEnum icon, Surface& dest, ViewPoint p) const;
  void render_icon_aligned(IconEnum icon, Surface& dest, HAlign h_align,
                           VAlign v_align,
                           std::optional<ViewRect> bounds = std::nullopt,
                           ViewPoint offset = {0, 0}) const;

  Surface icon_surface(IconEnum icon) const;
  [[nodiscard]] gfx::PaletteHandle palette() const { return {0}; }

private:
  [[nodiscard]] ViewRect rect_internal(int index) const;
  void render_icon_internal(int index, Surface& dest, ViewPoint p) const;
  [[nodiscard]] Surface icon_surface(int index) const;

  VgaManager* vga_manager_;
  Surface surface_;
  std::vector<ViewRect> rects_;
};

#include "engine/geom/Spaces.h"
#include "resources/Blitmap.inl"
