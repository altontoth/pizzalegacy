/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>

#include "gfx/Surface.h"

using gfx::PaletteHandle;
using gfx::Surface;

class Vga {
public:
  Vga(std::string filename, std::vector<uint8_t> picture, int width,
      int height);
  ~Vga() = default;

  Vga(Vga&&) noexcept = default;
  Vga& operator=(Vga&&) noexcept = default;

  Vga(const Vga&) = delete;
  Vga& operator=(const Vga&) = delete;

  void render_onto_surface(Surface& target_surface,
                           bool transparent = false) const;
  [[nodiscard]] int width() const;
  [[nodiscard]] int height() const;

private:
  std::string filename_;
  std::vector<uint8_t> picture_;
  int width_;
  int height_;
};
