/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gfx/Surface.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <numeric>
#include "engine/Fatal.h"
#include "gfx/PaletteBank.h"
#include "resources/util/Platform.h"

namespace gfx {

Surface::Surface(Size size, std::optional<PaletteHandle> palette,
                 ColorIndex fill_index)
    : Surface(size.w, size.h, palette, fill_index) {}

Surface::Surface(int width, int height, std::optional<PaletteHandle> palette,
                 ColorIndex fill_index)
    : width_(width), height_(height), pitch_(width), palette_(palette) {

  if (width_ <= 0 || height_ <= 0) {
    fatal("Surface: dimensions must be positive");
  }

  pixels_.resize(width_ * height_, fill_index.get()); // index 0 = transparent
}

Surface Surface::from_indexed(std::span<const uint8_t> src, int w, int h,
                              PaletteHandle pal, int scale) {

  if (src.size() != static_cast<std::size_t>(w * h)) {
    fatal("Surface::from_indexed: size mismatch");
  }

  Surface dst(w * scale, h * scale, pal, /*scale=*/1);

  if (scale == 1) {
    std::memcpy(dst.pixels_.data(), src.data(), src.size());
    return dst;
  }

  // scale == 2 (simple pixel-doubling)
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      uint8_t v = src[y * w + x];

      int dx = x * 2;
      int dy = y * 2;

      dst.pixels_[(dy)*dst.pitch_ + dx] = v;
      dst.pixels_[(dy)*dst.pitch_ + dx + 1] = v;
      dst.pixels_[(dy + 1) * dst.pitch_ + dx] = v;
      dst.pixels_[(dy + 1) * dst.pitch_ + dx + 1] = v;
    }
  }
  return dst;
}

Surface Surface::clone() const {
  Surface copy(width_, height_, palette_.value_or(PaletteHandle{0}));
  std::memcpy(copy.data(), pixels_.data(), pixels_.size());
  return copy;
}

/* Check if the point is within this surface */
inline bool Surface::in_bounds(ViewPoint p) const noexcept {
  return p.x >= 0 && p.x < width_ && p.y >= 0 && p.y < height_;
}

/* Set surface palette - clears multi-palette state */
void Surface::palette(PaletteHandle h) noexcept {
  palette_ = h;
  // Clear multi-palette segments and cached palette when changing primary
  // palette
  palette_segments_.clear();
  custom_palette_.reset();
}

/* translate() - Get or insert palette segment, returns base index */
uint8_t Surface::translate(std::optional<PaletteHandle> pal) {
  // Skip multi-palette logic for DOS assets (256-color palettes)
  if (!platform::is_amiga()) {
    return 0;
  }

  // Determine which palette to use
  PaletteHandle actual_pal;
  if (pal.has_value()) {
    // Explicit palette provided
    actual_pal = *pal;
  }
  else if (palette_.has_value()) {
    // Use surface's primary palette
    actual_pal = *palette_;
  }
  else {
    // Surface has no palette set yet - this shouldn't happen in normal use
    // but we'll handle it gracefully by using palette 0
    actual_pal = PaletteHandle{0};
  }

  // If this is the first time setting a palette on an uninitialized surface,
  // update the primary palette before adding to segments
  if (!palette_.has_value() && palette_segments_.empty()) {
    palette_ = actual_pal;
  }

  // Check if palette already exists in segments
  for (size_t i = 0; i < palette_segments_.size(); ++i) {
    if (palette_segments_[i].id == actual_pal.id) {
      return static_cast<uint8_t>(i * 32);
    }
  }

  // Add new segment if we have space (7 max, index 255 reserved for black)
  if (palette_segments_.size() >= 7) {
    fatal("Surface: Cannot add more than 7 palette segments "
          "(224 colors max, index 255 reserved for black)");
  }

  uint8_t base_index = static_cast<uint8_t>(palette_segments_.size() * 32);
  palette_segments_.push_back(actual_pal);
  return base_index;
}

