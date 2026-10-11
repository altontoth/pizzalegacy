/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/Vga.h"

#include "resources/AssetMetadata.h"
#include "resources/Upscaling.h"
#include "resources/VgaLoader.h"

#include <cassert>
#include <iostream>

Vga::Vga(std::string filename, std::vector<uint8_t> picture, int width,
         int height)
    : filename_(filename), picture_(std::move(picture)), width_(width),
      height_(height) {}

void Vga::render_onto_surface(Surface& target_surface, bool transparent) const {
  // Get palette from asset metadata
  auto palette_opt = resources::AssetMetadataRegistry::get_palette(filename_);
  uint8_t palette_index = palette_opt.value_or(0);

  if (!palette_opt.has_value()) {
    std::cerr << "Warning: No palette found for '" << filename_
              << "', defaulting to palette 0." << std::endl;
  }

  PaletteHandle pal{palette_index};
  Surface raw = Surface::from_indexed(picture_, width_, height_, pal,
                                      gfx::asset_pixel_scale,
                                      resources::upscaling().pictures);
  target_surface.blit_raw(raw);
}

int Vga::width() const { return width_; }

int Vga::height() const { return height_; }
