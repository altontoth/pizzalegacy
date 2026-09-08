/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "engine/Size.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/ColorIndex.h"
#include "engine/gfx/PaletteBank.h"

namespace gfx {

constexpr uint8_t TRANSPARENT = 0;

// Original assets are scaled up by this on load.
constexpr int asset_pixel_scale = 2;

class Surface {
public:
  Surface(Size size, std::optional<PaletteHandle> palette = std::nullopt,
          ColorIndex fill_index = 0);
  Surface(int width, int height,
          std::optional<PaletteHandle> palette = std::nullopt,
          ColorIndex fill_index = 0);

  Surface(Surface&&) noexcept = default;
  Surface& operator=(Surface&&) noexcept = default;
  Surface(const Surface&) = delete;
  Surface& operator=(const Surface&) = delete;

  [[nodiscard]] Surface clone() const;

  [[nodiscard]] static Surface from_indexed(std::span<const uint8_t> src,
                                            int width, int height,
                                            PaletteHandle palette,
                                            int scale = asset_pixel_scale);

  [[nodiscard]] int width() const noexcept { return width_; }
  [[nodiscard]] int height() const noexcept { return height_; }
  [[nodiscard]] PaletteHandle palette() const noexcept {
    return palette_.value_or(PaletteHandle{0});
  }
  void palette(PaletteHandle h) noexcept;

  /* pixel ops (index space) */
  void put(ViewPoint p, ColorIndex idx,
           std::optional<PaletteHandle> pal = std::nullopt);
  void fill_rect(ViewRect r, ColorIndex idx,
                 std::optional<PaletteHandle> pal = std::nullopt);
  void fill(ColorIndex idx, std::optional<PaletteHandle> pal = std::nullopt);
  [[nodiscard]] uint8_t get(ViewPoint p) const;

  // Full-featured: source rect + clipping + transparency
  void blit(const Surface& src, ViewRect src_rect,
            ViewPoint dst = ViewPoint{0, 0}, uint8_t transparent = TRANSPARENT);

  // Draws src_rect as a single-color shape: every non-transparent source
  // pixel becomes `color`, whatever index it was.
  void blit_stencil(const Surface& src, ViewRect src_rect, ViewPoint dst,
                    uint8_t color, uint8_t transparent = TRANSPARENT);

  // Fast path: full-width copy, no transparency, no clipping
  void blit_raw(const Surface& src, ViewPoint dst = ViewPoint{0, 0});

  // Transparency only
  void blit_masked(const Surface& src, ViewPoint dst = ViewPoint{0, 0},
                   uint8_t transparent = TRANSPARENT);

  /* raw access */
  [[nodiscard]] uint8_t* data() noexcept { return pixels_.data(); }
  [[nodiscard]] const uint8_t* data() const noexcept { return pixels_.data(); }
  [[nodiscard]] std::span<const uint8_t> pixels() const noexcept;

  // Replace every pixel whose index is in `keys` with `replacement`
  void replace_colors(std::initializer_list<uint8_t> keys, uint8_t replacement,
                      std::optional<ViewRect> region = std::nullopt) noexcept;

  void colors(const std::vector<Color>&);
  [[nodiscard]] std::vector<Color>& colors();
  [[nodiscard]] const std::vector<Color>& colors() const;
  void clear_custom_palette();
  void clear_palette_state();

  // Disable palette translation for this surface (useful for font atlases,
  // templates)
  void disable_palette_translation() { disable_palette_translation_ = true; }
  [[nodiscard]] bool palette_translation_disabled() const {
    return disable_palette_translation_;
  }

  [[nodiscard]] Size size() const { return Size{width_, height_}; }
  [[nodiscard]] ViewRect rect() const {
    return ViewRect{0, 0, width_, height_};
  }

private:
  // pitch is number of pixels per row in the surface
  int width_{0}, height_{0}, pitch_{0}; // pitch in *pixels*
  std::optional<PaletteHandle>
      palette_; // nullopt = uninitialized, first blit sets it
  std::optional<std::vector<Color>> custom_palette_;

  std::vector<uint8_t> pixels_; // size = width*height

  // Skip palette mapping (for fonts)
  bool disable_palette_translation_{false};

  [[nodiscard]] bool in_bounds(ViewPoint p) const noexcept;
  // Clips src_rect against both surfaces, moving dst to match. False when
  // nothing is left to draw.
  bool clip_blit(const Surface& src, ViewRect& src_rect, ViewPoint& dst) const;
};
} // namespace gfx