/* put a pixel at a specific point */
void Surface::put(ViewPoint p, ColorIndex idx,
                  std::optional<PaletteHandle> pal) {
  // Logger::debug("Surface::put");
  if (!in_bounds(p)) {
    return;
  }

  uint8_t pixel_index = idx.get();

  // On Amiga, only remap indices 0-31 (palette segment colors)
  // Indices >= 32 are absolute (UI colors, etc.) and don't get remapped
  if (platform::is_amiga() && pixel_index > 0 && pixel_index < 32) {
    uint8_t base = translate(pal);
    pixels_[(p.y * pitch_) + p.x] = base + pixel_index;
  }
  else {
    // DOS or absolute index: use as-is
    pixels_[(p.y * pitch_) + p.x] = pixel_index;
  }
}

/* fill, all the pixels will be updated */
void Surface::fill(ColorIndex idx, std::optional<PaletteHandle> pal) {
  // Logger::debug("Surface::fill");

  uint8_t pixel_index = idx.get();
  uint8_t final_index = pixel_index;

  // On Amiga, only remap indices 0-31 (palette segment colors)
  // Indices >= 32 are absolute (UI colors, etc.) and don't get remapped
  if (platform::is_amiga() && pixel_index > 0 && pixel_index < 32) {
    uint8_t base = translate(pal);
    final_index = base + pixel_index;
  }

  std::memset(pixels_.data(), final_index, height_ * pitch_);
}

/* fill_rect, give it a rect and all the pixels in it will be updated */
void Surface::fill_rect(ViewRect r, ColorIndex idx,
                        std::optional<PaletteHandle> pal) {
  // Logger::debug("Surface::fill_rect");
  if (r.w <= 0 || r.h <= 0) {
    return;
  }

  const int x0 = std::max(0, r.x);
  const int y0 = std::max(0, r.y);
  const int x1 = std::min(width_, r.x + r.w);
  const int y1 = std::min(height_, r.y + r.h);

  uint8_t pixel_index = idx.get();
  uint8_t final_index = pixel_index;

  // On Amiga, only remap indices 0-31 (palette segment colors)
  // Indices >= 32 are absolute (UI colors, etc.) and don't get remapped
  if (platform::is_amiga() && pixel_index > 0 && pixel_index < 32) {
    uint8_t base = translate(pal);
    final_index = base + pixel_index;
  }

  for (int y = y0; y < y1; ++y) {
    std::fill_n(&pixels_[y * pitch_ + x0], x1 - x0, final_index);
  }
}

// blit  (no transparency / colorkey yet: copy all bytes)
void Surface::blit_raw(const Surface& src, ViewPoint dst) {
#ifdef DEBUG
  bool has_transparency = std::any_of(pixels_.begin(), pixels_.end(),
                                      [](uint8_t c) { return c == 0; });
  if (has_transparency) {
    // Logger::warn("Calling blit_raw with a surface that has 0!");
  }
#endif
  // Logger::debug("Surface::blit_raw");

  // For multi-palette sources, add all palette segments to destination
  if (platform::is_amiga() && !src.disable_palette_translation_ &&
      !src.palette_segments_.empty()) {
    for (const auto& segment : src.palette_segments_) {
      translate(segment);
    }
  }

  // Get base index for source palette (Amiga multi-palette support)
  // Skip translation if source has it disabled (e.g., font atlases)
  uint8_t base = src.disable_palette_translation_ ? 0 : translate(src.palette_);

  for (int sy = 0; sy < src.height_; ++sy) {
    const int dy = dst.y + sy;
    if (dy < 0 || dy >= height_) {
      continue;
    }

    int copy_w = src.width_;
    int sx = 0;
    int dx = dst.x;

    if (dx < 0) {
      sx -= dx;
      copy_w += dx;
      dx = 0;
    }
    if (dx + copy_w > width_) {
      copy_w = width_ - dx;
    }
    if (copy_w <= 0) {
      continue;
    }

    const uint8_t* src_row = &src.pixels_[(sy * src.pitch_) + sx];
    uint8_t* dst_row = &pixels_[(dy * pitch_) + dx];

    // If not Amiga or base is 0, use fast memcpy
    if (!platform::is_amiga() || base == 0) {
      std::memcpy(dst_row, src_row, static_cast<std::size_t>(copy_w));
    }
    else {
      // Remap each pixel to the correct palette segment
      // Only remap indices 0-31 (palette colors); >= 32 are absolute
      for (int i = 0; i < copy_w; ++i) {
        uint8_t src_pixel = src_row[i];
        if (src_pixel > 0 && src_pixel < 32) {
          dst_row[i] = base + src_pixel;
        }
        else {
          dst_row[i] = src_pixel; // transparent (0) or absolute index (>=32)
        }
      }
    }
  }
}

