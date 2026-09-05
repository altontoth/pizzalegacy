/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/gfx/Surface.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <numeric>
#include "engine/Fatal.h"
#include "engine/gfx/PaletteBank.h"

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

void Surface::palette(PaletteHandle h) noexcept {
  palette_ = h;
  custom_palette_.reset();
}

/* put a pixel at a specific point */
void Surface::put(ViewPoint p, ColorIndex idx,
                  std::optional<PaletteHandle> pal) {
  (void)pal;
  if (!in_bounds(p)) {
    return;
  }
  pixels_[(p.y * pitch_) + p.x] = idx.get();
}

/* fill, all the pixels will be updated */
void Surface::fill(ColorIndex idx, std::optional<PaletteHandle> pal) {
  (void)pal;
  std::memset(pixels_.data(), idx.get(), height_ * pitch_);
}

/* fill_rect, give it a rect and all the pixels in it will be updated */
void Surface::fill_rect(ViewRect r, ColorIndex idx,
                        std::optional<PaletteHandle> pal) {
  (void)pal;
  if (r.w <= 0 || r.h <= 0) {
    return;
  }

  const int x0 = std::max(0, r.x);
  const int y0 = std::max(0, r.y);
  const int x1 = std::min(width_, r.x + r.w);
  const int y1 = std::min(height_, r.y + r.h);

  for (int y = y0; y < y1; ++y) {
    std::fill_n(&pixels_[y * pitch_ + x0], x1 - x0, idx.get());
  }
}

// blit  (no transparency / colorkey yet: copy all bytes)
void Surface::blit_raw(const Surface& src, ViewPoint dst) {
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
    std::memcpy(dst_row, src_row, static_cast<std::size_t>(copy_w));
  }
}

/* Full blit with support for transparency, source and destination rects */
bool Surface::clip_blit(const Surface& src, ViewRect& s, ViewPoint& d) const {
  // Clip src_rect to src surface
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
    return false;
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
  return s.w > 0 && s.h > 0;
}

void Surface::blit(const Surface& src, ViewRect s, ViewPoint d,
                   uint8_t transparent) {
  if (!clip_blit(src, s, d)) {
    return;
  }

  // Transfer line by line
  for (int row = 0; row < s.h; ++row) {
    const uint8_t* src_row = &src.pixels_[(s.y + row) * src.pitch_ + s.x];
    uint8_t* dst_row = &pixels_[(d.y + row) * pitch_ + d.x];

    for (int col = 0; col < s.w; ++col) {
      uint8_t pix = src_row[col];
      if (pix != transparent) {
        dst_row[col] = pix;
      }
    }
  }
}

void Surface::blit_stencil(const Surface& src, ViewRect s, ViewPoint d,
                           uint8_t color, uint8_t transparent) {
  if (!clip_blit(src, s, d)) {
    return;
  }

  for (int row = 0; row < s.h; ++row) {
    const uint8_t* src_row = &src.pixels_[(s.y + row) * src.pitch_ + s.x];
    uint8_t* dst_row = &pixels_[(d.y + row) * pitch_ + d.x];

    for (int col = 0; col < s.w; ++col) {
      if (src_row[col] != transparent) {
        dst_row[col] = color;
      }
    }
  }
}

/* Blit masked, so respect transparency */
void Surface::blit_masked(const Surface& src, ViewPoint dst,
                          uint8_t transparent) {
  blit(src, {0, 0, src.width_, src.height_}, dst, transparent);
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
    return PaletteBank::instance().colors(palette_.value_or(PaletteHandle{0}));
  }
  return *custom_palette_;
}

std::vector<Color>& Surface::colors() {
  if (!custom_palette_) {
    custom_palette_ =
        PaletteBank::instance().colors(palette_.value_or(PaletteHandle{0}));
  }
  return *custom_palette_;
}

void Surface::colors(const std::vector<Color>& custom_palette) {
  custom_palette_ = custom_palette;
}

void Surface::clear_custom_palette() { custom_palette_.reset(); }

void Surface::clear_palette_state() {
  palette_ = std::nullopt;
  custom_palette_.reset();
}

} // namespace gfx