/* Full blit with support for transparency, source and destination rects */
void Surface::blit(const Surface& src, ViewRect s, ViewPoint d,
                   uint8_t transparent) {
  // Logger::debug("Surface::blit");
  //  Clip src_rect to src surface
  if (s.x < 0) {
    d.x -= s.x;
    s.w += s.x;
    s.x = 0;
  }
  if (s.y < 0) {
    d.y -= s.y;
    s.h += s.y;
    s.y = 0;
  }
  if (s.x + s.w > src.width_) {
    s.w = src.width_ - s.x;
  }
  if (s.y + s.h > src.height_) {
    s.h = src.height_ - s.y;
  }
  if (s.w <= 0 || s.h <= 0) {
    return;
  }

  // Clip against *this* surface
  if (d.x < 0) {
    s.x -= d.x;
    s.w += d.x;
    d.x = 0;
  }
  if (d.y < 0) {
    s.y -= d.y;
    s.h += d.y;
    d.y = 0;
  }
  if (d.x + s.w > width_) {
    s.w = width_ - d.x;
  }
  if (d.y + s.h > height_) {
    s.h = height_ - d.y;
  }
  if (s.w <= 0 || s.h <= 0) {
    return;
  }

  // Get base index for source palette (Amiga multi-palette support)
  // Skip translation if source has it disabled (e.g., font atlases)
  uint8_t base = src.disable_palette_translation_ ? 0 : translate(src.palette_);

  // Transfer line by line
  for (int row = 0; row < s.h; ++row) {
    const uint8_t* src_row = &src.pixels_[(s.y + row) * src.pitch_ + s.x];
    uint8_t* dst_row = &pixels_[(d.y + row) * pitch_ + d.x];

    for (int col = 0; col < s.w; ++col) {
      uint8_t pix = src_row[col];
      if (pix != transparent) {
        // On Amiga, only remap indices 1-31 (palette colors); 0 and >=32 are
        // absolute
        if (platform::is_amiga() && pix > 0 && pix < 32) {
          dst_row[col] = base + pix;
        }
        else {
          dst_row[col] = pix; // DOS, transparent (0), or absolute index (>=32)
        }
      }
    }
  }
}

/* Blit masked, so respect transparency */
void Surface::blit_masked(const Surface& src, ViewPoint dst,
                          uint8_t transparent) {
  // Logger::debug("Surface::blit_masked");

  // Get base index for source palette (Amiga multi-palette support)
  // Skip translation if source has it disabled (e.g., font atlases)
  uint8_t base = src.disable_palette_translation_ ? 0 : translate(src.palette_);

  for (int sy = 0; sy < src.height_; ++sy) {
    int dy = dst.y + sy;
    if (dy < 0 || dy >= height_) {
      continue;
    }

    for (int sx = 0; sx < src.width_; ++sx) {
      int dx = dst.x + sx;
      if (dx < 0 || dx >= width_) {
        continue;
      }

      uint8_t pixel = src.pixels_[sy * src.pitch_ + sx];
      if (pixel != transparent) {
        // On Amiga, only remap indices 1-31 (palette colors); 0 and >=32 are
        // absolute
        if (platform::is_amiga() && pixel > 0 && pixel < 32) {
          pixels_[dy * pitch_ + dx] = base + pixel;
        }
        else {
          pixels_[dy * pitch_ + dx] =
              pixel; // DOS, transparent (0), or absolute index (>=32)
        }
      }
    }
  }
}

uint8_t Surface::get(ViewPoint p) const {
  assert(in_bounds(p) && "Surface::get out-of-bounds");
  return pixels_[p.y * pitch_ + p.x];
}

void Surface::replace_colors(std::initializer_list<uint8_t> keys,
                             uint8_t replacement,
                             std::optional<ViewRect> region) noexcept {
  // Let's agree you don't ask to replace the existing color, but only
  // if it's the first color.
  if (*keys.begin() == replacement) {
    return;
  }
  std::array<uint8_t, 256> map;
  std::iota(map.begin(), map.end(), 0);
  for (uint8_t k : keys) {
    map[k] = replacement;
  }

  // Clamp region to the surface if needed, otherwise just do whole surface
  int x0 = 0;
  int y0 = 0;
  int x1 = width_;
  int y1 = height_;
  if (region) {
    x0 = std::clamp(region->x, 0, width_);
    y0 = std::clamp(region->y, 0, height_);
    x1 = std::clamp(region->x + region->w, 0, width_);
    y1 = std::clamp(region->y + region->h, 0, height_);
  }

  for (int y = y0; y < y1; ++y) {
    uint8_t* row = pixels_.data() + (y * pitch_) + x0;
    for (int x = x0; x < x1; ++x) {
      row[x - x0] = map[row[x - x0]];
    }
  }
}

std::span<const uint8_t> Surface::pixels() const noexcept {
  return {pixels_.data(), pixels_.size()};
}

const std::vector<Color>& Surface::colors() const {
  if (!custom_palette_) {
    // If we have multiple palette segments (Amiga mode), build combined palette
    if (platform::is_amiga() && !palette_segments_.empty()) {
      // Need to cast away const to cache the combined palette
      auto* mutable_this = const_cast<Surface*>(this);
      mutable_this->custom_palette_ = std::vector<Color>(256);

      // Fill each 32-color segment
      for (size_t i = 0; i < palette_segments_.size(); ++i) {
        const auto& segment_colors =
            PaletteBank::instance().colors(palette_segments_[i]);
        // Copy first 32 colors of this palette to the appropriate segment
        for (size_t c = 0; c < 32 && c < segment_colors.size(); ++c) {
          (*mutable_this->custom_palette_)[(i * 32) + c] = segment_colors[c];
        }
      }
      // Index 255 is always opaque black for Amiga backgrounds
      // This is a hack because our surfaces don't start black. Not sure
      // how they start even, can't be bothered to figure that out now.
      (*mutable_this->custom_palette_)[255] = Color{0, 0, 0};
      return *mutable_this->custom_palette_;
    }

    return PaletteBank::instance().colors(palette_.value_or(PaletteHandle{0}));
  }
  return *custom_palette_;
}

std::vector<Color>& Surface::colors() {
  if (!custom_palette_) {
    // If we have multiple palette segments (Amiga mode), build combined palette
    if (platform::is_amiga() && !palette_segments_.empty()) {
      custom_palette_ = std::vector<Color>(256);

      // Fill each 32-color segment
      for (size_t i = 0; i < palette_segments_.size(); ++i) {
        const auto& segment_colors =
            PaletteBank::instance().colors(palette_segments_[i]);
        // Copy first 32 colors of this palette to the appropriate segment
        for (size_t c = 0; c < 32 && c < segment_colors.size(); ++c) {
          (*custom_palette_)[(i * 32) + c] = segment_colors[c];
        }
      }
      // Index 255 is always opaque black for Amiga backgrounds
      (*custom_palette_)[255] = Color{0, 0, 0};
      return *custom_palette_;
    }

    // Create custom palette based on current palette handle
    custom_palette_ =
        PaletteBank::instance().colors(palette_.value_or(PaletteHandle{0}));
  }
  return *custom_palette_;
}

void Surface::colors(const std::vector<Color>& custom_palette) {
  custom_palette_ = custom_palette;
}

void Surface::clear_custom_palette() { custom_palette_.reset(); }

void Surface::clear_palette_segments() {
  palette_ = std::nullopt;
  palette_segments_.clear();
  custom_palette_.reset();
}

} // namespace gfx
